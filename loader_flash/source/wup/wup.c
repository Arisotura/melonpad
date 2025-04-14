#include <wup/wup.h>


typedef struct
{
    fnIRQHandler handler;
    void* userdata;
    int priority;

} sIRQHandlerEntry;

sIRQHandlerEntry IRQTable[40];


volatile u8 Timer0Flag;
void Timer0IRQ(int irq, void* userdata);


void WUP_Init()
{
    // IRQ init

    for (int i = 0; i < 40; i++)
    {
        IRQTable[i].handler = NULL;
        IRQTable[i].userdata = NULL;
        IRQTable[i].priority = 0;
    }

    REG_IRQ_ACK = 0xF;
    *(vu8*)0xF00019D8 = 1;

    for (int i = 0x20; i < 0x28; i++) REG_IRQ_TRIGGER(i) = 1;
    for (int i = 0x00; i < 0x1F; i++) REG_IRQ_TRIGGER(i) = 1;

    for (int i = 0x20; i < 0x28; i++) REG_IRQ_ENABLE(i) = 0x40;
    for (int i = 0x00; i < 0x1F; i++) REG_IRQ_ENABLE(i) = 0x4E;

    // set the timers' base clock to tick every 108 cycles (ie. every microsecond)
    // for timers, we halve it so we get microsecond precision
    REG_TIMER_PRESCALER = 53;
    REG_COUNTUP_PRESCALER = 107;

    EnableIRQ();

    REG_TIMER_CNT(0) = 0;
    WUP_SetIRQHandler(IRQ_TIMER0, Timer0IRQ, NULL, 0);
    Timer0Flag = 0;

    DMA_Init();
    SPI_Init();

    Flash_Init();
}


void WUP_SetIRQHandler(u8 irq, fnIRQHandler handler, void* userdata, int prio)
{
    if (irq >= 40) return;

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
    if (irq >= 40) return;

    int irqen = DisableIRQ();

    int prio = IRQTable[irq].priority & 0xF;
    REG_IRQ_ENABLE(irq) = (REG_IRQ_ENABLE(irq) & ~0x4F) | prio;

    RestoreIRQ(irqen);
}

void WUP_DisableIRQ(u8 irq)
{
    if (irq >= 40) return;

    int irqen = DisableIRQ();

    REG_IRQ_ENABLE(irq) |= 0x40;

    RestoreIRQ(irqen);
}

void IRQHandler()
{
    u32 irqnum = REG_IRQ_CURRENT;
    u32 ack = REG_IRQ_ACK_KEY;

    if (irqnum < 40)
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
    REG_TIMER_CNT(0) = 0;
    Timer0Flag = 0;
    REG_TIMER_VALUE(0) = 0;
    REG_TIMER_TARGET(0) = us - 1;
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
    REG_TIMER_TARGET(0) = (ms * 1000) - 1;
    REG_TIMER_CNT(0) = TIMER_ENABLE;

    while (!Timer0Flag)
        WaitForIRQ();

    REG_TIMER_CNT(0) = 0;
}
