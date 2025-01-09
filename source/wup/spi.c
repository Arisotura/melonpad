#include <wup/wup.h>

// SPI TODO:
// * add DMA transfers

volatile u8 SPI_IRQStatus;
void SPI_IRQHandler(int irq, void* userdata);


void SPI_Init()
{
    // setup GPIO
    *(vu32*)0xF00050EC = 0x8001;    // clock
    *(vu32*)0xF00050F0 = 0x0001;    // MISO
    *(vu32*)0xF00050F4 = 0x8001;    // MOSI
    *(vu32*)0xF00050F8 = 0x8001;    // FLASH CS
    *(vu32*)0xF00050FC = 0x8001;    // UIC CS

    // ???
    REG_SPI_CNT = 0x307;
    REG_SPI_UNK14 = (REG_SPI_UNK14 & ~0x8013) | 0x8010;

    SPI_IRQStatus = 0;
    WUP_SetIRQHandler(IRQ_SPI, SPI_IRQHandler, NULL, 0);
}


void SPI_IRQHandler(int irq, void* userdata)
{
    u32 flags = REG_SPI_IRQ_STATUS;

    if (flags & SPI_IRQ_READ)
    {
        SPI_IRQStatus |= SPI_IRQ_READ;
        REG_SPI_IRQ_STATUS |= SPI_IRQ_READ;
    }

    if (flags & SPI_IRQ_WRITE)
    {
        SPI_IRQStatus |= SPI_IRQ_WRITE;
        REG_SPI_IRQ_STATUS |= SPI_IRQ_WRITE;
    }
}


void SPI_Start(u32 device, u32 speed)
{
    REG_SPI_DEVICE_SEL = device & 0x3;
    REG_SPI_SPEED = speed & 0x87FF;

    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_CS_MASK) | SPI_CSMODE_MANUAL | SPI_CS_SELECT;
}

void SPI_Finish()
{
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_CS_MASK) | SPI_CSMODE_MANUAL | SPI_CS_RELEASE;
}


void SPI_Read(u8* buf, int len)
{
    DC_FlushRange(buf, len);
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_DIR_MASK) | SPI_DIR_READ;

    REG_SPI_READ_LEN = len;
    SPDMA_Transfer(0, buf, SPDMA_PERI_SPI, SPDMA_DIR_READ, len);
    SPDMA_Wait(0);

    REG_SPI_READ_LEN = 0;
    DC_InvalidateRange(buf, len);
}

void SPI_Write(u8* buf, int len)
{
    DC_FlushRange(buf, len);
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_DIR_MASK) | SPI_DIR_WRITE;

    SPI_IRQStatus &= ~SPI_IRQ_WRITE;
    REG_SPI_IRQ_ENABLE |= SPI_IRQ_WRITE;

    SPDMA_Transfer(0, buf, SPDMA_PERI_SPI, SPDMA_DIR_WRITE, len);
    SPDMA_Wait(0);

    while (!(SPI_IRQStatus & SPI_IRQ_WRITE))
        WaitForIRQ();

    REG_SPI_IRQ_ENABLE &= ~SPI_IRQ_WRITE;
}
