#include <wup/wup.h>


volatile u8 DMA_IRQFlag[5];
void DMA_IRQHandler(int irq, void* userdata);


void DMA_Init()
{
    // enable DMA
    // CHECKME they set it to 0003 in the stock bootloader
    REG_DMA_CNT |= 0x8001;
    REG_DMA_CNT &= ~0xFC;

    for (int i = 0; i < 5; i++)
        DMA_IRQFlag[i] = 0;

    WUP_SetIRQHandler(IRQ_SPDMA0, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_SPDMA1, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_GPDMA0, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_GPDMA1, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_GPDMA2, DMA_IRQHandler, NULL, 0);
}


void DMA_IRQHandler(int irq, void* userdata)
{
    switch (irq)
    {
        case IRQ_SPDMA0: DMA_IRQFlag[0] = 1; break;
        case IRQ_SPDMA1: DMA_IRQFlag[1] = 1; break;
        case IRQ_GPDMA0: DMA_IRQFlag[2] = 1; break;
        case IRQ_GPDMA1: DMA_IRQFlag[3] = 1; break;
        case IRQ_GPDMA2: DMA_IRQFlag[4] = 1; break;
    }
}


void SPDMA_Transfer(u32 chan, const void* data, u32 peri, u32 dir, u32 len)
{
    if (chan > 1) return;

    SPDMA_Wait(chan);

    DMA_IRQFlag[chan] = 0;
    REG_SPDMA_CNT(chan) = dir | peri;
    REG_SPDMA_UNK08(chan) = 0;
    REG_SPDMA_UNK0C(chan) = 0;
    REG_SPDMA_LEN(chan) = len - 1;
    REG_SPDMA_MEMADDR(chan) = (u32)data;
    REG_SPDMA_START(chan) = SPDMA_START;
}

void SPDMA_Wait(u32 chan)
{
    if (chan > 1) return;

    if (REG_SPDMA_START(chan) & SPDMA_BUSY)
    {
        while (!DMA_IRQFlag[chan])
            WaitForIRQ();
    }

    REG_SPDMA_START(chan) = SPDMA_STOP;
}


void GPDMA_Transfer(u32 chan, const void* src, void* dst, u32 len)
{
    if (chan > 2) return;

    GPDMA_Wait(chan);

    DMA_IRQFlag[2+chan] = 0;
    REG_GPDMA_CNT(chan) = GPDMA_SRC_INCREMENT;
    REG_GPDMA_LINELEN(chan) = len;
    REG_GPDMA_SRCSTRIDE(chan) = len;
    REG_GPDMA_DSTSTRIDE(chan) = len;
    REG_GPDMA_LEN(chan) = len - 1;
    REG_GPDMA_SRCADDR(chan) = (u32)src;
    REG_GPDMA_DSTADDR(chan) = (u32)dst;
    REG_GPDMA_FGFILL(chan) = 0;
    REG_GPDMA_BGFILL(chan) = 0;
    REG_GPDMA_START(chan) = GPDMA_START;
}

void GPDMA_Fill(u32 chan, int fillval, int fill16, void* dst, u32 len)
{
    if (chan > 2) return;

    GPDMA_Wait(chan);

    u32 fillw;
    if (fill16)
    {
        fillw = GPDMA_FILL_16BIT;
        fillval &= 0xFFFF;
    }
    else
    {
        fillw = GPDMA_FILL_8BIT;
        fillval &= 0xFF;
    }

    DMA_IRQFlag[2+chan] = 0;
    REG_GPDMA_CNT(chan) = GPDMA_SRC_INCREMENT | GPDMA_SIMPLE_FILL | fillw;
    REG_GPDMA_LINELEN(chan) = len;
    REG_GPDMA_SRCSTRIDE(chan) = 0;
    REG_GPDMA_DSTSTRIDE(chan) = len;
    REG_GPDMA_LEN(chan) = len - 1;
    REG_GPDMA_SRCADDR(chan) = 0;
    REG_GPDMA_DSTADDR(chan) = (u32)dst;
    REG_GPDMA_FGFILL(chan) = fillval;
    REG_GPDMA_BGFILL(chan) = 0;
    REG_GPDMA_START(chan) = GPDMA_START;
}

