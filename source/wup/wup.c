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

volatile u32 TickCount;
void Timer1IRQ(int irq, void* userdata);


void WUP_Init()
{
    for (int i = 0; i < 40; i++)
    {
        IRQTable[i].handler = NULL;
        IRQTable[i].userdata = NULL;
        IRQTable[i].priority = 0;
    }

    for (int i = 0; i < 32; i++)
    {
        REG_IRQ_ENABLE(i) = 0x4F;
        REG_IRQ_TRIGGER(i) = 1;
    }
    *(vu32*)0xF00019D8 = 1;
    *(vu32*)0xF00019DC = 0;
    REG_IRQ_ACK_KEY = 0;
    REG_IRQ_ACK = 0xF;

    int is41 = (WUP_HardwareType() == 0x41);
    const u32 gpioFast    = is41 ? GPIO_SAM_SLEW_FAST : GPIO_REN_SLEW_FAST;
    const u32 gpioFastAlt = gpioFast | GPIO_ALT_FUNCTION;

    // IRQ trigger type?
    // for example for IRQ 15/16 it needs to be 1 for them to fire repeatedly
    const u8 irqtrig[40] = {
        1, 1, 1,                // 00..02
        5, 5, 5, 5, 5,          // 03..07
        1, 1, 1, 1, 1, 1, 1,    // 08..0E
        5, 5, 5, 5, 5, 5,       // 0F..14
        1, 1,                   // 15..16
        5, 5, 5, 5, 5, 5, 5,    // 17..1D
        1, 1,                   // 1E..1F
        1, 1, 1, 1, 1, 1, 1, 1  // 20..27
    };

    for (int i = 0; i < 40; i++)
        REG_IRQ_TRIGGER(i) = irqtrig[i];
    for (int i = 0; i < 32; i++)
        REG_IRQ_ENABLE(i) = 0x4A;
    for (int i = 32; i < 40; i++)
        REG_IRQ_ENABLE(i) = 0x60; // ???

    // GPIO setup
    REG_GPIO_UNK2C = 0;
    REG_GPIO_UNK38 = 0;
    REG_GPIO_AUDIO_WCLK = 0;
    REG_GPIO_AUDIO_BCLK = 0;
    REG_GPIO_AUDIO_MIC = 0;
    REG_GPIO_UNK48 = 0;
    REG_GPIO_UNK4C = 0;
    REG_GPIO_UNK50 = 0;
    REG_GPIO_UNK54 = 0;
    REG_GPIO_UNK58 = 0;
    REG_GPIO_UNK5C = 0;
    REG_GPIO_UNK60 = 0;
    REG_GPIO_UNK64 = 0;
    REG_GPIO_UNK68 = 0;
    REG_GPIO_UNK6C = 0;
    REG_GPIO_UNK70 = 0;
    REG_GPIO_UNK74 = 0;
    REG_GPIO_UNK78 = 0;
    REG_GPIO_UNK80 = 0;
    REG_GPIO_UNK84 = 0;
    REG_GPIO_UNK88 = 0;
    REG_GPIO_UNK8C = 0;
    REG_GPIO_UNK90 = 0;
    REG_GPIO_UNK94 = 0;
    REG_GPIO_UNK98 = GPIO_SLEW_FAST_AND_UNK;
    REG_GPIO_I2C_SCL = 0;
    REG_GPIO_I2C_SDA = 0;
    REG_GPIO_SDIO_CLOCK = 0;
    REG_GPIO_SDIO_CMD = 0;
    REG_GPIO_SDIO_DAT0 = 0;
    REG_GPIO_SDIO_DAT1 = 0;
    REG_GPIO_SDIO_DAT2 = 0;
    REG_GPIO_SDIO_DAT3 = 0;
    REG_GPIO_UNKC4 = 0;
    REG_GPIO_UNKC8 = 0;
    REG_GPIO_UNKCC = 0;
    REG_GPIO_UNKD0 = 0;
    REG_GPIO_UNKD4 = 0;
    REG_GPIO_UNKD8 = 0;
    REG_GPIO_UNKDC = 0;
    REG_GPIO_UNKE0 = 0;
    REG_GPIO_UNKE4 = 0;
    REG_GPIO_UNKE8 = 0;
    REG_GPIO_SPI_CLOCK = 0;
    REG_GPIO_SPI_MISO = 0;
    REG_GPIO_SPI_MOSI = 0;
    REG_GPIO_SPI_CS0 = GPIO_SLEW_FAST_AND_UNK | GPIO_OUTPUT_HIGH;
    REG_GPIO_SPI_CS1 = GPIO_SLEW_FAST_AND_UNK | GPIO_OUTPUT_HIGH;
    REG_GPIO_LCD_RESET = GPIO_SLEW_FAST_AND_UNK | GPIO_OUTPUT_LOW;
    REG_GPIO_SPI_CS2 = GPIO_SLEW_FAST_AND_UNK | GPIO_OUTPUT_HIGH;
    REG_GPIO_UNK108 = gpioFast;
    REG_GPIO_UNK10C = GPIO_SLEW_FAST_AND_UNK | GPIO_INPUT_MODE | GPIO_UNK12;
    REG_GPIO_UNK110 = GPIO_SLEW_FAST_AND_UNK | GPIO_OUTPUT_MODE | GPIO_UNK12 | GPIO_UNK13;
    REG_GPIO_RUMBLE = gpioFast;
    REG_GPIO_SENSOR_BAR = gpioFast;
    REG_GPIO_CAMERA = gpioFast;

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
    REG_GPIO_SPI_CLOCK = gpioFastAlt;
    REG_GPIO_SPI_MISO = GPIO_ALT_FUNCTION;
    REG_GPIO_SPI_MOSI = gpioFastAlt;
    REG_SPI_CLOCK = SPI_CLK_48MHZ;
    if (is41)
    {
        REG_GPIO_SDIO_CLOCK = GPIO_ALT_FUNCTION | GPIO_SLEW_FAST_AND_UNK | GPIO_UNK16;
        REG_GPIO_SDIO_CMD   = GPIO_ALT_FUNCTION | GPIO_SAM_UNK;
        REG_GPIO_SDIO_DAT0  = GPIO_ALT_FUNCTION | GPIO_SAM_UNK;
        REG_GPIO_SDIO_DAT1  = GPIO_ALT_FUNCTION | GPIO_SAM_UNK;
        REG_GPIO_SDIO_DAT2  = GPIO_ALT_FUNCTION | GPIO_SAM_UNK;
        REG_GPIO_SDIO_DAT3  = GPIO_ALT_FUNCTION | GPIO_SAM_UNK;
    }
    else
    {
        REG_GPIO_SDIO_CLOCK = GPIO_ALT_FUNCTION | GPIO_SLEW_FAST_AND_UNK;
        REG_GPIO_SDIO_CMD   = GPIO_ALT_FUNCTION | GPIO_SLEW_FAST_AND_UNK;
        REG_GPIO_SDIO_DAT0  = GPIO_ALT_FUNCTION | GPIO_SLEW_FAST_AND_UNK;
        REG_GPIO_SDIO_DAT1  = GPIO_ALT_FUNCTION | GPIO_SLEW_FAST_AND_UNK;
        REG_GPIO_SDIO_DAT2  = GPIO_ALT_FUNCTION | GPIO_SLEW_FAST_AND_UNK;
        REG_GPIO_SDIO_DAT3  = GPIO_ALT_FUNCTION | GPIO_SLEW_FAST_AND_UNK;
    }
    REG_GPIO_UNKD4 = gpioFastAlt;
    REG_GPIO_UNKD8 = GPIO_ALT_FUNCTION;
    REG_GPIO_I2C_SCL = gpioFastAlt;
    REG_GPIO_I2C_SDA = gpioFastAlt;
    REG_GPIO_UNK2C = gpioFast;
    REG_GPIO_UNK4C = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK50 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK54 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK58 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK5C = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK60 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK64 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK68 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK6C = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK70 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK74 = gpioFastAlt;
    REG_GPIO_UNK78 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNKDC = gpioFastAlt;
    REG_GPIO_UNKE0 = gpioFastAlt;
    REG_GPIO_UNKE4 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNKE8 = GPIO_ALT_FUNCTION;
    REG_GPIO_UNK48 = gpioFast;
    REG_GPIO_UNK38 = gpioFast;
    REG_GPIO_AUDIO_WCLK = GPIO_ALT_FUNCTION;
    REG_GPIO_AUDIO_BCLK = GPIO_ALT_FUNCTION;
    REG_GPIO_AUDIO_MIC = GPIO_ALT_FUNCTION;

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

    Video_Init();
    LCD_Init();

    AudioAmp_Init();
    Audio_Init();

    Input_Init();

    // setup rumble GPIO (checkme)
    REG_GPIO_RUMBLE = GPIO_SLEW_FAST_AND_UNK | GPIO_OUTPUT_LOW;

    UIC_WaitWifiReady();
    SDIO_Init();
    Wifi_Init();
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
