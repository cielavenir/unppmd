/* Ppmd7Enc.c -- Ppmd7z (PPMdH with 7z Range Coder) Encoder
2021-04-13 : Igor Pavlov : Public domain
This code is based on:
  PPMd var.H (2001): Dmitry Shkarin : Public domain */

/* Ported to Ppmd7a by @cielavenir */

#include "Precomp.h"

#include "Ppmd7.h"

#define kTop (1 << 24)
#define kBot (1 << 15)

#define R (&p->rc.enc)

#define WRITE_BYTE(p) IByteOut_Write(p->Stream, (Byte)(p->Low >> 24))

void Ppmd7a_Flush_RangeEnc(CPpmd7 *p)
{
  unsigned i;
  for (i = 0; i < 4; i++, R->Low <<= 8 )
    WRITE_BYTE(R);
}

#define RC_NORM(p) \
while ((R->Low ^ (R->Low + R->Range)) < kTop \
  || (R->Range < kBot && ((R->Range = (0 - R->Low) & (kBot - 1)), 1))) \
  { WRITE_BYTE(R); R->Range <<= 8; R->Low <<= 8; }

// we must use only one type of Normalization from two: LOCAL or REMOTE
#define RC_NORM_LOCAL(p)    // RC_NORM(p)
#define RC_NORM_REMOTE(p)   RC_NORM(p)

MY_FORCE_INLINE
// MY_NO_INLINE
static void RangeEnc_Encode(CPpmd7 *p, UInt32 start, UInt32 size, UInt32 total)
{
  R->Low += start * (R->Range /= total);
  R->Range *= size;
  RC_NORM_LOCAL(R);
}

#define RC_Encode(start, size, total) RangeEnc_Encode(p, start, size, total);
#define RC_EncodeFinal(start, size, total) RC_Encode(start, size, total); RC_NORM_REMOTE(p);

#define CTX(ref) ((CPpmd7_Context *)Ppmd7_GetContext(p, ref))
#define SUFFIX(ctx) CTX((ctx)->Suffix)
typedef CPpmd7_Context * CTX_PTR;
#define SUCCESSOR(p) Ppmd_GET_SUCCESSOR(p)

void Ppmd7_UpdateModel(CPpmd7 *p);

#define MASK(sym) ((unsigned char *)charMask)[sym]

