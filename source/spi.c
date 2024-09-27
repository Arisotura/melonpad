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

u32* irqlog = (u32*)0x200040;
void SPI_IRQHandler(int irq, void* userdata)
{
    u32 flags = REG_SPI_IRQ_STATUS;

    //*irqlog++ = flags | 0x34000000;

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
    //*irqlog++ = 0x12000000 | device | (speed<<8);
    //REG_SPI_CNT = 0x307;
    //REG_SPI_UNK14 = (REG_SPI_UNK14 & ~0x8013) | 0x8010;
    REG_SPI_DEVICE_SEL = device & 0x3;
    REG_SPI_SPEED = speed & 0x87FF;

    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_CS_MASK) | SPI_CSMODE_MANUAL | SPI_CS_SELECT;
}

void SPI_Finish()
{
    //*irqlog++ = 0x9A000000;
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_CS_MASK) | SPI_CSMODE_MANUAL | SPI_CS_RELEASE;
    //REG_SPI_CNT = (REG_SPI_CNT & ~0x702) | 0x300;
    /**(vu32*)0xF0004424 = 1;

    *(vu32*)0xF0004400 = 0x808C;
    *(vu32*)0xF00050FC = 0xC300;*/
}

void SPI_Read(u8* buf, int len)
{
    //*irqlog++ = 0x78000000;
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_DIR_MASK) | SPI_DIR_READ;
    //REG_SPI_CNT = (REG_SPI_CNT & ~0x702) | SPI_CSMODE_MANUAL | SPI_CS_SELECT | SPI_DIR_READ;

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

    REG_SPI_READ_LEN = 0;
    REG_SPI_IRQ_ENABLE &= ~SPI_IRQ_READ;
}

void SPI_Write(u8* buf, int len)
{
    //*irqlog++ = 0x56000000;
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_DIR_MASK) | SPI_DIR_WRITE;
    //REG_SPI_CNT = (REG_SPI_CNT & ~0x702) | SPI_CSMODE_MANUAL | SPI_CS_SELECT | SPI_DIR_WRITE;

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

    REG_SPI_IRQ_ENABLE &= ~SPI_IRQ_WRITE;
}

void SPI_fart()
{
    *(vu32*)0xF0004424 = 0x1;

    *(vu32*)0xF0004404 = 0x307;
    *(vu32*)0xF0004400 = (*(vu32*)0xF0004400 & ~0x87FF) | 0x808C;
    //*(vu32*)0xF0004400 = (*(vu32*)0xF0004400 & ~0x87FF) | 0x8400;
    *(vu32*)0xF0004414 = (*(vu32*)0xF0004414 & ~0x8013) | 0x8010;

    // bit9=CS  bit8=device  bit1=direction
    *(vu32*)0xF0004404 = (*(vu32*)0xF0004404 & ~0x702) | 0x100;
    *(vu32*)0xF0004400 = (*(vu32*)0xF0004400 & ~0x87FF) | 0x8400;

    SPI_IRQStatus = 0;
    *(vu32*)0xF0004418 |= 0x80;

    *(vu32*)0xF0004410 = 0x03; // command byte
    *(vu32*)0xF0004410 = 0x00;
    /**(vu32*)0xF0004410 = 0x50;
    *(vu32*)0xF0004410 = 0x00;
    *(vu32*)0xF0004410 = 0x10;*/
    *(vu32*)0xF0004410 = 0x00;
    *(vu32*)0xF0004410 = 0x00;
    *(vu32*)0xF0004410 = 0x00;

    for (;;)
    {
        while (!(SPI_IRQStatus & SPI_IRQ_WRITE))
            WaitForIRQ();

        u32 fifo = *(vu32*)0xF000440C;
        if ((fifo & 0x1F) == 0x10) break;

        SPI_IRQStatus = 0;
    }
    *(vu32*)0xF0004418 &= ~0x80;

    // read
    // 102 = normal
    // 202 = chipselect released and asserted again
    // 002 = same as 202
    // 302 = lockup
    *(vu32*)0xF0004404 = (*(vu32*)0xF0004404 & ~0x702) | 0x102;
    //minidelay();

    u8* out = (u8*)0x200000;

    SPI_IRQStatus = 0;
    *(vu32*)0xF0004418 |= 0x40;

    // initiate transfer
    *(vu32*)0xF0004420 = 0x10; // length
    while (!(SPI_IRQStatus & SPI_IRQ_READ))
        WaitForIRQ();

    //for (int i = 0; i < 0x40; )
    for (int i = 0; i < 0x10; )
    {
        // ??? maybe trigger read?
        //*(vu32*)0xF0004404 |= 0x40;
        //*(vu32*)0xF0004404 &= ~0x40;

        u8 byte = *(vu32*)0xF0004410;
        out[i++] = byte;
    }

    *(vu32*)0xF0004420 = 0;
    *(vu32*)0xF0004418 &= ~0x40;

    // release CS
    *(vu32*)0xF0004404 = (*(vu32*)0xF0004404 & ~0x702) | 0x302;
}
