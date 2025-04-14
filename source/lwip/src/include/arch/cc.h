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
#define LWIP_PLATFORM_ASSERT(x) do { printf(x); printf("\n"); DisableIRQ(); for (;;); } while (0)

void sys_mark_tcpip_thread();
void sys_assert_core_locked();
#define LWIP_MARK_TCPIP_THREAD() sys_mark_tcpip_thread()

#define LWIP_ERRNO_STDINCLUDE

#ifndef sys_msleep
#define sys_msleep sys_msleep
#endif

#define LWIP_TIMEVAL_PRIVATE 0
#include <sys/types.h>

#endif // _ARCH_CC_H_