//MY_FORCE_INLINE
//static
void Ppmd7a_EncodeSymbol(CPpmd7 *p, int symbol)
{
  size_t charMask[256 / sizeof(size_t)];
  
  if (p->MinContext->NumStats != 1)
  {
    CPpmd_State *s = Ppmd7_GetStats(p, p->MinContext);
    UInt32 sum;
    unsigned i;
   

    
    
    //R->Range /= p->MinContext->Union2.SummFreq;
    
    if (s->Symbol == symbol)
    {
      // R->Range /= p->MinContext->Union2.SummFreq;
      RC_EncodeFinal(0, s->Freq, p->MinContext->Union2.SummFreq);
      p->FoundState = s;
      Ppmd7_Update1_0(p);
      return;
    }
    p->PrevSuccess = 0;
    sum = s->Freq;
    i = (unsigned)p->MinContext->NumStats - 1;
    do
    {
      if ((++s)->Symbol == symbol)
      {
        // R->Range /= p->MinContext->Union2.SummFreq;
        RC_EncodeFinal(sum, s->Freq, p->MinContext->Union2.SummFreq);
        p->FoundState = s;
        Ppmd7_Update1(p);
        return;
      }
      sum += s->Freq;
    }
    while (--i);

    // R->Range /= p->MinContext->Union2.SummFreq;
    RC_Encode(sum, p->MinContext->Union2.SummFreq - sum, p->MinContext->Union2.SummFreq);
    
    p->HiBitsFlag = PPMD7_HiBitsFlag_3(p->FoundState->Symbol);
    PPMD_SetAllBitsIn256Bytes(charMask);
    // MASK(s->Symbol) = 0;
    // i = p->MinContext->NumStats - 1;
    // do { MASK((--s)->Symbol) = 0; } while (--i);
    {
      CPpmd_State *s2 = Ppmd7_GetStats(p, p->MinContext);
      MASK(s->Symbol) = 0;
      do
      {
        unsigned sym0 = s2[0].Symbol;
        unsigned sym1 = s2[1].Symbol;
        s2 += 2;
        MASK(sym0) = 0;
        MASK(sym1) = 0;
      }
      while (s2 < s);
    }
  }
  else
  {
    UInt16 *prob = Ppmd7_GetBinSumm(p);
    CPpmd_State *s = Ppmd7Context_OneState(p->MinContext);
    UInt32 pr = *prob;
    UInt32 bound = (R->Range >> 14) * pr;
    pr = PPMD_UPDATE_PROB_1(pr);
    if (s->Symbol == symbol)
    {
      *prob = (UInt16)(pr + (1 << PPMD_INT_BITS));
      // RangeEnc_EncodeBit_0(p, bound);
      R->Range = bound;
      RC_NORM(p);
      
      // p->FoundState = s;
      // Ppmd7_UpdateBin(p);
      {
        unsigned freq = s->Freq;
        CTX_PTR c = CTX(SUCCESSOR(s));
        p->FoundState = s;
        p->PrevSuccess = 1;
        p->RunLength++;
        s->Freq = (Byte)(freq + (freq < 128));
        // NextContext(p);
        if (p->OrderFall == 0 && (const Byte *)c > p->Text)
          p->MaxContext = p->MinContext = c;
        else
          Ppmd7_UpdateModel(p);
      }
      return;
    }

    *prob = (UInt16)pr;
    p->InitEsc = p->ExpEscape[pr >> 10];
    // RangeEnc_EncodeBit_1(p, bound);
    R->Low += bound;
    R->Range = (R->Range & ~((UInt32)PPMD_BIN_SCALE - 1)) - bound;
    RC_NORM_LOCAL(p)
    
    PPMD_SetAllBitsIn256Bytes(charMask);
    MASK(s->Symbol) = 0;
    p->PrevSuccess = 0;
  }

  for (;;)
  {
    CPpmd_See *see;
    CPpmd_State *s;
    UInt32 sum, escFreq;
    CPpmd7_Context *mc;
    unsigned i, numMasked;
    
    RC_NORM_REMOTE(p)

    mc = p->MinContext;
    numMasked = mc->NumStats;

    do
    {
      p->OrderFall++;
      if (!mc->Suffix)
        return; /* EndMarker (symbol = -1) */
      mc = Ppmd7_GetContext(p, mc->Suffix);
      i = mc->NumStats;
    }
    while (i == numMasked);

    p->MinContext = mc;
    
    // see = Ppmd7_MakeEscFreq(p, numMasked, &escFreq);
    {
      if (i != 256)
      {
        unsigned nonMasked = i - numMasked;
        see = p->See[(unsigned)p->NS2Indx[(size_t)nonMasked - 1]]
            + p->HiBitsFlag
            + (nonMasked < (unsigned)SUFFIX(mc)->NumStats - i)
            + 2 * (unsigned)(mc->Union2.SummFreq < 11 * i)
            + 4 * (unsigned)(numMasked > nonMasked);
        {
          // if (see->Summ) field is larger than 16-bit, we need only low 16 bits of Summ
          unsigned summ = (UInt16)see->Summ; // & 0xFFFF
          unsigned r = (summ >> see->Shift);
          see->Summ = (UInt16)(summ - r);
          escFreq = r + (r == 0);
        }
      }
      else
      {
        see = &p->DummySee;
        escFreq = 1;
      }
    }

    s = Ppmd7_GetStats(p, mc);
    sum = 0;
    // i = mc->NumStats;

    do
    {
      unsigned cur = s->Symbol;
      if ((int)cur == symbol)
      {
        UInt32 low = sum;
        UInt32 freq = s->Freq;
        unsigned num2;

        Ppmd_See_Update(see);
        p->FoundState = s;
        sum += escFreq;

        num2 = i / 2;
        i &= 1;
        sum += freq & (0 - (UInt32)i);
        if (num2 != 0)
        {
          s += i;
          for (;;)
          {
            unsigned sym0 = s[0].Symbol;
            unsigned sym1 = s[1].Symbol;
            s += 2;
            sum += (s[-2].Freq & (unsigned)(MASK(sym0)));
            sum += (s[-1].Freq & (unsigned)(MASK(sym1)));
            if (--num2 == 0)
              break;
          }
        }

        
        //R->Range /= sum;
        RC_EncodeFinal(low, freq, sum);
        Ppmd7_Update2(p);
        return;
      }
      sum += (s->Freq & (unsigned)(MASK(cur)));
      s++;
    }
    while (--i);
    
    {
      UInt32 total = sum + escFreq;
      see->Summ = (UInt16)(see->Summ + total);

      //R->Range /= total;
      RC_Encode(sum, escFreq, total);
    }

    {
      CPpmd_State *s2 = Ppmd7_GetStats(p, p->MinContext);
      s--;
      MASK(s->Symbol) = 0;
      do
      {
        unsigned sym0 = s2[0].Symbol;
        unsigned sym1 = s2[1].Symbol;
        s2 += 2;
        MASK(sym0) = 0;
        MASK(sym1) = 0;
      }
      while (s2 < s);
    }
  }
}

#if 0
void Ppmd7z_EncodeSymbols(CPpmd7 *p, const Byte *buf, const Byte *lim)
{
  for (; buf < lim; buf++)
  {
    Ppmd7z_EncodeSymbol(p, *buf);
  }
}
#endif
