#include <wup/wup.h>


// SDIO TODO
// * use proper defines for register names, bits, etc

void SDIO_IRQHandler(int irq, void* userdata);
void Wifi_CardIRQ();

static u32 SD_Caps;
static u32 SD_RCA;

static u8 SD_NumFuncs;
static u32 SD_CISPtr[7];

static u32 SD_F1Base;

static void* IRQEventMask;
static volatile u16 ErrorFlags;

static void* Mutex;


int SDIO_Init()
{
    SD_Caps = REG_SD_CAPS;

    IRQEventMask = EventMask_Create();
    if (!IRQEventMask) return 0;
    ErrorFlags = 0;

    Mutex = Mutex_Create();
    if (!Mutex) return 0;

    // reset host
    REG_SD_SOFTRESET = SD_RESET_ALL;
    while (REG_SD_SOFTRESET & SD_RESET_ALL);

    REG_SD_IRQSTATUS = 0x01FF;
    REG_SD_EIRQSTATUS = 0x0FFF;
    REG_SD_IRQSTATUSENABLE = 0x01FF;
    REG_SD_EIRQSTATUSENABLE = 0x0FFF;
    REG_SD_IRQSIGNALENABLE =
        SD_IRQ_CMD_DONE |
        SD_IRQ_TRANSFER_DONE |
        SD_IRQ_READ_READY |
        SD_IRQ_WRITE_READY |
        SD_IRQ_DMA;
    REG_SD_EIRQSIGNALENABLE = 0x0FFF;

    WUP_SetIRQHandler(IRQ_SDIO, SDIO_IRQHandler, NULL, 0);

    if (!SDIO_EnableClock(128)) return 0;
    if (!SDIO_EnablePower()) return 0;

    // read RCA
    if (!SDIO_SendCommand(3, 0)) return 0;
    SDIO_ReadResponse(&SD_RCA, 1);
    // TODO check lower RCA bits
    printf("RCA=%08X\n", SD_RCA);

    SD_RCA &= 0xFFFF0000;

    u32 resp;
    if (!SDIO_SendCommand(7, SD_RCA)) return 0;
    SDIO_ReadResponse(&resp, 1);
    if (resp != 0x1E00) return 0;

    // enable SD card funcs

    memset(SD_CISPtr, 0, sizeof(SD_CISPtr));
    SDIO_ReadCardRegs(0, 0x9, &SD_CISPtr[0], 3);
    for (int i = 1; i <= SD_NumFuncs; i++)
        SDIO_ReadCardRegs(0, (0x100 * i) + 0x9, &SD_CISPtr[i], 3);
    for (int i = 0; i <= SD_NumFuncs; i++)
        SD_CISPtr[i] &= 0x1FFFF;

    u8 val = 0x02; // enable F1
    if (!SDIO_WriteCardRegs(0, 0x2, &val, 1))
        return 0;

    SD_F1Base = 0;
    SDIO_SetF1Base(0x18000000);

    /*u16 chipid = 0;
    SDIO_ReadCardRegs(1, 0x0, &chipid, 2);

    // verify the chip ID
    // these are the chip IDs the stock firmware supports
    // even though the chip ID in the gamepad should be 0x4319
    if ((chipid != 0x4315) &&
        (chipid != 0x4319) &&
        (chipid != 0x4325) &&
        (chipid != 0x4329) &&
        (chipid != 0xA8E5))
    {
        printf("SDIO: unrecognized chip ID %04X\n", chipid);
        return 0;
    }

    printf("SDIO: chip ID %04X\n", chipid);*/

    for (int i = 0; i <= SD_NumFuncs; i++)
        printf("F%d: %05X\n", i, SD_CISPtr[i]);

    SDIO_SetBusWidth(4);

    u16 blocksize = 64;
    SDIO_WriteCardRegs(0, 0x110, &blocksize, 2);
    if (SD_NumFuncs > 1)
    {
        blocksize = 64;//512;
        SDIO_WriteCardRegs(0, 0x210, &blocksize, 2);
    }

    u8 fn_ints = (1<<0);
    fn_ints |= (1<<1); // F1
    if (SD_NumFuncs > 1)
        fn_ints |= (1<<2); // F2
    SDIO_WriteCardRegs(0, 0x4, &fn_ints, 1);

    if (SD_Caps & SD_CAP_HISPEED)
    {
        u8 hostcnt = REG_SD_HOSTCNT;

        u8 speedcnt = 0;
        SDIO_ReadCardRegs(0, 0x13, &speedcnt, 1);
        if (speedcnt & (1<<0))
        {
            printf("SDIO: enabling high-speed mode\n");

            speedcnt |= (1<<1); // enable hi-speed
            SDIO_WriteCardRegs(0, 0x13, &speedcnt, 1);
            SDIO_ReadCardRegs(0, 0x13, &speedcnt, 1);

            hostcnt |= (1<<2);
        }
        else
            printf("SDIO: high-speed mode not supported\n");

        REG_SD_HOSTCNT = hostcnt;
    }

    // fasterer clock
    if (!SDIO_EnableClock(1)) return 0;

    return 1;
}

