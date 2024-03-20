// unppmd.c: PPMd H/I2 encoder/decoder with solid support
// [note] 7z cannot handle PPMd archive with multiple files.

#define PPMD8_FREEZE_SUPPORT
//#define PPMD7_RANGE7Z // should not be used

// experimental purpose; 20 should be used.
// changing FACTOR will make incompatible archive.
// 10U==KB, 20U==MB
#define FACTOR 20U

#include "../compat.h"
#include "../lib/popt/popt.h"
#include "../lib/lzma/C/Alloc.h"
#include "../lib/lzma/C/Ppmd7.h"
#include "../lib/lzma/C/Ppmd8.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef STANDALONE
unsigned char buf[BUFLEN];
int makedir(const char *_dir){
	if(!_dir||!*_dir)return -1;
	char *dir=(char*)_dir; //unsafe, but OK
	int l=strlen(dir),i=0,ret=0;
	for(;i<l;i++){
		int c=dir[i];
		if(c=='\\'||c=='/'){
			dir[i]=0;
#if defined(_WIN32) || (!defined(__GNUC__) && !defined(__clang__))
			ret=mkdir(dir);
#else
			ret=mkdir(dir,0755);
#endif
			dir[i]=c;
		}
	}
	return ret;
}
#else
#include "../lib/xutil.h"
#endif

#define CTX7(ref) ((CPpmd7_Context *)Ppmd7_GetContext(p, ref))
#define STATS7(ctx) Ppmd7_GetStats(p, ctx)
#define SUFFIX7(ctx) CTX7((ctx)->Suffix)
static void Ppmd7_InitSolid(CPpmd7 *p){
	p->OrderFall = p->MaxOrder;
	p->MinContext = p->MaxContext;
	CPpmd7_Context *ctx = p->MinContext;
	for(;ctx->Suffix;ctx=SUFFIX7(ctx))p->OrderFall--;
	p->FoundState = STATS7(ctx);
	p->MinContext = p->MaxContext;
}
#define CTX8(ref) ((CPpmd8_Context *)Ppmd8_GetContext(p, ref))
#define STATS8(ctx) Ppmd8_GetStats(p, ctx)
#define SUFFIX8(ctx) CTX8((ctx)->Suffix)
static void Ppmd8_InitSolid(CPpmd8 *p){
	p->OrderFall = p->MaxOrder;
	p->MinContext = p->MaxContext;
	CPpmd8_Context *ctx = p->MinContext;
	for(;ctx->Suffix;ctx=SUFFIX8(ctx))p->OrderFall--;
	p->FoundState = STATS8(ctx);
	p->MinContext = p->MaxContext;
}

const unsigned int PPMdSignature=0x84ACAF8F;
typedef struct{
	unsigned int signature,attrib;
	unsigned short info,FNLen,time,date;
} ARC_INFO;

