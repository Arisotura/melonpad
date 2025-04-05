#ifndef _ARCH_CC_H_
#define _ARCH_CC_H_

#include <wup/wup.h>

#define U16_F "hu"
#define S16_F "d"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "uz"

#define BYTE_ORDER LITTLE_ENDIAN

#define LWIP_CHKSUM_ALGORITHM 3

#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x) printf(x)
#define LWIP_PLATFORM_ASSERT(x) do { printf(x); DisableIRQ(); for (;;); } while (0)

// DO MORE PROPERLY!!
/*#define SYS_ARCH_PROTECT(x) do { x = DisableIRQ(); } while (0)
#define SYS_ARCH_UNPROTECT(x) do { RestoreIRQ(x); } while (0)
#define SYS_ARCH_DECL_PROTECT(x) int x*/

#define LWIP_ERRNO_STDINCLUDE

#ifndef sys_msleep
#define sys_msleep sys_msleep
#endif

#endif // _ARCH_CC_H_
