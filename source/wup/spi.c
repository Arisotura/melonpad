#include "wup.h"

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

extern volatile int irqnum;
void SPI_Read(u8* buf, int len)
{
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_DIR_MASK) | SPI_DIR_READ;

    SPI_IRQStatus &= ~SPI_IRQ_READ;
    REG_SPI_IRQ_ENABLE |= SPI_IRQ_READ;

    for (int i = 0; i < len; )
    {
        int chunk = 16;
        if ((i + chunk) > len)
            chunk = len - i;

        // initiate a read
        REG_SPI_READ_LEN = chunk;

        while (!(SPI_IRQStatus & SPI_IRQ_READ))
            WaitForIRQ();

        SPI_IRQStatus &= ~SPI_IRQ_READ;

        // read out the data from the FIFO
        while ((SPI_READ_FIFO_LVL > 0) && (i < len))
            buf[i++] = REG_SPI_DATA;
    }

    /*REG_SPI_READ_LEN = len;

    irqnum = 0;
    *(vu32*)0xF0004044 = (2<<1); // must be 2
    *(vu32*)0xF0004048 = 0; //??
    *(vu32*)0xF000404C = 0; //??
    *(vu32*)0xF0004050 = len - 1;
    *(vu32*)0xF0004054 = (u32)buf;
    *(vu32*)0xF0004040 = 1;

    while (!(SPI_IRQStatus & SPI_IRQ_READ))
        WaitForIRQ();

    //while (*(vu32*)0xF0004040 & 1);
    while (!irqnum) WaitForIRQ();

    *(vu32*)0xF0004040 = 2;*/

    REG_SPI_READ_LEN = 0;
    REG_SPI_IRQ_ENABLE &= ~SPI_IRQ_READ;
}

void SPI_Write(u8* buf, int len)
{
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_DIR_MASK) | SPI_DIR_WRITE;

    SPI_IRQStatus &= ~SPI_IRQ_WRITE;
    REG_SPI_IRQ_ENABLE |= SPI_IRQ_WRITE;

    for (int i = 0; i < len; i++)
    {
        REG_SPI_DATA = buf[i];

        // if we filled the FIFO entirely, wait for it to be transferred
        if (SPI_WRITE_FIFO_LVL == 0)
        {
            while (!(SPI_IRQStatus & SPI_IRQ_WRITE))
                WaitForIRQ();

            SPI_IRQStatus &= ~SPI_IRQ_WRITE;
        }
    }

    // wait for any leftover contents to be transferred
    if (SPI_WRITE_FIFO_LVL < 16)
    {
        while (!(SPI_IRQStatus & SPI_IRQ_WRITE))
            WaitForIRQ();
    }

    /*irqnum = 0;
    *(vu32*)0xF0004044 = 1 | (2<<1); // must be 2
    *(vu32*)0xF0004048 = 0; //??
    *(vu32*)0xF000404C = 0; //??
    *(vu32*)0xF0004050 = len - 1;
    *(vu32*)0xF0004054 = (u32)buf;
    *(vu32*)0xF0004040 = 1;

    //while (*(vu32*)0xF0004040 & 1);
    while (!irqnum) WaitForIRQ();

    while (!(SPI_IRQStatus & SPI_IRQ_WRITE))
        WaitForIRQ();

    *(vu32*)0xF0004040 = 2;*/

    REG_SPI_IRQ_ENABLE &= ~SPI_IRQ_WRITE;
}
