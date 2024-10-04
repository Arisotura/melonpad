#include "wup.h"
#include "font.h"


static u8 Active = 0;

static int Width, Height;

static int BufferTop;
static int BufferBottom;
static int NumLines;
static int LineStride;
static u8* Buffer = NULL;

static u8 Dirty = 0;


int Console_Open(int w, int h)
{
    // the buffer's stride needs to match that of the framebuffer
    LineStride = 856 / 8;
    int totalsize = LineStride * 16 * h;

    Buffer = (u8*)malloc(totalsize);
    if (!Buffer)
        return 0;

    u32 pal[256];
    pal[0] = 0xFF000000;
    for (int i = 1; i < 256; i++)
        pal[i] = 0xFFFFFFFF;

    GFX_SetPalette(0, pal, 256);

    Width = w;
    Height = h;
    Active = 1;

    Console_Clear();

    return 1;
}

int Console_OpenDefault()
{
    return Console_Open(106, 30);
}

void Console_Close()
{
    if (!Active) return;

    free(Buffer);
    Buffer = NULL;

    Active = 0;
}

int Console_IsActive()
{
    return Active;
}


void Console_Clear()
{
    if (!Active) return;

    BufferTop = 0;
    BufferBottom = 0;
    NumLines = 0;
    memset(Buffer, 0, LineStride * 16 * Height);

    Dirty = 1;
}

void Console_Print(char* str, int len)
{
    if (!Active) return;

    // TODO: support escape codes and shit?

    u8* bufptr = &Buffer[LineStride * 16 * BufferBottom];
    int x = 0;
    for (int i = 0; i < len && str[i]; i++)
    {
        char ch = str[i];
        if ((ch == '\n') || (x >= Width))
        {
            // fill the rest of the line with spaces
            if (x < Width)
            {
                for (int cy = 0; cy < 16; cy++)
                {
                    for (int cx = 0; cx < Width-x; cx++)
                        bufptr[LineStride * cy + cx] = 0;
                }
            }

            // go to next line
            x = 0;
            BufferBottom++;
            if (BufferBottom >= Height) BufferBottom = 0;
            if (NumLines >= Height)
            {
                BufferTop++;
                if (BufferTop >= Height) BufferTop = 0;
            }
            else
                NumLines++;

            bufptr = &Buffer[LineStride * 16 * BufferBottom];
        }

        if (ch == '\r' || ch == '\n')
            continue;

        u8* glyph = (u8*)&font[ch<<4];
        for (int cy = 0; cy < 16; cy++)
        {
            u8 row = *glyph++;
            bufptr[LineStride * cy] = row;
        }

        x++;
        bufptr++;
    }

    Dirty = 1;
}

void Console_Update()
{
    if (!Active) return;
    if (!Dirty) return;

    u8* framebuf = GFX_GetFramebuffer();
    int fbstride = 856; // TODO: not hardcode this!!

    DC_FlushRange(Buffer, LineStride * 16 * Height);

    int l1 = Height - BufferTop;
    if (l1)
    {
        int length = l1 * 16 * fbstride;
        GPDMA_BlitMaskedFill(2,
                             &Buffer[LineStride * 16 * BufferTop], 0xFF, 0x00, 0,
                             framebuf, fbstride, fbstride,
                             length);
    }

    if (l1 != Height)
    {
        int l2 = Height - l1;
        int length = l2 * 16 * fbstride;
        GPDMA_BlitMaskedFill(1,
                             &Buffer[0], 0xFF, 0x00, 0,
                             &framebuf[fbstride * 16 * l1], fbstride, fbstride,
                             length);
    }

    Dirty = 0;
}
