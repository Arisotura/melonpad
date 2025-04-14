#include <stdio.h>
#include <string.h>

#include <wup/wup.h>


void main(u32 loadaddr)
{
    u32 codeaddr, codelen;
    if (!Flash_GetCodeAddr(loadaddr, &codeaddr, &codelen))
    {
        // TODO handle this gracefully somehow??
        DisableIRQ();
        for(;;);
    }

    u8 excepvectors[64];
    Flash_Read(codeaddr, excepvectors, 64);

    // load bulk of code blob
    Flash_Read(codeaddr + 64, (u8*)64, codelen - 64);

    // disable IRQs
    DisableIRQ();
    REG_IRQ_ENABLEMASK0 = 0xFFFF;
    REG_IRQ_ENABLEMASK1 = 0xFFFF;

    // load exception vectors
    for (int i = 0; i < 64; i++)
        *(u8*)i = excepvectors[i];

    // trigger soft reset
    REG_HARDWARE_RESET = 0xFFFFFFFB;
    REG_HARDWARE_RESET = 0;
    REG_CPU_RESET = 0;
    REG_CPU_RESET = 1;

	for(;;);
}



