#include <wup/wup.h>


// function pointers to actual I2C implementations
// TODO:
// * determine if it's worth it to keep infrastructure for Samsung compatibility
//   (Samsung stuff is likely prototype hardware and not retail gamepads)
// * define I2C registers properly
// * leave out busses other than 3 since they don't work

int (*_I2C_Start)(u32 bus);
void (*_I2C_Finish)(u32 bus);
int (*_I2C_Read)(u32 bus, u32 dev, u8* buf, u32 len);
int (*_I2C_Write)(u32 bus, u32 dev, u8* buf, u32 len, u32 dontstop);
void (*_I2C_IRQ)(u32 bus);

int I2C_Renesas_Start(u32 bus);
void I2C_Renesas_Finish(u32 bus);
int I2C_Renesas_Read(u32 bus, u32 dev, u8* buf, u32 len);
int I2C_Renesas_Write(u32 bus, u32 dev, u8* buf, u32 len, u32 dontstop);
void I2C_Renesas_IRQ(u32 bus);

int I2C_Samsung_Start(u32 bus);
void I2C_Samsung_Finish(u32 bus);
int I2C_Samsung_Read(u32 bus, u32 dev, u8* buf, u32 len);
int I2C_Samsung_Write(u32 bus, u32 dev, u8* buf, u32 len, u32 dontstop);
void I2C_Samsung_IRQ(u32 bus);

void I2C_IRQHandler(int irq, void* userdata);

// bus status flags
#define STATUS_IRQ_STOP     (1<<0)
#define STATUS_IRQ_READ     (1<<1)
#define STATUS_IRQ_WRITE    (1<<2)
#define STATUS_IRQ_ALL      (STATUS_IRQ_STOP|STATUS_IRQ_READ|STATUS_IRQ_WRITE)
#define STATUS_XFER_PENDING (1<<7)

static u8 I2C_BusStatus[5];
static void* I2C_BusEvent[5];


void I2C_Init()
{
    for (int i = 0; i < 5; i++)
    {
        I2C_BusStatus[i] = 0;
        I2C_BusEvent[i] = EventMask_Create();
    }

    if (WUP_HardwareType() == 0x41)
    {
        // Samsung controller

        *(vu32*)0xF0005804 &= ~0x18;
        *(vu32*)0xF0005808 = 0x18;

        _I2C_Start = I2C_Samsung_Start;
        _I2C_Finish = I2C_Samsung_Finish;
        _I2C_Read = I2C_Samsung_Read;
        _I2C_Write = I2C_Samsung_Write;
        _I2C_IRQ = I2C_Samsung_IRQ;
    }
    else
    {
        // Renesas controller

        *(vu32*)0xF0005804 &= ~0x1F;
        *(vu32*)0xF0005904 = 0;
        *(vu32*)0xF0005D04 = 0;
        *(vu32*)0xF0006000 = 0;
        *(vu32*)0xF0006400 = 0;
        *(vu32*)0xF0006800 = 0;
        *(vu32*)0xF0005908 = 7;
        *(vu32*)0xF0005D08 = 7;
        *(vu32*)0xF0005900 = 0;
        *(vu32*)0xF0005D00 = 0;
        *(vu32*)0xF0005808 = 0x1C;

        _I2C_Start = I2C_Renesas_Start;
        _I2C_Finish = I2C_Renesas_Finish;
        _I2C_Read = I2C_Renesas_Read;
        _I2C_Write = I2C_Renesas_Write;
        _I2C_IRQ = I2C_Renesas_IRQ;
    }

    WUP_SetIRQHandler(IRQ_I2C, I2C_IRQHandler, NULL, 0);
}

int I2C_Start(u32 bus)
{
    return _I2C_Start(bus);
}

void I2C_Finish(u32 bus)
{
    return _I2C_Finish(bus);
}

int I2C_Read(u32 bus, u32 dev, u8* buf, u32 len)
{
    return _I2C_Read(bus, dev, buf, len);
}

int I2C_Write(u32 bus, u32 dev, u8* buf, u32 len, u32 dontstop)
{
    return _I2C_Write(bus, dev, buf, len, dontstop);
}

void I2C_IRQHandler(int irq, void* userdata)
{
    u32 irqflags = *(vu32*)0xF0005800;

    for (int i = 0; i < 5; i++)
    {
        if (irqflags & (1<<i))
            _I2C_IRQ(i);
    }
}


// --- RENESAS I2C ------------------------------------------------------------

void I2C_Renesas_IRQ(u32 bus)
{
    vu32* base = (vu32*)(0xF0005800 + (bus<<10));

    u32 reg18 = base[0x18>>2];
    u32 flags = 0;
    if (reg18 & (1<<0))
        flags |= STATUS_IRQ_STOP;
    else if (reg18 & (1<<3))
        flags |= STATUS_IRQ_WRITE;
    else
        flags |= STATUS_IRQ_READ;

    if (flags)
        EventMask_Signal(I2C_BusEvent[bus], flags);

    *(vu32*)0xF0005808 = (1<<bus);
}

