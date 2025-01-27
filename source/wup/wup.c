#include <wup/wup.h>


static void fill32(u32 a, u32 b, u32 v)
{
    u32 i;
    for (i = a; i <= b; i+=4) *(vu32*)i = v;
}

static void fill16(u32 a, u32 b, u32 v)
{
    u32 i;
    for (i = a; i <= b; i+=4) *(vu16*)i = v;
}


typedef struct
{
    fnIRQHandler handler;
    void* userdata;
    int priority;

} sIRQHandlerEntry;

sIRQHandlerEntry IRQTable[64];


volatile u8 Timer0Flag;
void Timer0IRQ(int irq, void* userdata);


void WUP_Init()
{
    for (int i = 0; i < 64; i++)
    {
        IRQTable[i].handler = NULL;
        IRQTable[i].userdata = NULL;
        IRQTable[i].priority = 0;
    }

    for (int i = 0; i < 0x20; i++)
    {
        *(vu32*)(0xF0001208+(i<<2)) = 0x4F;
        *(vu32*)(0xF0001420+(i<<2)) = 1;
    }
    *(vu32*)0xF00019D8 = 1;
    *(vu32*)0xF00019DC = 0;
    *(vu32*)0xF00013FC = 0;
    *(vu32*)0xF00013F8 = 0xF;

    u32 sgdh = *(vu32*)0xF0000000; // TODO: blarg

    // IRQ trigger type?
    // for example for IRQ 15/16 it needs to be 1 for them to fire repeatedly
    fill16(0xF0001420, 0xF0001428, 1); // 00..02
    fill16(0xF000142C, 0xF000143C, 5); // 03..07
    fill16(0xF0001440, 0xF0001458, 1); // 08..0E
    fill16(0xF000145C, 0xF0001470, 5); // 0F..14
    fill16(0xF0001474, 0xF0001478, 1); // 15..16
    fill16(0xF000147C, 0xF0001494, 5); // 17..1D
    fill16(0xF0001498, 0xF000149C, 1); // 1E..1F
    fill16(0xF0001520, 0xF000153C, 1); // 40..47??

    fill16(0xF0001208, 0xF0001284, 0x4A);
    fill16(0xF0001288, 0xF00012A4, 0x60);

    // GPIO setup
    *(vu32*)0xF000502C = 0;
    fill32(0xF0005038, 0xF0005078, 0);
    fill32(0xF0005080, 0xF0005094, 0);
    *(vu32*)0xF0005098 = 0xC000;
    *(vu32*)0xF000509C = 0;
    *(vu32*)0xF00050A0 = 0;
    fill32(0xF00050AC, 0xF00050F4, 0);
    *(vu32*)0xF00050F8 = 0xC300;
    *(vu32*)0xF00050FC = 0xC300;
    *(vu32*)0xF0005100 = 0xC200;
    *(vu32*)0xF0005104 = 0xC300;
    *(vu32*)0xF0005108 = 0x8000;
    *(vu32*)0xF000510C = 0xD800;
    *(vu32*)0xF0005110 = 0xF200;
    fill32(0xF0005114, 0xF000511C, 0x8000);

    fill32(0xF0000040, 0xF0000048, 1);
    *(vu32*)0xF0000054 = 0x411;
    *(vu32*)0xF000003C = 1;
    *(vu32*)0xF000004C = 0x4D7;
    *(vu32*)0xF0000034 = 0;
    *(vu32*)0xF0000038 = 0x447;
    *(vu32*)0xF0000050 = 1;

    // set the timers' base clock to tick every 108 cycles (ie. every microsecond)
    *(vu32*)0xF0000400 = 0x6B;
    *(vu32*)0xF0000404 = 0x6B;

    *(vu32*)0xF0000058 |= 0x003FFFF8; // address mask for something?? probably
    *(vu32*)0xF0000064 = 6;
    *(vu32*)0xF0000058 &= ~0x003FFFF8; // ok what
    *(vu32*)0xF0000030 |= 0x300;

    //
    *(vu32*)0xF00050EC = 0x8001;
    *(vu32*)0xF00050F0 = 0x0001;
    *(vu32*)0xF00050F4 = 0x8001;
    *(vu32*)0xF0004400 = 0x8018;
    fill32(0xF00050AC, 0xF00050C0, 0xC001);
    *(vu32*)0xF00050D4 = 0x8001;
    *(vu32*)0xF00050D8 = 0x0001;
    *(vu32*)0xF000509C = 0x8001;
    *(vu32*)0xF00050A0 = 0x8001;
    *(vu32*)0xF000502C = 0x8000;
    fill32(0xF000504C, 0xF0005070, 0x0001);
    *(vu32*)0xF0005074 = 0x8001;
    *(vu32*)0xF0005078 = 0x0001;
    *(vu32*)0xF00050DC = 0x8001;
    *(vu32*)0xF00050E0 = 0x8001;
    *(vu32*)0xF00050E4 = 0x0001;
    *(vu32*)0xF00050E8 = 0x0001;
    *(vu32*)0xF0005048 = 0x8000;
    *(vu32*)0xF0005038 = 0x8000;
    fill32(0xF000503C, 0xF0005044, 0x0001);

    // configure timer 1 with a 1ms interval
    u32 timerval = *(vu32*)0xF0000400;
    timerval = 108000000 / (timerval+1);
    timerval /= 1000;

    u32 target, prescaler;
    if      (!(timerval & 0xFF)) { target = timerval>>8; prescaler = 0x70; }
    else if (!(timerval & 0x7F)) { target = timerval>>7; prescaler = 0x60; }
    else if (!(timerval & 0x3F)) { target = timerval>>6; prescaler = 0x50; }
    else if (!(timerval & 0x1F)) { target = timerval>>5; prescaler = 0x40; }
    else if (!(timerval & 0x0F)) { target = timerval>>4; prescaler = 0x30; }
    else if (!(timerval & 0x07)) { target = timerval>>3; prescaler = 0x20; }
    else if (!(timerval & 0x03)) { target = timerval>>2; prescaler = 0x10; }
    else                         { target = timerval>>1; prescaler = 0x00; }

    if (target > 0) target--;

    *(vu32*)0xF0001424 = 1;
    *(vu32*)0xF0000420 = prescaler;
    *(vu32*)0xF0000428 = target;
    *(vu32*)0xF0000420 = prescaler | 0x2;

    // reset count-up timer
    *(vu32*)0xF0000408 = 0;

    EnableIRQ();

    *(vu32*)0xF0000410 = 0;
    WUP_SetIRQHandler(IRQ_TIMER0, Timer0IRQ, NULL, 0);
    Timer0Flag = 0;

    DMA_Init();
    SPI_Init();
    I2C_Init();

    Flash_Init();
    UIC_Init();

    GFX_Init();
    LCD_Init();

    AudioAmp_Init();
    Audio_Init();

    //SDIO_Init();

    Input_Init();
}


