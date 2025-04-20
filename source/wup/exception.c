#include <wup/wup.h>
#include "font.h"
#include "exception.h"


static void ExcPrintStr(int xpos, int ypos, char* str)
{
    u8* fb = Video_GetOvlFramebuffer();
    int len = strlen(str);

    int x = xpos * 8;
    int y = ypos * 16;
    u8* bufptr = &fb[y*854 + x];
    for (int i = 0; i < len && str[i]; i++)
    {
        char ch = str[i];
        if ((ch == '\n') || (x >= 847))
        {
            // go to next line
            x = xpos * 8;
            y += 16;

            bufptr = &fb[y*854 + x];
        }

        if (ch == '\r' || ch == '\n')
            continue;

        u8* glyph = (u8*)&font[ch<<4];
        for (int cy = 0; cy < 16; cy++)
        {
            u8 row = *glyph++;
            for (int cx = 0; cx < 8; cx++)
            {
                if (row & 0x80)
                    bufptr[cy*854 + cx] = 0xFF;
                else
                    bufptr[cy*854 + cx] = 0;
                row <<= 1;
            }
        }

        x += 8;
        bufptr += 8;
    }
}

static void ExcPrintHex(int xpos, int ypos, u32 val)
{
    char str[9];

    for (int i = 0; i < 8; i++)
    {
        u32 c = val >> 28;
        if (c < 10)
            str[i] = '0' + c;
        else
            str[i] = 'A' + (c-10);

        val <<= 4;
    }

    str[8] = '\0';
    ExcPrintStr(xpos, ypos, str);
}


void ExceptionHandler(u32 type, u32* regdump)
{
    DisableIRQ();
    REG_IRQ_ENABLEMASK0 = 0xFFFF;
    REG_IRQ_ENABLEMASK1 = 0xFFFF;
    REG_SPDMA_START(0) = SPDMA_STOP;
    REG_SPDMA_START(1) = SPDMA_STOP;
    REG_GPDMA_START(0) = GPDMA_STOP;
    REG_GPDMA_START(1) = GPDMA_STOP;
    REG_GPDMA_START(2) = GPDMA_STOP;

    DC_FlushAll();
    IC_InvalidateAll();
    DisableMMU();

    u32 pal[256];
    pal[0] = 0xFF0000FF;
    for (int i = 1; i < 256; i++)
        pal[i] = 0xFFFFFFFF;

    Video_SetOvlPalette(0, pal, 256);

    //u8* fb = Video_GetOvlFramebuffer();
    u8* fb = (u8*)0x300000;
    memset(fb, 0, 854*480);
    Video_SetOvlFramebuffer(fb, 0, 0, 854, 480, 854, PIXEL_FMT_PAL_256);
    Video_SetOvlEnable(1);
    Video_SetDisplayEnable(1);

    ExcPrintStr(1, 1, "-- EXCEPTION --");

    switch (type)
    {
    case 0: ExcPrintStr(1, 3, "Undefined instruction"); break;
    case 1: ExcPrintStr(1, 3, "Prefetch abort"); break;
    case 2: ExcPrintStr(1, 3, "Data abort"); break;
    case 3: ExcPrintStr(1, 3, "Unsupported Samsung SoC"); break;
    }

    if (regdump)
    {
        ExcPrintStr(1, 5, "-- REGISTER DUMP --");
        ExcPrintStr(1, 7, "R0  = ________  |  R1  = ________  |  R2  = ________  |  R3  = ________");
        ExcPrintStr(1, 8, "R4  = ________  |  R5  = ________  |  R6  = ________  |  R7  = ________");
        ExcPrintStr(1, 9, "R8  = ________  |  R9  = ________  |  R10 = ________  |  R11 = ________");
        ExcPrintStr(1, 10, "R12 = ________  |  SP  = ________  |  LR  = ________  |  PC  = ________");

        for (int i = 0; i < 16; i++)
        {
            int xpos = 7 + (19 * (i&3));
            int ypos = 7 + (i>>2);
            ExcPrintHex(xpos, ypos, regdump[i]);
        }
    }

    for (;;) WaitForIRQ();
}
