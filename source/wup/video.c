#include <wup/wup.h>


// GFX TODO
// * framebuffer management
// * support different pixel formats
// * support different resolutions/refresh rates?
// * use DMA for blitting
// * double buffering?


static volatile u8 IsVBlank;
static volatile u8 VBlankFlag;
static void VBlankIRQ(int irq, void* userdata);
static void VBlankEndIRQ(int irq, void* userdata);


void Video_Init()
{
    *(vu32*)0xF0008C24 = 0x23;
    *(vu32*)0xF0008C28 = (*(vu32*)0xF0008C28 & ~0xFFF0000) | 0xFFE0000;
    *(vu32*)0xF000848C |= 2;
    REG_LCD_PIXEL_FMT = (REG_LCD_PIXEL_FMT & ~0xFF000C) | 0xFF0008;
    REG_LCD_UNKB4 &= ~0xFFFF;

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

    REG_LCD_DISP_CNT &= ~0x1F;
    *(vu32*)0xF0008C18 = 0x86531E36;
    *(vu32*)0xF00094F8 = 0x1DC;

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

    IsVBlank = 0;
    VBlankFlag = 0;
    WUP_SetIRQHandler(IRQ_VBLANK_END, VBlankEndIRQ, NULL, 0);
    WUP_SetIRQHandler(IRQ_VBLANK, VBlankIRQ, NULL, 0);
}


static void VBlankIRQ(int irq, void* userdata)
{
    IsVBlank = 1;
    VBlankFlag = 1;
}

static void VBlankEndIRQ(int irq, void* userdata)
{
    IsVBlank = 0;
}

void Video_WaitForVBlank()
{
    VBlankFlag = 0;
    while (!VBlankFlag)
        WaitForIRQ();
}

int Video_IsVBlank()
{
    return IsVBlank;
}


void Video_SetDisplayEnable(int enable)
{
    if (enable)
        REG_LCD_DISP_CNT |= DISP_ENABLE;
    else
        REG_LCD_DISP_CNT &= ~DISP_ENABLE;
}


void Video_SetOvlEnable(int enable)
{
    if (enable)
        REG_LCD_DISP_CNT |= DISP_ENABLE_OVERLAY;
    else
        REG_LCD_DISP_CNT &= ~DISP_ENABLE_OVERLAY;
}

int Video_SetOvlFramebuffer(void* buffer, int x, int y, int width, int height, int stride, int format)
{
    u32 bufptr = (u32)buffer;
    // CHECKME: is there an alignment requirement for the framebuffer?

    format &= 0x3;

    // for 16-bit color formats, stride is in 16-bit units
    if (format == PIXEL_FMT_ARGB1555 || format == PIXEL_FMT_RGB565)
        stride >>= 1;

    REG_LCD_FB_MEMADDR = bufptr;
    REG_LCD_FB_XOFFSET = 96 + x;
    REG_LCD_FB_YOFFSET = 8 + y;
    REG_LCD_FB_WIDTH = width;
    REG_LCD_FB_HEIGHT = height;
    REG_LCD_FB_STRIDE = stride;
    REG_LCD_PIXEL_FMT = (REG_LCD_PIXEL_FMT & ~0x7) | format;
    REG_LCD_UNKB4 &= ~0xFFFF;

    return 1;
}

void* Video_GetOvlFramebuffer()
{
    return (void*)REG_LCD_FB_MEMADDR;
}


void Video_SetOvlPalette(u8 offset, u32* pal, int len)
{
    REG_PALETTE_ADDR = offset;
    for (int i = 0; i < len; i++)
        REG_PALETTE_DATA = pal[i];
}

void Video_GetOvlPalette(u8 offset, u32* pal, int len)
{
    for (int i = 0; i < len; i++)
    {
        REG_PALETTE_ADDR = offset + i;
        pal[i] = REG_PALETTE_DATA;
    }
}
