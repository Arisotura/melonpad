#ifndef _DMA_H_
#define _DMA_H_

void DMA_Init();


// SPDMA: for transferring from/to peripherals

void SPDMA_Transfer(u32 chan, const void* data, u32 peri, u32 dir, u32 len);
void SPDMA_Wait(u32 chan);


// GPDMA: for transferring from memory to memory

void GPDMA_Transfer(u32 chan, const void* src, void* dst, u32 len);
void GPDMA_Fill(u32 chan, int fillval, int fill16, void* dst, u32 len);

void GPDMA_BlitTransfer(u32 chan,
                        const void* src, u32 srcstride,
                        void* dst, u32 dststride,
                        u32 linelen, u32 len);

void GPDMA_BlitFill(u32 chan,
                    int fillval, int fill16,
                    void* dst, u32 dststride,
                    u32 linelen, u32 len);

void GPDMA_BlitMaskedFill(u32 chan,
                          const void* mask, int fillfg, int fillbg, int fill16,
                          void* dst, u32 dststride,
                          u32 linelen, u32 len);

void GPDMA_Wait(u32 chan);

#endif // _DMA_H_
