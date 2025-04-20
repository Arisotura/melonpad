#include <wup/wup.h>


void I2C_IRQHandler(void* userdata);

// bus status flags
#define STATUS_IRQ_STOP     (1<<0)
#define STATUS_IRQ_READ     (1<<1)
#define STATUS_IRQ_WRITE    (1<<2)
#define STATUS_IRQ_ALL      (STATUS_IRQ_STOP|STATUS_IRQ_READ|STATUS_IRQ_WRITE)
#define STATUS_XFER_PENDING (1<<7)

static u8 I2C_BusStatus[5];
static void* I2C_BusEvent[5];

static void* I2C_Mutex[5];


void I2C_Init()
{
    for (int i = 0; i < 5; i++)
    {
        I2C_BusStatus[i] = 0;
        I2C_BusEvent[i] = EventMask_Create();
        I2C_Mutex[i] = Mutex_Create();
    }

    REG_I2C_IRQ_ENABLE &= ~0x1F;
    REG_I2C_S_UNK04(0) = 0;
    REG_I2C_S_UNK04(1) = 0;
    REG_I2C_M_UNK00(2) = 0;
    REG_I2C_M_UNK00(3) = 0;
    REG_I2C_M_UNK00(4) = 0;
    REG_I2C_S_UNK08(0) = 7;
    REG_I2C_S_UNK08(1) = 7;
    REG_I2C_S_UNK00(0) = 0;
    REG_I2C_S_UNK00(1) = 0;
    REG_I2C_IRQ_ACK = 0x1C;

    WUP_SetIRQHandler(IRQ_I2C, I2C_IRQHandler, NULL, 0);
}


static void I2C_BusIRQ(u32 bus)
{
    if (bus < 2)
    {
        // TODO?

        REG_I2C_IRQ_ACK = (1<<bus);
        return;
    }

    u32 stat = REG_I2C_M_STAT1(bus);
    u32 flags = 0;
    if (stat & (1<<0))
        flags |= STATUS_IRQ_STOP;
    else if (stat & (1<<3))
        flags |= STATUS_IRQ_WRITE;
    else
        flags |= STATUS_IRQ_READ;

    if (flags)
        EventMask_Signal(I2C_BusEvent[bus], flags);

    REG_I2C_IRQ_ACK = (1<<bus);
}

void I2C_IRQHandler(void* userdata)
{
    u32 irqflags = REG_I2C_IRQ_STATUS;

    for (int i = 0; i < 5; i++)
    {
        if (irqflags & (1<<i))
            I2C_BusIRQ(i);
    }
}


static u32 I2C_WaitForFlag(int bus, u32 flag)
{
    u32 res;
    if (EventMask_Wait(I2C_BusEvent[bus], flag, NoTimeout, &res) < 1)
        return 0;

    return res & STATUS_IRQ_ALL;
}

static void I2C_ClearFlag(int bus, u32 mask)
{
    EventMask_Clear(I2C_BusEvent[bus], mask);
}


static void I2C_SendStart(int bus)
{
    I2C_ClearFlag(bus, STATUS_IRQ_ALL);

    if (!(I2C_BusStatus[bus] & STATUS_XFER_PENDING))
        REG_I2C_M_CNT(bus) |= (I2C_CNT_TRX_ENABLE | I2C_CNT_DIR_TX);

    REG_I2C_IRQ_ENABLE |= (1<<bus);
    I2C_BusStatus[bus] |= STATUS_XFER_PENDING;

    if ((I2C_BusStatus[bus] & STATUS_XFER_PENDING) ||
        (!(REG_I2C_M_STAT2(bus) & I2C_STAT2_BUSY)))
    {
        REG_I2C_M_CNT(bus) |= I2C_CNT_START;
        if (!(REG_I2C_M_STAT2(bus) & I2C_STAT2_START))
            return;
    }

    u32 f = I2C_WaitForFlag(bus, STATUS_IRQ_STOP);
    I2C_ClearFlag(bus, f);

    REG_I2C_M_CNT(bus) |= I2C_CNT_START;
}