void WUP_SetIRQHandler(u8 irq, fnIRQHandler handler, void* userdata, int prio)
{
    if (irq >= 64) return;

    int irqen = DisableIRQ();

    IRQTable[irq].handler = handler;
    IRQTable[irq].userdata = userdata;
    IRQTable[irq].priority = prio;

    if (handler)
        WUP_EnableIRQ(irq);
    else
        WUP_DisableIRQ(irq);

    RestoreIRQ(irqen);
}

void WUP_EnableIRQ(u8 irq)
{
    if (irq >= 64) return;

    int irqen = DisableIRQ();

    int prio = IRQTable[irq].priority & 0xF;
    REG_IRQ_ENABLE(irq) = (REG_IRQ_ENABLE(irq) & ~0x4F) | prio;

    RestoreIRQ(irqen);
}

void WUP_DisableIRQ(u8 irq)
{
    if (irq >= 64) return;

    int irqen = DisableIRQ();

    REG_IRQ_ENABLE(irq) |= 0x40;

    RestoreIRQ(irqen);
}
u32 irqlog[16];
void IRQHandler()
{
    irqlog[12] = *(vu32*)0xF00013FC;
    irqlog[10] = *(vu32*)0xF00013F8;
    irqlog[11] = *(vu32*)0xF00019F8;
    u32 irqnum = REG_IRQ_CURRENT;
    irqlog[13] = *(vu32*)0xF00013FC;
    irqlog[7] = *(vu32*)0xF00013F8;
    irqlog[8] = *(vu32*)0xF00019F8;
    u32 ack = REG_IRQ_ACK_KEY;
    irqlog[0] = ack;
    irqlog[1] = REG_IRQ_ACK_KEY;
    irqlog[2] = *(vu32*)0xF00013F8;
    irqlog[9] = *(vu32*)0xF00019F8;
    irqlog[3] = *(vu32*)0xF00013FC;
    irqlog[4] = *(vu32*)0xF00019F8;
    irqlog[5] = *(vu32*)0xF00019FC;
    irqlog[6] = REG_IRQ_ACK_KEY;

    if (irqnum < 64)
    {
        sIRQHandlerEntry* entry = &IRQTable[irqnum];
        if (entry->handler)
            entry->handler(irqnum, entry->userdata);
    }
    else
    {
        // ???
    }

    REG_IRQ_ACK = ack;
}


void Timer0IRQ(int irq, void* userdata)
{
    Timer0Flag = 1;
}

void WUP_DelayUS(int us)
{
    // TODO: consider halving the timer prescaler, so we can actually
    // have microsecond precision here
    if (us < 2) us = 2;

    *(vu32*)0xF0000410 = 0;
    Timer0Flag = 0;
    *(vu32*)0xF0000414 = 0;
    *(vu32*)0xF0000418 = (us >> 1) - 1;
    *(vu32*)0xF0000410 = 2;

    while (!Timer0Flag)
        WaitForIRQ();

    *(vu32*)0xF0000410 = 0;
}

void WUP_DelayMS(int ms)
{
    *(vu32*)0xF0000410 = 0;
    Timer0Flag = 0;
    *(vu32*)0xF0000414 = 0;
    *(vu32*)0xF0000418 = (ms * 500) - 1;
    *(vu32*)0xF0000410 = 2;

    while (!Timer0Flag)
        WaitForIRQ();

    *(vu32*)0xF0000410 = 0;
}
