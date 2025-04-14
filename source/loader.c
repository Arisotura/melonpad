#include <wup/wup.h>
#include "loader.h"

void DeInit();


void LoadBinaryFromFlash(u32 addr)
{
    DeInit();

    u32 loaderaddr, loaderlen;
    Flash_GetEntryInfo("LDRf", &loaderaddr, &loaderlen, NULL);

    u8 excepvectors[64];
    Flash_Read(loaderaddr, excepvectors, 64);
    Flash_Read(loaderaddr + 64, (u8*)0x3F0000, loaderlen - 64);

    DisableIRQ();
    REG_IRQ_ENABLEMASK0 = 0xFFFF;
    REG_IRQ_ENABLEMASK1 = 0xFFFF;

    for (int i = 0; i < 64; i++)
        *(u8*)i = excepvectors[i];

    void* loadermain = (void*)(*(vu32*)0x20);
    ((void(*)(u32))loadermain)(addr);
}