u32 I2C_Renesas_WaitForFlag(u32 bus, u32 flag)
{
    u32 res;
    if (EventMask_Wait(I2C_BusEvent[bus], flag, NoTimeout, &res) < 1)
        return 0;

    return res & STATUS_IRQ_ALL;
}

void I2C_Renesas_ClearFlag(u32 bus, u32 mask)
{
    EventMask_Clear(I2C_BusEvent[bus], mask);
}

void I2C_Renesas_StartTransfer(u32 bus)
{
    vu32* base = (vu32*)(0xF0005800 + (bus<<10));

    if ((I2C_BusStatus[bus] & STATUS_XFER_PENDING) ||
        (!(base[0x20>>2] & 0x40)))
    {
        base[0x8>>2] |= 0x2;
        if (!(base[0x20>>2] & 0x80))
            return;
    }

    u32 f = I2C_Renesas_WaitForFlag(bus, STATUS_IRQ_STOP);
    I2C_Renesas_ClearFlag(bus, f);

    base[0x8>>2] |= 0x2;
}

void I2C_Renesas_FinishTransfer(u32 bus)
{
    if (!(I2C_BusStatus[bus] & STATUS_XFER_PENDING))
        return;

    vu32* base = (vu32*)(0xF0005800 + (bus<<10));

    u32 old5804 = *(vu32*)0xF0005804;
    if (!((*(vu32*)0xF0005804) & (1<<bus)))
        *(vu32*)0xF0005804 |= (1<<bus);

    base[0x8>>2] |= 0x1;
    I2C_Renesas_WaitForFlag(bus, STATUS_IRQ_STOP);
    I2C_Renesas_ClearFlag(bus, STATUS_IRQ_STOP);

    *(vu32*)0xF0005804 = old5804 & ~(1<<bus);
    I2C_BusStatus[bus] &= ~STATUS_XFER_PENDING;
}

void I2C_Renesas_Wait(u32 bus)
{
    vu32* base = (vu32*)(0xF0005800 + (bus<<10));

    for (;;)
    {
        if (!(base[0x8>>2] & 0x20))
            return;

        WUP_DelayMS(1);
    }
}

int I2C_Renesas_Start(u32 bus)
{
    vu32* base = (vu32*)(0xF0005800 + (bus<<10));

    base[0x10>>2] = 0xC;
    base[0x20>>2] = 0x3;
    base[0x00>>2] |= 0x1;

    for (int i = 0; i < 100; i++)
    {
        if (base[0x00>>2] & 0x1)
            return 1;

        WUP_DelayMS(1);
    }

    return 0;
}

void I2C_Renesas_Finish(u32 bus)
{
    I2C_Renesas_FinishTransfer(bus);
}

int I2C_Renesas_Read(u32 bus, u32 dev, u8* buf, u32 len)
{
    vu32* base = (vu32*)(0xF0005800 + (bus<<10));
    u32 f;

    I2C_Renesas_ClearFlag(bus, STATUS_IRQ_ALL);

    if (!(I2C_BusStatus[bus] & STATUS_XFER_PENDING))
        base[0x8>>2] |= 0x18;

    *(vu32*)0xF0005804 |= (1<<bus);
    I2C_BusStatus[bus] |= STATUS_XFER_PENDING;

    I2C_Renesas_StartTransfer(bus);

    base[0x4>>2] = (dev<<1) | 0x01;

    f = I2C_Renesas_WaitForFlag(bus, STATUS_IRQ_ALL);
    if (f & STATUS_IRQ_READ)
    {
        if (base[0x18>>2] & 0x4)
            I2C_Renesas_ClearFlag(bus, STATUS_IRQ_READ);
        else
        {
            I2C_Renesas_FinishTransfer(bus);
            return 0;
        }
    }

    base[0x8>>2] = (base[0x8>>2] & ~0x8) | 0x4;

    u32 i;
    for (i = 0; i < len;)
    {
        base[0x8>>2] |= 0x20;

        I2C_Renesas_Wait(bus);
        f = I2C_Renesas_WaitForFlag(bus, STATUS_IRQ_ALL);
        if (!(f & STATUS_IRQ_READ)) continue;
        I2C_Renesas_ClearFlag(bus, STATUS_IRQ_READ);

        buf[i++] = (u8)base[0x4>>2];
    }

    base[0x8>>2] &= ~0x4;
    base[0x8>>2] = (base[0x8>>2] & ~0x4) | 0x28;
    I2C_Renesas_Wait(bus);
    I2C_Renesas_WaitForFlag(bus, STATUS_IRQ_READ);
    I2C_Renesas_ClearFlag(bus, STATUS_IRQ_READ);

    I2C_Renesas_FinishTransfer(bus);
    return 1;
}

