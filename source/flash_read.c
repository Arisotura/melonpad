#include <wup/wup.h>
#include <lvgl/lvgl.h>
#include "main.h"
#include "flash_read.h"

static sFlashReadArgs frArgs;


static int FrStart(void* _args, const char* filename, const char* mode)
{
    return 1;
}

static int FrRead(void* _args, int pos, void* data, int len)
{
    sFlashReadArgs* args = (sFlashReadArgs*)_args;

    if (pos >= args->Length)
        return 0;

    if ((pos + len) > args->Length)
        len = args->Length - pos;

    Flash_Read(args->Offset + pos, data, len);
    return len;
}

static void FrFinish(void* _args)
{
}

static void FrError(void* _args, u16 code, const char* msg)
{
}


void FlashReadStart(u32 offset, u32 length)
{
    strncpy(frArgs.Base.Title, "Dump FLASH memory", sizeof(frArgs.Base.Title));
    frArgs.Base.StartCB = FrStart;
    frArgs.Base.ReadCB = FrRead;
    frArgs.Base.FinishCB = FrFinish;
    frArgs.Base.ErrorCB = FrError;
    frArgs.Offset = offset;
    frArgs.Length = length;

    ScOpen(&scTftpSend, &frArgs, NULL);
}