/// stream interface ///
// I really think this is abuse of pointer, but I also think this is a limitation of C.
// Sreader={{reader},fin}
typedef struct{
	IByteIn p;
	FILE *f;
} Treader;
static Byte reader(const IByteIn *p){
	Treader *r=(Treader*)p;
	int c=fgetc(r->f);
	if(c<0)return 0;
	return c;
}
// Swriter={{writer},fout}
typedef struct{
	IByteOut p;
	FILE *f;
} Twriter;
static void writer(const IByteOut *p, Byte c){
	Twriter *r=(Twriter*)p;
	fputc(c,r->f);
}

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int unppmd(const int argc, const char **argv){
#endif
	int cmode=0,mode=0;
	int sasize=32,order=6,solid=0,cutoff=0,variant=8;
	poptContext optCon;
	int optc;

	struct poptOption optionsTable[] = {
		//{ "longname", "shortname", argInfo,      *arg,       int val, description, argment description}
		{ "encode", 'e',         POPT_ARG_NONE,            &cmode,      0,       "encode", NULL },
		{ "decode", 'd',         POPT_ARG_NONE,            &mode,      0,       "decode", NULL },
		{ "memory",     'm',         POPT_ARG_INT, &sasize,    'm',       "1-256 MB (32)", "memory" },
		{ "order",     'o',         POPT_ARG_INT, &order,    'o',       "2-16 (6)", "order" },
		{ "solid",     's',         POPT_ARG_INT, &solid,    's',       "0-1 (0)", "solid" },
		{ "cutoff",     'r',         POPT_ARG_INT, &cutoff,    'r',       "0-2 (0)", "cutoff" },
		{ "variant",     'v',         POPT_ARG_INT, &variant,    'r',       "7-8 (8)", "variant" },
		POPT_AUTOHELP,
		POPT_TABLEEND,
	};
	optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "{-e files... >arc.pmd} or {-d <arc.pmd}");

	for(;(optc=poptGetNextOpt(optCon))>=0;){}

	if(sasize<1)sasize=1;
	if(sasize>256)sasize=256;
	if(order<2)order=2;
	if(order>16)order=16;
	if(solid<0)solid=0;
	if(solid>1)solid=1;
	if(cutoff<0)cutoff=0;
	if(cutoff>2)cutoff=2;

	if(cmode+mode!=1 || variant<7 || 8<variant){
		poptPrintHelp(optCon, stderr, 0);
		poptFreeContext(optCon);
		return 1;
	}

	if(mode){
		ARC_INFO ai;
		memset(&ai,0,sizeof(ai));
		if(isatty(fileno(stdin))/*&&argc<2*/)
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *fin=stdin;
		poptFreeContext(optCon);

		CPpmd7 ppmd7;
		CPpmd8 ppmd8;
		memset(&ppmd7,0,sizeof(ppmd7));
		memset(&ppmd8,0,sizeof(ppmd8));
		Treader Sreader={{reader},fin};
		ppmd8.Stream.In=&Sreader.p;
		Ppmd7_Construct(&ppmd7);
		Ppmd8_Construct(&ppmd8);
		for(;;){
			if(fread(&ai,1,sizeof(ai),fin)<sizeof(ai)){
				break;
			}
			int cutoff=ai.FNLen>>14;
			int order=(ai.info&0x0F)+1;
			unsigned int sasize=((ai.info>>4)&0xFF)+1;
			unsigned char variant=(ai.info >> 12)+'A';
			fread(cbuf,ai.FNLen,1,fin);
			cbuf[ai.FNLen]=0;
			char *fname=cbuf+(cbuf[0]=='/');
			fprintf(stderr,"Decompressing %s: ",fname);
			if(ai.signature != PPMdSignature){
				fprintf(stderr,"not in PPMd format. cannot continue.\n");
				return 1;
			}
			if(variant!='H' && variant!='I'){
				fprintf(stderr,"saved in variant %c. cannot continue.\n",variant);
				return 1;
			}
			makedir(fname);
			FILE *fout=fopen(fname,"wb");
			if(!fout){
				fprintf(stderr,"failed to open. cannot continue.\n");
				return 1;
			}
			if(variant=='I'){
				Ppmd8_Alloc(&ppmd8,((unsigned int)sasize)<<FACTOR,&g_Alloc);
				Ppmd8_Init_RangeDec(&ppmd8);
				if(order<2){
					Ppmd8_InitSolid(&ppmd8);
				}else{
					Ppmd8_Init(&ppmd8,order,cutoff);
				}
				for(;;){
					int c=Ppmd8_DecodeSymbol(&ppmd8);
					if(c<0){
						if(c==-1)break;
						fprintf(stderr,"corrupted. cannot continue.\n");
						fclose(fout);
						return 1;
					}
					fputc(c,fout);
				}
			}else if(variant=='H'){
				CPpmd7_RangeDec *rc = &ppmd7.rc.dec;
				rc->Stream=&Sreader.p;
#ifdef PPMD7_RANGE7Z
				Ppmd7z_RangeDec_Init(rc);
#else
				Ppmd7a_RangeDec_Init(rc);
#endif
				Ppmd7_Alloc(&ppmd7,((unsigned int)sasize)<<FACTOR,&g_Alloc);
				if(order<2){
					Ppmd7_InitSolid(&ppmd7);
				}else{
					Ppmd7_Init(&ppmd7,order);
				}
				for(;;){
#ifdef PPMD7_RANGE7Z
					int c=Ppmd7z_DecodeSymbol(&ppmd7);
#else
					int c=Ppmd7a_DecodeSymbol(&ppmd7);
#endif
					if(c<0){
						if(c==-1)break;
						fprintf(stderr,"corrupted. cannot continue.\n");
						fclose(fout);
						return 1;
					}
					fputc(c,fout);
				}
			}
			fclose(fout);
			fprintf(stderr,"OK.\n");
		}
		Ppmd7_Free(&ppmd7,&g_Alloc);
		Ppmd8_Free(&ppmd8,&g_Alloc);
		return 0;
	}else{
		ARC_INFO ai;
		memset(&ai,0,sizeof(ai));
		if(isatty(fileno(stdout))/*&&argc<3*/)
			{poptPrintHelp(optCon, stderr, 0);poptFreeContext(optCon);return -1;}
		FILE *fout=stdout;
		const char **_argv=poptGetArgs(optCon);
		CPpmd7 ppmd7;
		CPpmd8 ppmd8;
		memset(&ppmd7,0,sizeof(ppmd7));
		memset(&ppmd8,0,sizeof(ppmd8));
		Twriter Swriter={{writer},fout};
		ppmd8.Stream.Out=&Swriter.p;
		Ppmd7_Construct(&ppmd7);
		Ppmd8_Construct(&ppmd8);

		for(;*_argv;_argv++){
			fprintf(stderr,"Compressing %s: ",*_argv);
			FILE *fin=fopen(*_argv,"rb");
			if(!fin){
				fprintf(stderr,"not found.\n");
				continue;
			}
			{
				struct stat s;
				memset(&s,0,sizeof(struct stat));
				fstat(fileno(fin),&s);
				if(!S_ISREG(s.st_mode)){
					fclose(fin);
					fprintf(stderr,"not normal file.\n");
					continue;
				}
			}
			ai.signature=PPMdSignature;
			int len=strlen(*_argv);
			ai.FNLen=len-(*_argv[0]=='/')+(cutoff<<14);
			ai.info=(order-1) | ((sasize-1) << 4) | (variant << 12);
			fwrite(&ai,1,sizeof(ai),fout);
			fwrite(*_argv+(*_argv[0]=='/'),1,len-(*_argv[0]=='/'),fout);

			if(variant==8){
				Ppmd8_Alloc(&ppmd8,((unsigned int)sasize)<<FACTOR,&g_Alloc);
				Ppmd8_Init_RangeEnc(&ppmd8);
				if(order<2){
					Ppmd8_InitSolid(&ppmd8);
				}else{
					Ppmd8_Init(&ppmd8,order,cutoff);
				}
				int c;
				for(;~(c=fgetc(fin));)Ppmd8_EncodeSymbol(&ppmd8,c);
				Ppmd8_EncodeSymbol(&ppmd8,-1);
				Ppmd8_Flush_RangeEnc(&ppmd8);
			}else if(variant==7){
				int c;
				Ppmd7_Alloc(&ppmd7,((unsigned int)sasize)<<FACTOR,&g_Alloc);
				if(order<2){
					Ppmd7_InitSolid(&ppmd7);
				}else{
					Ppmd7_Init(&ppmd7,order);
				}
				CPpmd7z_RangeEnc *rc = &ppmd7.rc.enc;
				rc->Stream=&Swriter.p;
#ifdef PPMD7_RANGE7Z
				Ppmd7z_Init_RangeEnc(&ppmd7);
				for(;~(c=fgetc(fin));)Ppmd7z_EncodeSymbol(&ppmd7,c);
				Ppmd7z_EncodeSymbol(&ppmd7,-1);
				Ppmd7z_Flush_RangeEnc(&ppmd7);
#else
				Ppmd7a_Init_RangeEnc(rc);
				for(;~(c=fgetc(fin));)Ppmd7a_EncodeSymbol(&ppmd7,c);
				Ppmd7a_EncodeSymbol(&ppmd7,-1);
				Ppmd7a_Flush_RangeEnc(&ppmd7);
#endif
			}
			fclose(fin);
			fprintf(stderr,"OK.\n");
			if(solid)order=1;
		}
		Ppmd7_Free(&ppmd7,&g_Alloc);
		Ppmd8_Free(&ppmd8,&g_Alloc);
		poptFreeContext(optCon);
		return 0;
	}
}
