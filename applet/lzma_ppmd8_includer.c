#define PPMD8_FREEZE_SUPPORT
#define CTX_PTR CTX_PTR_PPMD8
#include "../lib/lzma/C/Ppmd8.c"
#undef CTX_PTR
#define CTX_PTR CTX_PTR_PPMD8ENC
#include "../lib/lzma/C/Ppmd8Enc.c"
#undef CTX_PTR
#undef RC_NORM_BASE
#undef RC_NORM
#undef R
#define CTX_PTR CTX_PTR_PPMD8DEC
#include "../lib/lzma/C/Ppmd8Dec.c"
