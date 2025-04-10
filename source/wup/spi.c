#include <wup/wup.h>


static void* Mutex;

static void* IRQEvent;
static void SPI_IRQHandler(int irq, void* userdata);


void SPI_Init()
{
    // setup GPIO
    /**(vu32*)0xF00050EC = 0x8001;    // clock
    *(vu32*)0xF00050F0 = 0x0001;    // MISO
    *(vu32*)0xF00050F4 = 0x8001;    // MOSI*/
    *(vu32*)0xF00050F8 = 0x8001;    // FLASH CS
    *(vu32*)0xF00050FC = 0x8001;    // UIC CS

    // ???
    REG_SPI_CNT = 0x307;
    REG_SPI_UNK14 = (REG_SPI_UNK14 & ~0x8013) | 0x8010;

    Mutex = Mutex_Create();
    IRQEvent = EventMask_Create();
    WUP_SetIRQHandler(IRQ_SPI, SPI_IRQHandler, NULL, 0);
}


void SPI_Lock()
{
    Mutex_Acquire(Mutex, NoTimeout);
}

void SPI_Unlock()
{
    Mutex_Release(Mutex);
}


static void SPI_IRQHandler(int irq, void* userdata)
{
    u32 flags = REG_SPI_IRQ_STATUS;

    REG_SPI_IRQ_STATUS = flags;
    EventMask_Signal(IRQEvent, flags);
}


void SPI_Start(u32 device, u32 clock)
{
    Mutex_Acquire(Mutex, NoTimeout);

    REG_SPI_DEVICE_SEL = device & 0x3;
    REG_SPI_CLOCK = clock & 0x87FF;

    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_CS_MASK) | SPI_CSMODE_MANUAL | SPI_CS_SELECT;
}

void SPI_Finish()
{
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_CS_MASK) | SPI_CSMODE_MANUAL | SPI_CS_RELEASE;
    Mutex_Release(Mutex);
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

    EventMask_Clear(IRQEvent, SPI_IRQ_WRITE);
    REG_SPI_IRQ_ENABLE |= SPI_IRQ_WRITE;

    SPDMA_Transfer(0, buf, SPDMA_PERI_SPI, SPDMA_DIR_WRITE, len);
    SPDMA_Wait(0);

    EventMask_Wait(IRQEvent, SPI_IRQ_WRITE, NoTimeout, NULL);

    REG_SPI_IRQ_ENABLE &= ~SPI_IRQ_WRITE;
}