void GPDMA_BlitTransfer(u32 chan, const void* src, u32 srcstride, void* dst, u32 dststride, u32 linelen, u32 len)
{
    if (chan > 2) return;

    GPDMA_Wait(chan);

    DMA_IRQFlag[2+chan] = 0;
    REG_GPDMA_CNT(chan) = GPDMA_SRC_INCREMENT;
    REG_GPDMA_LINELEN(chan) = linelen;
    REG_GPDMA_SRCSTRIDE(chan) = srcstride;
    REG_GPDMA_DSTSTRIDE(chan) = dststride;
    REG_GPDMA_LEN(chan) = len - 1;
    REG_GPDMA_SRCADDR(chan) = (u32)src;
    REG_GPDMA_DSTADDR(chan) = (u32)dst;
    REG_GPDMA_FGFILL(chan) = 0;
    REG_GPDMA_BGFILL(chan) = 0;
    REG_GPDMA_START(chan) = GPDMA_START;
}

void GPDMA_BlitFill(u32 chan, int fillval, int fill16, void* dst, u32 dststride, u32 linelen, u32 len)
{
    if (chan > 2) return;

    GPDMA_Wait(chan);

    u32 fillw;
    if (fill16)
    {
        fillw = GPDMA_FILL_16BIT;
        fillval &= 0xFFFF;
    }
    else
    {
        fillw = GPDMA_FILL_8BIT;
        fillval &= 0xFF;
    }

    DMA_IRQFlag[2+chan] = 0;
    REG_GPDMA_CNT(chan) = GPDMA_SRC_INCREMENT | GPDMA_SIMPLE_FILL | fillw;
    REG_GPDMA_LINELEN(chan) = linelen;
    REG_GPDMA_SRCSTRIDE(chan) = 0;
    REG_GPDMA_DSTSTRIDE(chan) = dststride;
    REG_GPDMA_LEN(chan) = len - 1;
    REG_GPDMA_SRCADDR(chan) = 0;
    REG_GPDMA_DSTADDR(chan) = (u32)dst;
    REG_GPDMA_FGFILL(chan) = fillval;
    REG_GPDMA_BGFILL(chan) = 0;
    REG_GPDMA_START(chan) = GPDMA_START;
}

void GPDMA_BlitMaskedFill(u32 chan, const void* mask, int fillfg, int fillbg, int fill16, void* dst, u32 dststride, u32 linelen, u32 len)
{
    if (chan > 2) return;

    GPDMA_Wait(chan);

    u32 fillw = 0;
    if (fillbg == -1)
    {
        fillw |= GPDMA_MASKED_BGTRANS;
        fillbg = 0;
    }
    else
        fillw |= GPDMA_MASKED_BGFILL;

    if (fill16)
    {
        fillw |= GPDMA_FILL_16BIT;
        fillfg &= 0xFFFF;
        fillbg &= 0xFFFF;
    }
    else
    {
        fillw |= GPDMA_FILL_8BIT;
        fillfg &= 0xFF;
        fillbg &= 0xFF;
    }

    DMA_IRQFlag[2+chan] = 0;
    REG_GPDMA_CNT(chan) = GPDMA_SRC_INCREMENT | GPDMA_MASKED_FILL | fillw;
    REG_GPDMA_LINELEN(chan) = linelen;
    REG_GPDMA_SRCSTRIDE(chan) = 0;
    REG_GPDMA_DSTSTRIDE(chan) = dststride;
    REG_GPDMA_LEN(chan) = len - 1;
    REG_GPDMA_SRCADDR(chan) = (u32)mask;
    REG_GPDMA_DSTADDR(chan) = (u32)dst;
    REG_GPDMA_FGFILL(chan) = fillfg;
    REG_GPDMA_BGFILL(chan) = fillbg;
    REG_GPDMA_START(chan) = GPDMA_START;
}

void GPDMA_Wait(u32 chan)
{
    if (chan > 2) return;

    if (REG_GPDMA_START(chan) & GPDMA_BUSY)
    {
        while (!DMA_IRQFlag[2+chan])
            WaitForIRQ();
    }

    REG_GPDMA_START(chan) = GPDMA_STOP;
}
