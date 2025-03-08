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

volatile u32 TickCount;
void Timer1IRQ(int irq, void* userdata);

void uictest();
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
        REG_IRQ_ENABLE(i) = 0x4F;
        REG_IRQ_TRIGGER(i) = 1;
    }
    *(vu32*)0xF00019D8 = 1;
    *(vu32*)0xF00019DC = 0;
    REG_IRQ_ACK_KEY = 0;
    REG_IRQ_ACK = 0xF;

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

    REG_CLK_UNK40    = CLK_SOURCE(CLKSRC_32MHZ)    | CLK_DIVIDER(2);
    REG_CLK_UNK44    = CLK_SOURCE(CLKSRC_32MHZ)    | CLK_DIVIDER(2);
    REG_CLK_UNK48    = CLK_SOURCE(CLKSRC_32MHZ)    | CLK_DIVIDER(2);
    REG_CLK_SDIO     = CLK_SOURCE(CLKSRC_PLL_PRIM) | CLK_DIVIDER(18);   // 48 MHz
    REG_CLK_AUDIOAMP = CLK_SOURCE(CLKSRC_32MHZ)    | CLK_DIVIDER(2);
    REG_CLK_I2C      = CLK_SOURCE(CLKSRC_PLL_PRIM) | CLK_DIVIDER(216);  // 4 MHz
    REG_CLK_PIXEL    = CLK_SOURCE(CLKSRC_32MHZ)    | CLK_DIVIDER(1);
    REG_CLK_CAMERA   = CLK_SOURCE(CLKSRC_PLL_PRIM) | CLK_DIVIDER(72);   // 12 MHz
    REG_CLK_UNK50    = CLK_SOURCE(CLKSRC_32MHZ)    | CLK_DIVIDER(2);

    // set the timers' base clock to tick every 108 cycles (ie. every microsecond)
    REG_TIMER_PRESCALER = 107;
    REG_COUNTUP_PRESCALER = 107;

    // reset hardware
    REG_HARDWARE_RESET |= 0x003FFFF8;
    REG_UNK64 = 6;
    REG_HARDWARE_RESET &= ~0x003FFFF8;
    REG_UNK30 |= 0x300;

    // more GPIO setup
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
    REG_TIMER_CNT(1) = 0;
    WUP_SetIRQHandler(IRQ_TIMER1, Timer1IRQ, NULL, 0);
    TickCount = 0;

    REG_TIMER_TARGET(1) = 124;
    REG_TIMER_CNT(1) = TIMER_DIV_8 | TIMER_ENABLE;

    // reset count-up timer
    REG_COUNTUP_VALUE = 0;

    EnableIRQ();

    REG_TIMER_CNT(0) = 0;
    WUP_SetIRQHandler(IRQ_TIMER0, Timer0IRQ, NULL, 0);
    Timer0Flag = 0;

    DMA_Init();
    SPI_Init();
    I2C_Init();

    Flash_Init();
    UIC_Init();
    //uictest(); // works

    GFX_Init();
    LCD_Init();

    AudioAmp_Init();
    Audio_Init();

    //SDIO_Init();

    Input_Init();
    //uictest(); works also
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

void IRQHandler()
{
    u32 irqnum = REG_IRQ_CURRENT;
    u32 ack = REG_IRQ_ACK_KEY;

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

void Timer1IRQ(int irq, void* userdata)
{
    TickCount++;
}

void WUP_DelayUS(int us)
{
    // TODO: consider halving the timer prescaler, so we can actually
    // have microsecond precision here
    if (us < 2) us = 2;

    REG_TIMER_CNT(0) = 0;
    Timer0Flag = 0;
    REG_TIMER_VALUE(0) = 0;
    REG_TIMER_TARGET(0) = (us >> 1) - 1;
    REG_TIMER_CNT(0) = TIMER_ENABLE;

    while (!Timer0Flag)
        WaitForIRQ();

    REG_TIMER_CNT(0) = 0;
}

void WUP_DelayMS(int ms)
{
    REG_TIMER_CNT(0) = 0;
    Timer0Flag = 0;
    REG_TIMER_VALUE(0) = 0;
    REG_TIMER_TARGET(0) = (ms  * 500) - 1;
    REG_TIMER_CNT(0) = TIMER_ENABLE;

    while (!Timer0Flag)
        WaitForIRQ();

    REG_TIMER_CNT(0) = 0;
}

u32 WUP_GetTicks()
{
    return TickCount;
}
