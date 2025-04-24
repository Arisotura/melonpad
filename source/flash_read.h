#ifndef _FLASH_READ_H_
#define _FLASH_READ_H_

#include "sc_tftp_send.h"

typedef struct sFlashReadArgs
{
    sTftpSendArgs Base;
    u32 Offset;
    u32 Length;

} sFlashReadArgs;

void FlashReadStart(u32 offset, u32 length);

#endif //_FLASH_READ_H_
