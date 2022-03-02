#define CTX_PTR CTX_PTR_PPMD7
#include "../lib/lzma/C/Ppmd7.c"
#undef CTX_PTR
#define CTX_PTR CTX_PTR_PPMD7ENC
#include "../lib/lzma/C/Ppmd7Enc.c"
#undef CTX_PTR
#undef RC_NORM_BASE
#undef RC_NORM
#undef R
#define CTX_PTR CTX_PTR_PPMD7DEC
#include "../lib/lzma/C/Ppmd7Dec.c"
