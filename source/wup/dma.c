#include <wup/wup.h>


static void* IRQEvent;
static void DMA_IRQHandler(int irq, void* userdata);


void DMA_Init()
{
    // enable DMA
    REG_DMA_CNT |= 0x8001;
    REG_DMA_CNT &= ~0xFC;

    IRQEvent = EventMask_Create();
    WUP_SetIRQHandler(IRQ_SPDMA0, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_SPDMA1, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_GPDMA0, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_GPDMA1, DMA_IRQHandler, NULL, 0);
    WUP_SetIRQHandler(IRQ_GPDMA2, DMA_IRQHandler, NULL, 0);
}


static void DMA_IRQHandler(int irq, void* userdata)
{
    u32 mask;
    switch (irq)
    {
    case IRQ_SPDMA0: mask = (1<<0); REG_SPDMA_START(0) = SPDMA_STOP; break;
    case IRQ_SPDMA1: mask = (1<<1); REG_SPDMA_START(1) = SPDMA_STOP; break;
    case IRQ_GPDMA0: mask = (1<<2); REG_GPDMA_START(0) = SPDMA_STOP; break;
    case IRQ_GPDMA1: mask = (1<<3); REG_GPDMA_START(1) = SPDMA_STOP; break;
    case IRQ_GPDMA2: mask = (1<<4); REG_GPDMA_START(2) = SPDMA_STOP; break;
    default: return;
    }

    EventMask_Signal(IRQEvent, mask);
}


void SPDMA_Transfer(u32 chan, const void* data, u32 peri, u32 dir, u32 len)
{
    if (chan > 1) return;

    SPDMA_Wait(chan);

    EventMask_Clear(IRQEvent, (1 << chan));
    REG_SPDMA_CNT(chan) = dir | peri;
    REG_SPDMA_CHUNKLEN(chan) = 0;
    REG_SPDMA_MEMSTRIDE(chan) = 0;
    REG_SPDMA_LEN(chan) = len - 1;
    REG_SPDMA_MEMADDR(chan) = (u32)data;
    REG_SPDMA_START(chan) = SPDMA_START;
}

void SPDMA_Wait(u32 chan)
{
    if (chan > 1) return;

    if (REG_SPDMA_START(chan) & SPDMA_BUSY)
    {
        EventMask_Wait(IRQEvent, (1 << chan), NoTimeout, NULL);
    }
}


void GPDMA_Transfer(u32 chan, const void* src, void* dst, u32 len)
{
    if (chan > 2) return;

    GPDMA_Wait(chan);

    EventMask_Clear(IRQEvent, (1 << (2+chan)));
    REG_GPDMA_CNT(chan) = GPDMA_LOGIC_OP(GPDMA_OP_COPY);
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

    EventMask_Clear(IRQEvent, (1 << (2+chan)));
    REG_GPDMA_CNT(chan) = GPDMA_LOGIC_OP(GPDMA_OP_COPY) | GPDMA_SIMPLE_FILL | fillw;
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

    EventMask_Clear(IRQEvent, (1 << (2+chan)));
    REG_GPDMA_CNT(chan) = GPDMA_LOGIC_OP(GPDMA_OP_COPY);
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

    EventMask_Clear(IRQEvent, (1 << (2+chan)));
    REG_GPDMA_CNT(chan) = GPDMA_LOGIC_OP(GPDMA_OP_COPY) | GPDMA_SIMPLE_FILL | fillw;
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

    EventMask_Clear(IRQEvent, (1 << (2+chan)));
    REG_GPDMA_CNT(chan) = GPDMA_LOGIC_OP(GPDMA_OP_COPY) | GPDMA_MASKED_FILL | fillw;
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
        EventMask_Wait(IRQEvent, (1 << (2+chan)), NoTimeout, NULL);
    }
}

void GPDMA_ClearFlag(u32 chan)
{
    if (chan > 2) return;
    EventMask_Clear(IRQEvent, (1 << (2+chan)));
}

void GPDMA_WaitFlag(u32 chan)
{
    if (chan > 2) return;
    EventMask_Wait(IRQEvent, (1 << (2+chan)), NoTimeout, NULL);
}
