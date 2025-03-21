#include <wup/wup.h>


// GFX TODO
// * framebuffer management
// * support different pixel formats
// * support different resolutions/refresh rates?
// * use DMA for blitting
// * double buffering?


volatile u8 VBlankFlag;
void GFX_VBlank(int irq, void* userdata);

// TODO: make this not hardcoded?
//u8* Framebuffer = (u8*)0x380000;
u8* Framebuffer = (u8*)0x300000;


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
        REG_UNK9700 = (REG_UNK9700 & ~0x7FFFFF) | 0x94921;
        REG_UNK9708 |= 1;

        WUP_DelayMS(1);
    }
    else
    {
        REG_UNK9700 = (REG_UNK9700 & ~0x3FFFFFF) | 0x6C9242;
        REG_UNK9704 |= 0x63;
        REG_UNK9708 = (REG_UNK9708 & ~0xF) | 5;

        WUP_DelayMS(1);

        REG_UNK9708 |= 2;
        while (REG_UNK970C & 1);
        REG_UNK9708 |= 8;
    }

    *(vu32*)0xF0009480 = (*(vu32*)0xF0009480 & ~0x1F) | 2;
    *(vu32*)0xF0008C18 = 0x86531E36;
    *(vu32*)0xF00094F8 = 0x1DC;

    //

    REG_LCD_FB_MEMADDR = (u32)Framebuffer;
    REG_LCD_FB_XOFFSET = 96;
    REG_LCD_FB_YOFFSET = 8;
    REG_LCD_FB_WIDTH = 854;
    REG_LCD_FB_HEIGHT = 480;
    //REG_LCD_FB_STRIDE = 856;
    //REG_LCD_PIXEL_FMT = (REG_LCD_PIXEL_FMT & ~0x7) | 0;
    REG_LCD_FB_STRIDE = 854;
    REG_LCD_PIXEL_FMT = (REG_LCD_PIXEL_FMT & ~0x7) | 3;
    REG_LCD_UNKB4 &= ~0xFFFF;

    REG_LCD_DISP_CNT = (REG_LCD_DISP_CNT & 0x14) | DISP_ENABLE_OVERLAY;
    REG_LCD_HEND = 950;
    REG_LCD_HSTART = 96;
    REG_LCD_HTIMING = (944 << 16) | 1047;
    REG_LCD_UNK08 = 32;
    REG_LCD_VEND = 488;
    REG_LCD_VSTART = 8;
    REG_LCD_UNK20 = 96;
    REG_LCD_UNK24 = 854;
    REG_LCD_UNK28 = 8;
    REG_LCD_VTIMING = (434 << 16) | 510;
    REG_LCD_UNK0C = 8;

    // setup IRQ 0x1E with a 180 Hz frequency
    REG_VCOUNT_MATCH(0) = 8;
    REG_VCOUNT_MATCH(1) = 178;
    REG_VCOUNT_MATCH(2) = 348;
    REG_VCOUNT_MATCH(3) = 0x7FF; // disable

    // enable display
    REG_LCD_DISP_CNT |= DISP_ENABLE;

    VBlankFlag = 0;
    WUP_SetIRQHandler(IRQ_VBLANK, GFX_VBlank, NULL, 0);
}


void GFX_VBlank(int irq, void* userdata)
{
    VBlankFlag = 1;

    //Console_Update();
}

void GFX_WaitForVBlank()
{
    VBlankFlag = 0;
    while (!VBlankFlag)
        WaitForIRQ();

    Console_Update();
}


void GFX_SetPalette(u8 offset, u32* pal, int len)
{
    REG_PALETTE_ADDR = offset;
    for (int i = 0; i < len; i++)
        REG_PALETTE_DATA = pal[i];
}

void* GFX_GetFramebuffer()
{
    return Framebuffer;
}
