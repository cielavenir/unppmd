#define DELTA 0

#include "../compat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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

#pragma pack(push,1)
typedef struct{
	unsigned int len;
	unsigned char FNLen;
} ARC_INFO;
#pragma pack(pop)

#ifdef STANDALONE
int main(const int argc, const char **argv){
	initstdio();
#else
int littlearc(const int argc, const char **argv){
#endif
	{
		int i=0;
		for(;i<DELTA;i++)buf[i]=0;
	}

	ARC_INFO ai;
	int mode=argc<2; //lol
	if(mode&&!isatty(fileno(stdin))){
		char fnbuf[512];
		FILE *fin=stdin;
		for(;;){
			if(fread(&ai,1,sizeof(ai),fin)<sizeof(ai)){
				break;
			}
			fread(fnbuf,1,ai.FNLen,fin);
			fnbuf[ai.FNLen]=0;
			char *fname=fnbuf+(fnbuf[0]=='/');
			makedir(fname);
			fprintf(stderr,"Extracting %s: ",fname);
			FILE *fout=fopen(fname,"wb");
			if(!fout){
				fprintf(stderr,"failed to open.\n");
				//return 1;
			}
			//process anyway.
			for(;ai.len;){
				unsigned int len=min(BUFLEN-DELTA,ai.len);
				unsigned int readlen=fread(buf+DELTA,1,len,fin);
				{
					unsigned int k=0;
					if(DELTA)for(;k<readlen;k++){
						buf[k+DELTA]=buf[k]-buf[k+DELTA];
					}
					if(fout)fwrite(buf+DELTA,1,readlen,fout);
					for(k=0;k<DELTA;k++){
						buf[k]=buf[k+readlen];
					}
				}
				ai.len-=len;
			}
			if(fout){
				fclose(fout);
				fprintf(stderr,"OK.\n");
			}
		}
		return 0;
	}else if(!isatty(fileno(stdout))){
		FILE *fout=stdout;
		int i=1;
		for(;i<argc;i++){
			fprintf(stderr,"Adding %s: ",argv[i]);
			FILE *fin=fopen(argv[i],"rb");
			if(!fin){
				fprintf(stderr,"not found.\n");
				continue;
			}
			int len=strlen(argv[i]);
			ai.FNLen=len-(argv[i][0]=='/');
			{
				struct stat st;
				fstat(fileno(fin),&st);
				ai.len=st.st_size;
			}
			fwrite(&ai,1,sizeof(ai),fout);
			fwrite(argv[i]+(argv[i][0]=='/'),1,len-(argv[i][0]=='/'),fout);
			for(;ai.len;){
				unsigned int len=min(BUFLEN-DELTA,ai.len);
				unsigned int readlen=fread(buf+DELTA,1,len,fin);
				{
					unsigned int k=0;
					if(DELTA)for(;k<readlen;k++){
						buf[k]-=buf[k+DELTA];
					}
					fwrite(buf,1,readlen,fout);
					for(k=0;k<DELTA;k++){
						buf[k]=buf[k+readlen];
					}
				}
				ai.len-=len;
			}
			fclose(fin);
			fprintf(stderr,"OK.\n");
		}
		return 0;
	}
	fprintf(stderr,"littlearc files... > arc , littlearc < arc\n");
	return 1;
}