int I2C_Renesas_Write(u32 bus, u32 dev, u8* buf, u32 len, u32 dontstop)
{
    vu32* base = (vu32*)(0xF0005800 + (bus<<10));
    u32 f;

    I2C_Renesas_ClearFlag(bus, STATUS_IRQ_ALL);

    if (!(I2C_BusStatus[bus] & STATUS_XFER_PENDING))
        base[0x8>>2] |= 0x18;

    *(vu32*)0xF0005804 |= (1<<bus);
    I2C_BusStatus[bus] |= STATUS_XFER_PENDING;

    I2C_Renesas_StartTransfer(bus);

    base[0x4>>2] = (dev<<1);

    u32 i;
    for (i = 0; i < len;)
    {
        f = I2C_Renesas_WaitForFlag(bus, STATUS_IRQ_ALL);
        if (!(f & STATUS_IRQ_WRITE)) continue;
        if (base[0x18>>2] & 0x4)
        {
            I2C_Renesas_ClearFlag(bus, STATUS_IRQ_WRITE);
            base[0x4>>2] = buf[i++];
        }
        else
        {
            I2C_Renesas_FinishTransfer(bus);
            return 0;
        }
    }
    I2C_Renesas_WaitForFlag(bus, STATUS_IRQ_WRITE);
    I2C_Renesas_ClearFlag(bus, STATUS_IRQ_WRITE);

    if (base[0x18>>2] & 0x4)
    {
        if (!dontstop)
            I2C_Renesas_FinishTransfer(bus);

        return 1;
    }
    else
    {
        I2C_Renesas_FinishTransfer(bus);
        return 0;
    }
}


// --- SAMSUNG I2C ------------------------------------------------------------

void I2C_Samsung_IRQ(u32 bus)
{
    // TODO
}

void I2C_Samsung_Delay(u32 val)
{
    val += 0x59;
    u32 i;
    for (i = 0; i < val; i += 0x5A)
    {
        u32 royal = *(vu32*)0xF0005804;
    }
}

int I2C_Samsung_Start(u32 bus)
{
    // TODO
    return 0;
}

void I2C_Samsung_Finish(u32 bus)
{
    // TODO
}

int I2C_Samsung_Read(u32 bus, u32 dev, u8* buf, u32 len)
{
    /*vu32* base = (vu32*)(0xF0005800 + (bus<<10));

    *(vu32*)0xF0005804 |= (1<<bus);

    base[0x4>>2] = 0x10;
    base[0xC>>2] = (dev<<1);
    base[0x4>>2] = 0x32; // or Ox33. checkme
    base[0x0>>2] &= ~0x10;
    base[0x0>>2] |= 0x100;

    u32 i;
    for (i = 0; i < len; i++)
    {
        if (i == len-1)
            base[0x10>>2] = 0x4;

        base[0x0>>2] &= ~0x10;
        base[0x0>>2] |= 0x100;

        buf[i] = (u8)base[0xC>>2];
    }

    I2C_Samsung_Delay(0xDE8);
    base[0x10>>2] = 0x1;
    base[0x0>>2] &= ~0x10;
    base[0x0>>2] |= 0x100;
    I2C_Samsung_Delay(0x6D6);
    base[0x4>>2] = 0x32; // checkme
    I2C_Samsung_Delay(0x514);

    *(vu32*)0xF0005804 &= ~(1<<bus);
    return 1;*/
    return 0;
}

int I2C_Samsung_Write(u32 bus, u32 dev, u8* buf, u32 len, u32 dontstop)
{
    /*vu32* base = (vu32*)(0xF0005800 + (bus<<10));

    *(vu32*)0xF0005804 |= (1<<bus);

    base[0x4>>2] = 0x10;
    base[0xC>>2] = (dev<<1);
    base[0x4>>2] = 0x32; // or Ox33. checkme
    base[0x0>>2] &= ~0x10;
    base[0x0>>2] |= 0x100;

    u32 i;
    for (i = 0; i < len; i++)
    {
        // todo: error checking, base+04 bit0

        base[0xC>>2] = buf[i];

        I2C_Samsung_Delay(0x7D0);
        base[0x0>>2] &= ~0x10;
        base[0x0>>2] |= 0x100;
    }

    // todo: more oddities with base+04 bit0

    if (!dontstop)
    {
        I2C_Samsung_Delay(0xDE8);
        base[0x10>>2] = 0x1;
        base[0x0>>2] &= ~0x10;
        base[0x0>>2] |= 0x100;
        I2C_Samsung_Delay(0x6D6);
        base[0x4>>2] = 0x32; // checkme
        I2C_Samsung_Delay(0x514);
    }

    *(vu32*)0xF0005804 &= ~(1<<bus);
    return 1;*/
    return 0;
}
