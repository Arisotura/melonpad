#include "wup.h"


// GFX TODO
// * framebuffer management
// * support different pixel formats
// * support different resolutions/refresh rates?
// * use DMA for blitting
// * double buffering?


volatile u8 VBlankFlag;
void GFX_VBlank(int irq, void* userdata);

// TODO: make this not hardcoded?
u8* Framebuffer = (u8*)0x380000;


void GFX_Init()
{
    // TODO: name registers more nicely

    *(vu32*)0xF0008C24 = 0x23;
    *(vu32*)0xF0008C28 = (*(vu32*)0xF0008C28 & ~0xFFF0000) | 0xFFE0000;
    *(vu32*)0xF000848C |= 2;
    *(vu32*)0xF00094B0 = (*(vu32*)0xF00094B0 & ~0xFF000C) | 0xFF0008;
    *(vu32*)0xF00094B4 &= ~0xFFFF;

    if (WUP_HardwareType() == 0x41)
    {
        *(vu32*)0xF0009700 = (*(vu32*)0xF0009700 & ~0x7FFFFF) | 0x94921;
        *(vu32*)0xF0009708 |= 1;

        WUP_DelayMS(1);
    }
    else
    {
        *(vu32*)0xF0009700 = (*(vu32*)0xF0009700 & ~0x3FFFFFF) | 0x6C9242;
        *(vu32*)0xF0009704 |= 0x63;
        *(vu32*)0xF0009708 = (*(vu32*)0xF0009708 & ~0xF) | 5;

        WUP_DelayMS(1);

        *(vu32*)0xF0009708 |= 2;
        while (*(vu32*)0xF000970C & 1);
        *(vu32*)0xF0009708 |= 8;
    }

    *(vu32*)0xF0009480 = (*(vu32*)0xF0009480 & ~0x1F) | 2;
    *(vu32*)0xF0008C18 = 0x86531E36;
    *(vu32*)0xF00094F8 = 0x1DC;

    //

    *(vu32*)0xF0009474 = (u32)Framebuffer;
    *(vu32*)0xF0009460 = 0x60;
    *(vu32*)0xF0009468 = 0x8;
    *(vu32*)0xF0009464 = 854;
    *(vu32*)0xF000946C = 480;
    *(vu32*)0xF0009470 = 854;
    *(vu32*)0xF00094B0 = (*(vu32*)0xF00094B0 & ~0x7) | 0;
    *(vu32*)0xF00094B4 &= ~0xFFFF;

    *(vu32*)0xF0009480 = (*(vu32*)0xF0009480 & 0x14) | 2;
    *(vu32*)0xF0009418 = 950; // total horizontal span
    *(vu32*)0xF0009410 = 96;
    *(vu32*)0xF0009400 = 0x03B00417; // 944-1047
    *(vu32*)0xF0009408 = 32;
    *(vu32*)0xF000941C = 488; // total vertical span
    *(vu32*)0xF0009414 = 8;
    *(vu32*)0xF0009420 = 96;
    *(vu32*)0xF0009424 = 854;
    *(vu32*)0xF0009428 = 8;
    *(vu32*)0xF0009404 = 0x01B201FE; // 434-510
    *(vu32*)0xF000940C = 8;

    *(vu32*)0xF0009508 = 8;
    *(vu32*)0xF000950C = 0xB2;
    *(vu32*)0xF0009510 = 0x15C;
    *(vu32*)0xF0009514 = 0x7FF;

    // important!!
    // needs to have bit1 and bit4 set for display to work
    *(vu32*)0xF0009480 |= 0x10;

    VBlankFlag = 0;
    WUP_SetIRQHandler(IRQ_VBLANK, GFX_VBlank, NULL, 0);
}


void GFX_VBlank(int irq, void* userdata)
{
    VBlankFlag = 1;
}

void GFX_WaitForVBlank()
{
    VBlankFlag = 0;
    while (!VBlankFlag)
        WaitForIRQ();
}


void GFX_SetPalette(u8 offset, u32* pal, int len)
{
    *(vu32*)0xF0009500 = offset;
    for (int i = 0; i < len; i++)
        *(vu32*)0xF0009504 = pal[i];
}

void* GFX_GetFramebuffer()
{
    return Framebuffer;
}
