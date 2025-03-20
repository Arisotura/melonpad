#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include <wup/wup.h>


u32_t sys_now()
{
    return WUP_GetTicks();
}
