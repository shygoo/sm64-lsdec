#define SWAP16(i) ((((i) & 0xFF) << 8) | (((i) & 0xFF00) >> 8))
#define SWAP32(i) ((((i) & 0xFF000000) >> 24) | (((i) & 0xFF0000) >> 8) | (((i) & 0xFF00) << 8) | (((i) & 0xFF) << 24))
#define HI16(i) (((i) >> 16) & 0xFFFF)
#define LO16(i) ((i) & 0xFFFF)

typedef char                 s8;
typedef unsigned char        u8;
typedef short               s16;
typedef unsigned short      u16;
typedef int                 s32;
typedef unsigned int        u32;
typedef long long           s64;
typedef unsigned long long  u64;

#define U32BE(p) (SWAP32(*(u32*)p))
#define U16BE(p)  (SWAP16(*(u16*)p))