void SDIO_EnableCardIRQ()
{
    REG_SD_IRQSIGNALENABLE |= SD_IRQ_CARD_IRQ;
}

void SDIO_DisableCardIRQ()
{
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;
}

void SDIO_Lock()
{
    Mutex_Acquire(Mutex, NoTimeout);
}

void SDIO_Unlock()
{
    Mutex_Release(Mutex);
}

void SDIO_IRQHandler(int irq, void* userdata)
{
    u16 irqreg = REG_SD_IRQSTATUS & ((REG_SD_IRQSTATUSENABLE & REG_SD_IRQSIGNALENABLE) | SD_IRQ_ERROR);
    if (!irqreg) return;

    if (irqreg & SD_IRQ_CARD_IRQ)
    {
        // for the card IRQ, it is required to disable it in REG_SD_IRQSTATUSENABLE
        // for the flag to go to zero
        u16 oldenable = REG_SD_IRQSTATUSENABLE;
        REG_SD_IRQSTATUSENABLE = oldenable & ~SD_IRQ_CARD_IRQ;

        Wifi_CardIRQ();

        REG_SD_IRQSTATUS = SD_IRQ_CARD_IRQ;
        REG_SD_IRQSTATUSENABLE = oldenable;
        irqreg &= ~SD_IRQ_CARD_IRQ;
    }

    if (irqreg & SD_IRQ_DMA)
    {
        // DMA hit a boundary, start it again if needed
        REG_SD_IRQSTATUS = SD_IRQ_DMA;

        if (!(irqreg & SD_IRQ_TRANSFER_DONE))
            REG_SD_SYSADDR = REG_SD_SYSADDR;

        irqreg &= ~SD_IRQ_DMA;
    }

    if (irqreg)
    {
        if (irqreg & SD_IRQ_ERROR)
        {
            ErrorFlags = REG_SD_EIRQSTATUS;
            REG_SD_EIRQSTATUS = ErrorFlags; // acknowledge it
        }
        REG_SD_IRQSTATUS = irqreg;

        EventMask_Signal(IRQEventMask, irqreg);
    }
}

int SDIO_WaitForIRQ(u16 irq)
{
    u32 timeout = 1000;
    u32 res = 0;
    if (EventMask_Wait(IRQEventMask, irq | SD_IRQ_ERROR, timeout, &res) < 1)
        return 0;
    EventMask_Clear(IRQEventMask, res);

    if (res & SD_IRQ_ERROR)
        return 0;

    return 1;
}


