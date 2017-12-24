#define PPMD8_FREEZE_SUPPORT
#include "../lib/lzma/C/Ppmd8.c"
#include "../lib/lzma/C/Ppmd8Enc.c"
#include "../lib/lzma/C/Ppmd8Dec.c"

#include "../lib/lzma/C/Ppmd7.h"
typedef struct{
	IPpmd7_RangeDec vt;
	CPpmd8 ppmd8;
} TRangeDecoderPpmd8;
static UInt32 R8_Range_DecodeBit(const IPpmd7_RangeDec *pp, UInt32 size0){
	TRangeDecoderPpmd8 *p = (TRangeDecoderPpmd8*)pp;
	if (p->ppmd8.Code / (p->ppmd8.Range >>= 14) < size0)
	{
		p->vt.Decode(&p->vt, 0, size0);
		return 0;
	}
	else
	{
		p->vt.Decode(&p->vt, size0, (1 << 14) - size0);
		return 1;
	}
}
static UInt32 R8_Range_GetThreshold(const IPpmd7_RangeDec *pp, UInt32 total){
	TRangeDecoderPpmd8 *p = (TRangeDecoderPpmd8*)pp;
	return RangeDec_GetThreshold(&p->ppmd8,total);
}
static void R8_Range_Decode(const IPpmd7_RangeDec *pp, UInt32 start, UInt32 size){
	TRangeDecoderPpmd8 *p = (TRangeDecoderPpmd8*)pp;
	return RangeDec_Decode(&p->ppmd8,start,size);
}
void R8_RangeDec_CreateVTable(TRangeDecoderPpmd8 *p){
	p->vt.GetThreshold = R8_Range_GetThreshold;
	p->vt.Decode = R8_Range_Decode;
	p->vt.DecodeBit = R8_Range_DecodeBit;
}

#define MASK(sym) ((signed char *)charMask)[sym]

void R8_Ppmd7_EncodeSymbol(CPpmd7 *p, CPpmd8 *rc, int symbol)
{
  size_t charMask[256 / sizeof(size_t)];
  if (p->MinContext->NumStats != 1)
  {
    CPpmd_State *s = Ppmd7_GetStats(p, p->MinContext);
    UInt32 sum;
    unsigned i;
    if (s->Symbol == symbol)
    {
      RangeEnc_Encode(rc, 0, s->Freq, p->MinContext->SummFreq);
      p->FoundState = s;
      Ppmd7_Update1_0(p);
      return;
    }
    p->PrevSuccess = 0;
    sum = s->Freq;
    i = p->MinContext->NumStats - 1;
    do
    {
      if ((++s)->Symbol == symbol)
      {
        RangeEnc_Encode(rc, sum, s->Freq, p->MinContext->SummFreq);
        p->FoundState = s;
        Ppmd7_Update1(p);
        return;
      }
      sum += s->Freq;
    }
    while (--i);
    
    p->HiBitsFlag = p->HB2Flag[p->FoundState->Symbol];
    PPMD_SetAllBitsIn256Bytes(charMask);
    MASK(s->Symbol) = 0;
    i = p->MinContext->NumStats - 1;
    do { MASK((--s)->Symbol) = 0; } while (--i);
    RangeEnc_Encode(rc, sum, p->MinContext->SummFreq - sum, p->MinContext->SummFreq);
  }
  else
  {
    UInt16 *prob = Ppmd7_GetBinSumm(p);
    CPpmd_State *s = Ppmd7Context_OneState(p->MinContext);
    if (s->Symbol == symbol)
    {
      RangeEnc_EncodeBit_0(rc, *prob);
      *prob = (UInt16)PPMD_UPDATE_PROB_0(*prob);
      p->FoundState = s;
      Ppmd7_UpdateBin(p);
      return;
    }
    else
    {
      RangeEnc_EncodeBit_1(rc, *prob);
      *prob = (UInt16)PPMD_UPDATE_PROB_1(*prob);
      p->InitEsc = PPMD7_kExpEscape[*prob >> 10];
      PPMD_SetAllBitsIn256Bytes(charMask);
      MASK(s->Symbol) = 0;
      p->PrevSuccess = 0;
    }
  }
  for (;;)
  {
    UInt32 escFreq;
    CPpmd_See *see;
    CPpmd_State *s;
    UInt32 sum;
    unsigned i, numMasked = p->MinContext->NumStats;
    do
    {
      p->OrderFall++;
      if (!p->MinContext->Suffix)
        return; /* EndMarker (symbol = -1) */
      p->MinContext = Ppmd7_GetContext(p, p->MinContext->Suffix);
    }
    while (p->MinContext->NumStats == numMasked);
    
    see = Ppmd7_MakeEscFreq(p, numMasked, &escFreq);
    s = Ppmd7_GetStats(p, p->MinContext);
    sum = 0;
    i = p->MinContext->NumStats;
    do
    {
      int cur = s->Symbol;
      if (cur == symbol)
      {
        UInt32 low = sum;
        CPpmd_State *s1 = s;
        do
        {
          sum += (s->Freq & (int)(MASK(s->Symbol)));
          s++;
        }
        while (--i);
        RangeEnc_Encode(rc, low, s1->Freq, sum + escFreq);
        Ppmd_See_Update(see);
        p->FoundState = s1;
        Ppmd7_Update2(p);
        return;
      }
      sum += (s->Freq & (int)(MASK(cur)));
      MASK(cur) = 0;
      s++;
    }
    while (--i);
    
    RangeEnc_Encode(rc, sum, escFreq, sum + escFreq);
    see->Summ = (UInt16)(see->Summ + sum + escFreq);
  }
}
