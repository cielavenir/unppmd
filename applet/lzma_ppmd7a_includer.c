#define CTX_PTR CTX_PTR_PPMD7aENC
#include "../lib/lzma/C/Ppmd7aEnc.c"
#undef CTX_PTR
#undef RC_NORM_BASE
#undef RC_NORM
#undef R
#define CTX_PTR CTX_PTR_PPMD7aDEC
#include "../lib/lzma/C/Ppmd7aDec.c"