int SDIO_EnableClock(u16 div)
{
    u16 div_reg = ((div >> 1) & 0xFF) << 8;

    if ((REG_SD_CLOCKCNT & 0xFF04) == (div_reg | (1<<2)))
        return 1;

    Mutex_Acquire(Mutex, NoTimeout);
    REG_SD_CLOCKCNT &= ~(1<<2);

    REG_SD_CLOCKCNT = (REG_SD_CLOCKCNT & ~0xFF00) | (div_reg & 0xFF00);

    REG_SD_CLOCKCNT |= (1<<0);
    while (!(REG_SD_CLOCKCNT & (1<<1)))
        WUP_DelayUS(2);

    REG_SD_CLOCKCNT |= (1<<2);

    u8 timeout = 7;
    while (timeout && (!(div & 1)))
    {
        timeout--;
        div >>= 1;
    }

    u16 eintenable = REG_SD_EIRQSTATUSENABLE;
    REG_SD_EIRQSTATUSENABLE = eintenable & ~SD_EIRQ_DATA_TIMEOUT;
    REG_SD_TIMEOUTCNT = timeout;
    REG_SD_EIRQSTATUSENABLE = eintenable;

    WUP_DelayUS(2);
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_DisableClock()
{
    if (!(REG_SD_CLOCKCNT & (1<<2)))
        return 1;

    Mutex_Acquire(Mutex, NoTimeout);
    REG_SD_CLOCKCNT &= ~(1<<2);
    WUP_DelayUS(2);
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_EnablePower()
{
    u8 volts = 0;
    if (SD_Caps & SD_CAP_18V)
        volts = 5;
    else if (SD_Caps & SD_CAP_30V)
        volts = 6;
    else if (SD_Caps & SD_CAP_33V)
        volts = 7;

    Mutex_Acquire(Mutex, NoTimeout);
    REG_SD_POWERCNT = SD_POWER_BUS_ENABLE | SD_POWER_VOLTS(volts);
    WUP_DelayMS(250);

    u32 ocr_resp = 0;
    if (!SDIO_GetOCR(0, &ocr_resp))
    {
        printf("OCR fail\n");
        Mutex_Release(Mutex);
        return 0;
    }

    SD_NumFuncs = (ocr_resp >> 30) & 0x7;
    if (SD_NumFuncs == 0)
    {
        printf("numfuncs=%d\n", SD_NumFuncs);
        Mutex_Release(Mutex);
        return 0;
    }

    SDIO_GetOCR(0xFFF000, &ocr_resp);
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_SetClocks(int sdclk, int htclk)
{
    Mutex_Acquire(Mutex, NoTimeout);

    if (sdclk) htclk &= 0x3F;
    else       htclk = 0;

    if (sdclk)
        SDIO_EnableClock(1);

    u8 clkstat = 0;
    SDIO_ReadCardRegs(1, 0x1000E, &clkstat, 1);
    if (htclk != (clkstat & 0x3F))
    {
        u8 clkreg = htclk;
        SDIO_WriteCardRegs(1, 0x1000E, &clkreg, 1);
        WUP_DelayUS(1);

        clkreg = htclk & 0x18;
        if (clkreg == SDIO_CLOCK_REQ_ALP ||
            clkreg == SDIO_CLOCK_REQ_HT)
        {
            u8 readybit = (clkreg == SDIO_CLOCK_REQ_HT) ? SDIO_CLOCK_HT_AVAIL : SDIO_CLOCK_ALP_AVAIL;
            for (;;)
            {
                SDIO_ReadCardRegs(1, 0x1000E, &clkreg, 1);
                if (clkreg & readybit)
                    break;

                WUP_DelayUS(1);
            }
        }
    }

    if (!sdclk)
        SDIO_DisableClock();

    Mutex_Release(Mutex);
    return 1;
}

int SDIO_SendCommand(u32 cmd, u32 arg)
{
    Mutex_Acquire(Mutex, NoTimeout);
    u16 oldirq = REG_SD_IRQSIGNALENABLE;
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;

    int retries = 100;
    while (REG_SD_PRESENTSTATE & SD_PRES_CMD_INHIBIT)
    {
        retries--;
        if (retries <= 0)
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }
    }

    u16 cmdreg = SD_CMD_NUM(cmd);
    switch (cmd)
    {
    case 0: // idle
        cmdreg |= SD_CMD_RESP_NONE;
        cmdreg |= SD_CMD_TYPE_NORMAL;
        break;

    case 3: // get RCA
        cmdreg |= SD_CMD_RESP_48;
        cmdreg |= SD_CMD_TYPE_NORMAL;
        break;

    case 5: // get OCR
        cmdreg |= SD_CMD_RESP_48;
        cmdreg |= SD_CMD_TYPE_NORMAL;
        break;

    case 7: // select card
        cmdreg |= SD_CMD_RESP_48;
        cmdreg |= SD_CMD_TYPE_NORMAL;
        cmdreg |= SD_CMD_CRC_ENABLE;
        cmdreg |= SD_CMD_INDEX_ENABLE;
        break;

    case 52: // direct read/write
        cmdreg |= SD_CMD_RESP_48;
        cmdreg |= SD_CMD_TYPE_NORMAL;
        cmdreg |= SD_CMD_CRC_ENABLE;
        cmdreg |= SD_CMD_INDEX_ENABLE;
        break;

    case 53: // extended read/write
        cmdreg |= SD_CMD_RESP_48;
        cmdreg |= SD_CMD_TYPE_NORMAL;
        cmdreg |= SD_CMD_CRC_ENABLE;
        cmdreg |= SD_CMD_INDEX_ENABLE;
        cmdreg |= SD_CMD_DATA_ENABLE;
        break;

    default:
        printf("SDIO: unknown command %d\n", cmd);
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    EventMask_Clear(IRQEventMask, 0xFFFF & ~SD_IRQ_CARD_IRQ);
    REG_SD_ARG = arg;
    REG_SD_COMMAND = cmdreg;

    if (!SDIO_WaitForIRQ(SD_IRQ_CMD_DONE))
    {
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    REG_SD_IRQSIGNALENABLE = oldirq;
    Mutex_Release(Mutex);
    return 1;
}

void SDIO_ReadResponse(u32* resp, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);

    if (len > 4) len = 4;
    for (int i = 0; i < len; i++)
        resp[i] = REG_SD_RESPONSE[i];

    Mutex_Release(Mutex);
}

int SDIO_GetOCR(u32 arg, u32* resp)
{
    Mutex_Acquire(Mutex, NoTimeout);

    int retries = 200;
    for (;;)
    {
        if (!SDIO_SendCommand(5, arg))
        {
            Mutex_Release(Mutex);
            return 0;
        }

        SDIO_ReadResponse(resp, 1);
        if (resp[0] & (1<<31)) // card ready
        {
            Mutex_Release(Mutex);
            return 1;
        }

        retries--;
        if (retries <= 0)
        {
            Mutex_Release(Mutex);
            return 0;
        }
    }
}

int SDIO_ReadCardRegs(int func, u32 addr, void* data, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);
    u16 oldirq = REG_SD_IRQSIGNALENABLE;
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;

    for (int i = 0; i < len; i++)
    {
        u32 cmdarg = (0 << 31) | (func << 28) | ((addr & 0x1FFFF) << 9);
        if (!SDIO_SendCommand(52, cmdarg))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        u32 resp;
        SDIO_ReadResponse(&resp, 1);
        if ((resp & 0xFF00) != 0x1000)
            printf("SDIO_ReadCardRegs: ??? resp=%08X\n", resp);

        ((u8*)data)[i] = resp & 0xFF;
        addr++;
    }

    REG_SD_IRQSIGNALENABLE = oldirq;
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_WriteCardRegs(int func, u32 addr, void* data, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);
    u16 oldirq = REG_SD_IRQSIGNALENABLE;
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;

    for (int i = 0; i < len; i++)
    {
        u32 cmdarg = (1 << 31) | (func << 28) | ((addr & 0x1FFFF) << 9) | ((u8*)data)[i];
        if (!SDIO_SendCommand(52, cmdarg))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        u32 resp;
        SDIO_ReadResponse(&resp, 1);
        if ((resp & 0xFF00) != 0x1000)
            printf("SDIO_WriteCardRegs: ??? resp=%08X\n", resp);

        addr++;
    }

    REG_SD_IRQSIGNALENABLE = oldirq;
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_ReadCardData(int func, u32 addr, void* data, int len, int incr_addr)
{
    if (func == 0) return 0;

    Mutex_Acquire(Mutex, NoTimeout);
    u16 oldirq = REG_SD_IRQSIGNALENABLE;
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;

    int retries = 100;
    while (REG_SD_PRESENTSTATE & SD_PRES_CMD_INHIBIT)
    {
        retries--;
        if (retries <= 0)
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }
    }

    u32 cmdarg = (0 << 31) | (func << 28) | ((addr & 0x1FFFF) << 9);
    u16 xfermode = (1<<4); // read

    if (incr_addr)
        cmdarg |= (1<<26);

    int blocksize = 64;//(func == 2) ? 512 : 64;

    if (len >= blocksize)
    {
        if (len & (blocksize-1))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        //int blocknum = len >> ((func == 2) ? 9 : 6);
        int blocknum = len >> 6;
        if (blocknum > 0x200)
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        cmdarg |= (1<<27); // block mode

        REG_SD_BLOCKSIZE = blocksize | 0x7000;
        REG_SD_BLOCKCOUNT = blocknum;
        cmdarg |= (blocknum & 0x1FF);

        xfermode |= (1<<0); // DMA enable
        if (blocknum > 1)
        {
            xfermode |= (1<<5); // multiple blocks
            xfermode |= (1<<1); // block count enable
        }
    }
    else
    {
        REG_SD_BLOCKSIZE = len | 0x7000;
        REG_SD_BLOCKCOUNT = 1;
        cmdarg |= (len & 0x1FF);

        //if (len > 4)
        //    xfermode |= (1<<0);
    }

    retries = 100;
    while (REG_SD_PRESENTSTATE & SD_PRES_DAT_INHIBIT)
    {
        retries--;
        if (retries <= 0)
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }
    }

    if (xfermode & (1<<0))
    {
        DC_FlushRange(data, len);
        REG_SD_SYSADDR = (u32)data;
    }

    REG_SD_TRANSFERMODE = xfermode;

    if (!SDIO_SendCommand(53, cmdarg))
    {
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    u32 resp;
    SDIO_ReadResponse(&resp, 1);
    if ((resp & 0xFF00) != 0x1000)
    {
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    if (!(xfermode & (1<<0)))
    {
        if (!SDIO_WaitForIRQ(SD_IRQ_READ_READY))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        u8* bdata = (u8*)data;
        int words = len >> 2;
        int i;
        if (((u32)data) & 0x3)
        {
            for (i = 0; i < words; i++)
            {
                u32 val = REG_SD_DATAPORT32;
                bdata[i*4+0] = val & 0xFF;
                bdata[i*4+1] = (val >> 8) & 0xFF;
                bdata[i*4+2] = (val >> 16) & 0xFF;
                bdata[i*4+3] = val >> 24;
            }

            if (len & 2)
            {
                u16 val = REG_SD_DATAPORT16;
                bdata[i*4+0] = val & 0xFF;
                bdata[i*4+1] = val >> 8;
            }
        }
        else
        {
            for (i = 0; i < words; i++)
                ((u32*)bdata)[i] = REG_SD_DATAPORT32;

            if (len & 2)
                ((u16*)bdata)[i*2] = REG_SD_DATAPORT16;
        }
        if (len & 1)
            bdata[i*4+2] = REG_SD_DATAPORT8;
    }

    if (!SDIO_WaitForIRQ(SD_IRQ_TRANSFER_DONE))
    {
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    if (xfermode & (1<<0))
        DC_InvalidateRange(data, len);

    REG_SD_IRQSIGNALENABLE = oldirq;
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_WriteCardData(int func, u32 addr, void* data, int len, int incr_addr)
{
    if (func == 0) return 0;

    Mutex_Acquire(Mutex, NoTimeout);
    u16 oldirq = REG_SD_IRQSIGNALENABLE;
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;

    int retries = 100;
    while (REG_SD_PRESENTSTATE & SD_PRES_CMD_INHIBIT)
    {
        retries--;
        if (retries <= 0)
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }
    }

    u32 cmdarg = (1 << 31) | (func << 28) | ((addr & 0x1FFFF) << 9);
    u16 xfermode = (0<<4); // write

    if (incr_addr)
        cmdarg |= (1<<26);

    int blocksize = 64;//(func == 2) ? 512 : 64;

    if (len >= blocksize)
    {
        if (len & (blocksize-1))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        //int blocknum = len >> ((func == 2) ? 9 : 6);
        int blocknum = len >> 6;
        if (blocknum > 0x200)
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        cmdarg |= (1<<27); // block mode

        REG_SD_BLOCKSIZE = blocksize | 0x7000;
        REG_SD_BLOCKCOUNT = blocknum;
        cmdarg |= (blocknum & 0x1FF);

        xfermode |= (1<<0); // DMA enable
        if (blocknum > 1)
        {
            xfermode |= (1<<5); // multiple blocks
            xfermode |= (1<<1); // block count enable
        }
    }
    else
    {
        REG_SD_BLOCKSIZE = len | 0x7000;
        REG_SD_BLOCKCOUNT = 1;
        cmdarg |= (len & 0x1FF);

        //if (len > 4)
        //    xfermode |= (1<<0);
    }

    retries = 100;
    while (REG_SD_PRESENTSTATE & SD_PRES_DAT_INHIBIT)
    {
        retries--;
        if (retries <= 0)
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }
    }

    if (xfermode & (1<<0))
    {
        DC_FlushRange(data, len);
        REG_SD_SYSADDR = (u32)data;
    }

    REG_SD_TRANSFERMODE = xfermode;

    if (!SDIO_SendCommand(53, cmdarg))
    {
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    u32 resp;
    SDIO_ReadResponse(&resp, 1);
    if ((resp & 0xFF00) != 0x1000)
    {
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    if (!(xfermode & (1<<0)))
    {
        if (!SDIO_WaitForIRQ(SD_IRQ_WRITE_READY))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            Mutex_Release(Mutex);
            return 0;
        }

        u8* bdata = (u8*)data;
        int words = len >> 2;
        int i;
        if (((u32)data) & 0x3)
        {
            for (i = 0; i < words; i++)
            {
                u32 val;
                val = bdata[i*4+0];
                val |= (bdata[i*4+1] << 8);
                val |= (bdata[i*4+2] << 16);
                val |= (bdata[i*4+3] << 24);
                REG_SD_DATAPORT32 = val;
            }

            if (len & 2)
            {
                u16 val;
                val = bdata[i*4+0];
                val |= (bdata[i*4+1] << 8);
                REG_SD_DATAPORT16 = val;
            }
        }
        else
        {
            for (i = 0; i < words; i++)
                REG_SD_DATAPORT32 = ((u32*)bdata)[i];

            if (len & 2)
                REG_SD_DATAPORT16 = ((u16*)bdata)[i*2];
        }

        if (len & 1)
            REG_SD_DATAPORT8 = bdata[i*4+2];
    }

    if (!SDIO_WaitForIRQ(SD_IRQ_TRANSFER_DONE))
    {
        REG_SD_IRQSIGNALENABLE = oldirq;
        Mutex_Release(Mutex);
        return 0;
    }

    REG_SD_IRQSIGNALENABLE = oldirq;
    Mutex_Release(Mutex);
    return 1;
}

void SDIO_SetBusWidth(int width)
{
    u8 regval;
    Mutex_Acquire(Mutex, NoTimeout);

    SDIO_ReadCardRegs(0, 0x7, &regval, 1);
    regval &= ~3;

    if (width == 4)
        regval |= 0x2; // 4-bit bus

    SDIO_WriteCardRegs(0, 0x7, &regval, 1);

    regval = REG_SD_HOSTCNT;
    regval &= ~0x2;

    if (width == 4)
        regval |= 0x2; // 4-bit bus

    REG_SD_HOSTCNT = regval;
    Mutex_Release(Mutex);
}

int SDIO_SetF1Base(u32 addr)
{
    addr &= 0xFFFF8000;
    if (addr == SD_F1Base)
        return 1;

    Mutex_Acquire(Mutex, NoTimeout);

    u8 addrparts[3];
    addrparts[0] = (addr >> 8) & 0x80;
    addrparts[1] = (addr >> 16) & 0xFF;
    addrparts[2] = (addr >> 24) & 0xFF;
    if (!SDIO_WriteCardRegs(1, 0x1000A, addrparts, 3))
    {
        Mutex_Release(Mutex);
        return 0;
    }

    SD_F1Base = addr;
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_ReadF1Memory(u32 addr, void* data, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);
    u16 oldirq = REG_SD_IRQSIGNALENABLE;
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;

    for (int i = 0; i < len; )
    {
        int chunk = len - i;
        u32 sdaddr = addr & 0x7FFF;
        if ((sdaddr + chunk) > 0x8000)
            chunk = 0x8000 - sdaddr;
        if (chunk > 64)
            chunk &= ~0x3F;

        if (!SDIO_SetF1Base(addr))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            return 0;
        }

        if (!SDIO_ReadCardData(1, sdaddr | 0x8000, data, chunk, 1))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            return 0;
        }

        i += chunk;
        addr += chunk;
        data = (void*)(((u8*)data) + chunk);
    }

    REG_SD_IRQSIGNALENABLE = oldirq;
    Mutex_Release(Mutex);
    return 1;
}

int SDIO_WriteF1Memory(u32 addr, void* data, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);
    u16 oldirq = REG_SD_IRQSIGNALENABLE;
    REG_SD_IRQSIGNALENABLE &= ~SD_IRQ_CARD_IRQ;

    for (int i = 0; i < len; )
    {
        int chunk = len - i;
        u32 sdaddr = addr & 0x7FFF;
        if ((sdaddr + chunk) > 0x8000)
            chunk = 0x8000 - sdaddr;
        if (chunk > 64)
            chunk &= ~0x3F;

        if (!SDIO_SetF1Base(addr))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            return 0;
        }

        if (!SDIO_WriteCardData(1, sdaddr | 0x8000, data, chunk, 1))
        {
            REG_SD_IRQSIGNALENABLE = oldirq;
            return 0;
        }

        i += chunk;
        addr += chunk;
        data = (void*)(((u8*)data) + chunk);
    }

    REG_SD_IRQSIGNALENABLE = oldirq;
    Mutex_Release(Mutex);
    return 1;
}