static void I2C_SendStop(int bus)
{
    if (!(I2C_BusStatus[bus] & STATUS_XFER_PENDING))
        return;

    u32 oldirq = REG_I2C_IRQ_ENABLE;
    if (!(REG_I2C_IRQ_ENABLE & (1<<bus)))
        REG_I2C_IRQ_ENABLE |= (1<<bus);

    REG_I2C_M_CNT(bus) |= I2C_CNT_STOP;
    I2C_WaitForFlag(bus, STATUS_IRQ_STOP);
    I2C_ClearFlag(bus, STATUS_IRQ_STOP);

    REG_I2C_IRQ_ENABLE = oldirq & ~(1<<bus);
    I2C_BusStatus[bus] &= ~STATUS_XFER_PENDING;
}


int I2C_Start()
{
    const int bus = 3;
    Mutex_Acquire(I2C_Mutex[bus], NoTimeout);

    REG_I2C_M_UNK10(bus) = 0xC;
    REG_I2C_M_STAT2(bus) = 0x3;
    REG_I2C_M_UNK00(bus) |= 0x1;

    for (int i = 0; i < 100; i++)
    {
        if (REG_I2C_M_UNK00(bus) & 0x1)
            return 1;

        WUP_DelayUS(10);
    }

    return 0;
}

void I2C_Finish()
{
    const int bus = 3;
    I2C_SendStop(bus);
    Mutex_Release(I2C_Mutex[bus]);
}

static void I2C_WaitRX(int bus)
{
    for (;;)
    {
        if (!(REG_I2C_M_CNT(bus) & I2C_CNT_REQ_RX))
            return;

        WUP_DelayUS(10);
    }
}

int I2C_Read(u32 dev, u8* buf, u32 len)
{
    const int bus = 3;
    u32 f;

    I2C_SendStart(bus);

    REG_I2C_M_DATA(bus) = (dev<<1) | 0x01;

    f = I2C_WaitForFlag(bus, STATUS_IRQ_ALL);
    if (f & STATUS_IRQ_READ)
    {
        if (REG_I2C_M_STAT1(bus) & I2C_STAT1_ACK)
            I2C_ClearFlag(bus, STATUS_IRQ_READ);
        else
        {
            I2C_SendStop(bus);
            return 0;
        }
    }

    REG_I2C_M_CNT(bus) = (REG_I2C_M_CNT(bus) & ~I2C_CNT_DIR_TX) | I2C_CNT_ACK;

    for (int i = 0; i < len;)
    {
        REG_I2C_M_CNT(bus) |= I2C_CNT_REQ_RX;

        I2C_WaitRX(bus);
        f = I2C_WaitForFlag(bus, STATUS_IRQ_ALL);
        if (!(f & STATUS_IRQ_READ)) continue;
        I2C_ClearFlag(bus, STATUS_IRQ_READ);

        buf[i++] = (u8)REG_I2C_M_DATA(bus);
    }

    REG_I2C_M_CNT(bus) &= ~I2C_CNT_ACK;
    REG_I2C_M_CNT(bus) = (REG_I2C_M_CNT(bus) & ~I2C_CNT_ACK) | I2C_CNT_REQ_RX | I2C_CNT_DIR_TX;
    I2C_WaitRX(bus);
    I2C_WaitForFlag(bus, STATUS_IRQ_READ);
    I2C_ClearFlag(bus, STATUS_IRQ_READ);

    I2C_SendStop(bus);
    return 1;
}

int I2C_Write(u32 dev, u8* buf, u32 len, int dontstop)
{
    const int bus = 3;
    u32 f;

    I2C_SendStart(bus);

    REG_I2C_M_DATA(bus) = (dev<<1);

    for (int i = 0; i < len;)
    {
        f = I2C_WaitForFlag(bus, STATUS_IRQ_ALL);
        if (!(f & STATUS_IRQ_WRITE)) continue;
        if (REG_I2C_M_STAT1(bus) & I2C_STAT1_ACK)
        {
            I2C_ClearFlag(bus, STATUS_IRQ_WRITE);
            REG_I2C_M_DATA(bus) = buf[i++];
        }
        else
        {
            I2C_SendStop(bus);
            return 0;
        }
    }
    I2C_WaitForFlag(bus, STATUS_IRQ_WRITE);
    I2C_ClearFlag(bus, STATUS_IRQ_WRITE);

    if (REG_I2C_M_STAT1(bus) & I2C_STAT1_ACK)
    {
        if (!dontstop)
            I2C_SendStop(bus);

        return 1;
    }
    else
    {
        I2C_SendStop(bus);
        return 0;
    }
}
