#ifndef _REGS_H_
#define _REGS_H_

// --- General registers ------------------------------------------------------

#define REG_HARDWARE_ID         *(vu32*)0xF0000000
#define REG_CPU_RESET           *(vu32*)0xF0000004
#define REG_UNK08               *(vu32*)0xF0000008
#define REG_PLL_INPUTDIV        *(vu32*)0xF000000C
#define REG_PLL_MULTIPLIER      *(vu32*)0xF0000010
#define REG_PLL_PRIM_DIV        *(vu32*)0xF0000014
#define REG_PLL_SEC1_DIV        *(vu32*)0xF0000018
#define REG_PLL_SEC2_DIV        *(vu32*)0xF000001C
#define REG_PLL_SEC3_DIV        *(vu32*)0xF0000020
#define REG_PLL_UNK24           *(vu32*)0xF0000024
#define REG_PLL_UNK28           *(vu32*)0xF0000028
#define REG_PLL_UNK2C           *(vu32*)0xF000002C
#define REG_UNK30               *(vu32*)0xF0000030
#define REG_CLK_PIXEL           *(vu32*)0xF0000034
#define REG_CLK_CAMERA          *(vu32*)0xF0000038
#define REG_CLK_AUDIOAMP        *(vu32*)0xF000003C
#define REG_CLK_UNK40           *(vu32*)0xF0000040
#define REG_CLK_UNK44           *(vu32*)0xF0000044
#define REG_CLK_UNK48           *(vu32*)0xF0000048
#define REG_CLK_I2C             *(vu32*)0xF000004C
#define REG_CLK_UNK50           *(vu32*)0xF0000050
#define REG_CLK_SDIO            *(vu32*)0xF0000054
#define REG_HARDWARE_RESET      *(vu32*)0xF0000058
#define REG_UNK64               *(vu32*)0xF0000064

#define PLL_DIV_2               0x005
#define PLL_DIV_2_5             0x101
#define PLL_DIV_N(n)            ((n) & 0x1FE)

// REG_CLK_xxx settings
#define CLK_SOURCE(i)           ((i) << 8)
#define CLK_DIVIDER(i)          (((i)-1) & 0xFF)

#define CLKSRC_32MHZ            0
#define CLKSRC_CAMERACLK        1
#define CLKSRC_UNK2             2
#define CLKSRC_AUDIO_BCLK       3
#define CLKSRC_PLL_PRIM         4
#define CLKSRC_PLL_SEC1         5
#define CLKSRC_PLL_SEC2         6
#define CLKSRC_PLL_SEC3         7

// REG_HARDWARE_RESET settings
#define RESET_IRQ               (1<<0)
#define RESET_TIMERS            (1<<1)
#define RESET_RAM               (1<<2)
#define RESET_SDIO              (1<<6)
#define RESET_UART              (1<<8)
#define RESET_I2C               (1<<13)
#define RESET_AUDIO             (1<<15)
#define RESET_VIDEO             (1<<21)


// --- IRQ --------------------------------------------------------------------

#define REG_IRQ_ENABLEMASK0     *(vu32*)0xF0001200
#define REG_IRQ_ENABLEMASK1     *(vu32*)0xF0001204
#define REG_IRQ_ENABLE(i)       *(vu32*)(0xF0001208 + ((i)<<2))
#define REG_IRQ_TRIGGER(i)      *(vu32*)(0xF0001420 + ((i)<<2))
#define REG_IRQ_CURRENT         *(vu32*)0xF00013F0
#define REG_IRQ_ACK             *(vu32*)0xF00013F8
#define REG_IRQ_ACK_KEY         *(vu32*)0xF00013FC

#define IRQ_TIMER0              0x00
#define IRQ_TIMER1              0x01
#define IRQ_SDIO                0x02
#define IRQ_SPI                 0x06
#define IRQ_SPI_UNK             0x07
#define IRQ_SPDMA0              0x08
#define IRQ_SPDMA1              0x09
#define IRQ_GPDMA2              0x0C
#define IRQ_GPDMA0              0x0D
#define IRQ_GPDMA1              0x0E
#define IRQ_I2C                 0x0F
#define IRQ_VBLANK_END          0x15
#define IRQ_VBLANK              0x16
#define IRQ_AUDIO               0x17
#define IRQ_MIC                 0x18
#define IRQ_VMATCH              0x1E


// --- Timers -----------------------------------------------------------------

#define REG_TIMER_PRESCALER     *(vu32*)0xF0000400
#define REG_COUNTUP_PRESCALER   *(vu32*)0xF0000404
#define REG_COUNTUP_VALUE       *(vu32*)0xF0000408
#define REG_TIMER_CNT(i)        *(vu32*)(0xF0000410 + ((i)*0x10))
#define REG_TIMER_VALUE(i)      *(vu32*)(0xF0000414 + ((i)*0x10))
#define REG_TIMER_TARGET(i)     *(vu32*)(0xF0000418 + ((i)*0x10))

// REG_TIMER_CNT settings
#define TIMER_UNK0              (1<<0)
#define TIMER_ENABLE            (1<<1)
#define TIMER_COUNT_UP          (0<<2)
#define TIMER_COUNT_DOWN        (1<<2)
#define TIMER_DIV_2             (0<<4)
#define TIMER_DIV_4             (1<<4)
#define TIMER_DIV_8             (2<<4)
#define TIMER_DIV_16            (3<<4)
#define TIMER_DIV_32            (4<<4)
#define TIMER_DIV_64            (5<<4)
#define TIMER_DIV_128           (6<<4)
#define TIMER_DIV_256           (7<<4)


// --- DMA --------------------------------------------------------------------

#define REG_DMA_CNT             *(vu32*)0xF0004000

// DMA channels:
// SPDMA: Specific Purpose DMA (from peripheral to memory, and vice versa)
// GPDMA: General Purpose DMA (from memory to memory)

#define REG_SPDMA_START(i)      *(vu32*)(0xF0004040 + ((i)*0x20))
#define REG_SPDMA_CNT(i)        *(vu32*)(0xF0004044 + ((i)*0x20))
#define REG_SPDMA_CHUNKLEN(i)   *(vu32*)(0xF0004048 + ((i)*0x20))
#define REG_SPDMA_MEMSTRIDE(i)  *(vu32*)(0xF000404C + ((i)*0x20))
#define REG_SPDMA_LEN(i)        *(vu32*)(0xF0004050 + ((i)*0x20))
#define REG_SPDMA_MEMADDR(i)    *(vu32*)(0xF0004054 + ((i)*0x20))

// REG_SPDMA_START settings
#define SPDMA_START             (1<<0)
#define SPDMA_STOP              (1<<1)
#define SPDMA_BUSY              (1<<0)

// REG_SPDMA_CNT settings
#define SPDMA_DIR_READ          (0<<0)
#define SPDMA_DIR_WRITE         (1<<0)
#define SPDMA_PERI_SPI          (2<<1)
#define SPDMA_PERI_IR           (6<<1)

#define REG_GPDMA_START(i)      *(vu32*)(0xF0004100 + ((i)*0x40))
#define REG_GPDMA_CNT(i)        *(vu32*)(0xF0004104 + ((i)*0x40))
#define REG_GPDMA_LINELEN(i)    *(vu32*)(0xF0004108 + ((i)*0x40))
#define REG_GPDMA_SRCSTRIDE(i)  *(vu32*)(0xF000410C + ((i)*0x40))
#define REG_GPDMA_DSTSTRIDE(i)  *(vu32*)(0xF0004110 + ((i)*0x40))
#define REG_GPDMA_LEN(i)        *(vu32*)(0xF0004114 + ((i)*0x40))
#define REG_GPDMA_SRCADDR(i)    *(vu32*)(0xF0004118 + ((i)*0x40))
#define REG_GPDMA_DSTADDR(i)    *(vu32*)(0xF000411C + ((i)*0x40))
#define REG_GPDMA_FGFILL(i)     *(vu32*)(0xF0004120 + ((i)*0x40))
#define REG_GPDMA_BGFILL(i)     *(vu32*)(0xF0004124 + ((i)*0x40))

// REG_GPDMA_START settings
#define GPDMA_START             (1<<0)
#define GPDMA_STOP              (1<<1)
#define GPDMA_BUSY              (1<<0)

// REG_GPDMA_CNT settings
#define GPDMA_REVERSE_16B       (1<<0)
#define GPDMA_REVERSE_8B        (1<<1)
#define GPDMA_SRC_DECREMENT     (3<<2)
#define GPDMA_SRC_INCREMENT     (3<<4)
#define GPDMA_FILL_8BIT         (0<<6)
#define GPDMA_FILL_16BIT        (1<<6)
#define GPDMA_MASKED_FILL       (1<<7)
#define GPDMA_MASKED_BGFILL     (0<<8)
#define GPDMA_MASKED_BGTRANS    (1<<8)
#define GPDMA_MASKED_REVERSE    (1<<9)
#define GPDMA_SIMPLE_FILL       (1<<10)


// --- SPI --------------------------------------------------------------------

#define REG_SPI_CLOCK           *(vu32*)0xF0004400
#define REG_SPI_CNT             *(vu32*)0xF0004404
#define REG_SPI_IRQ_STATUS      *(vu32*)0xF0004408
#define REG_SPI_FIFO_LVL        *(vu32*)0xF000440C
#define REG_SPI_DATA            *(vu32*)0xF0004410
#define REG_SPI_UNK14           *(vu32*)0xF0004414
#define REG_SPI_IRQ_ENABLE      *(vu32*)0xF0004418
#define REG_SPI_READ_LEN        *(vu32*)0xF0004420
#define REG_SPI_DEVICE_SEL      *(vu32*)0xF0004424

// REG_SPI_CLOCK settings
#define SPI_CLK_ENABLE          (1<<15)
#define SPI_CLK_SOURCE(i)       (i)
#define SPI_CLK_DIVIDER(i)      ((((i)-1) & 0xFF) << 3)

// Common REG_SPI_CLOCK settings for FLASH and UIC
#define SPI_CLK_48MHZ           (SPI_CLK_ENABLE | SPI_CLK_SOURCE(CLKSRC_PLL_PRIM) | SPI_CLK_DIVIDER(18))
#define SPI_CLK_8MHZ            (SPI_CLK_ENABLE | SPI_CLK_SOURCE(CLKSRC_32MHZ)    | SPI_CLK_DIVIDER(4))
#define SPI_CLK_248KHZ          (SPI_CLK_ENABLE | SPI_CLK_SOURCE(CLKSRC_32MHZ)    | SPI_CLK_DIVIDER(129))

// REG_SPI_CNT settings
#define SPI_DIR_READ            (1<<1)
#define SPI_DIR_WRITE           (0<<1)
#define SPI_DIR_MASK            (1<<1)
#define SPI_CSMODE_AUTO         (0<<8)
#define SPI_CSMODE_MANUAL       (1<<8)
#define SPI_CS_SELECT           (0<<9)
#define SPI_CS_RELEASE          (1<<9)
#define SPI_CS_MASK             (3<<8)

// SPI IRQ bits
// Not yet sure if there are more.
// REG_SPI_IRQ_ENABLE has more settings, but they are unknown.
#define SPI_IRQ_READ            (1<<6)
#define SPI_IRQ_WRITE           (1<<7)

// REG_SPI_FIFO_LVL macros
// * for the read FIFO, the level is the occupied space
// * for the write FIFO, the level is the free space
#define SPI_READ_FIFO_LVL       ((REG_SPI_FIFO_LVL >> 8) & 0x1F)
#define SPI_WRITE_FIFO_LVL      (REG_SPI_FIFO_LVL & 0x1F)

// REG_SPI_DEVICE_SEL settings
#define SPI_DEVICE_FLASH        (1<<0)
#define SPI_DEVICE_UIC          (1<<1)


// --- GPIO -------------------------------------------------------------------

#define REG_GPIO_UNK2C          *(vu32*)0xF000502C
#define REG_GPIO_UNK38          *(vu32*)0xF0005038
#define REG_GPIO_AUDIO_WCLK     *(vu32*)0xF000503C
#define REG_GPIO_AUDIO_BCLK     *(vu32*)0xF0005040
#define REG_GPIO_AUDIO_MIC      *(vu32*)0xF0005044
#define REG_GPIO_UNK48          *(vu32*)0xF0005048
#define REG_GPIO_UNK4C          *(vu32*)0xF000504C
#define REG_GPIO_UNK50          *(vu32*)0xF0005050
#define REG_GPIO_UNK54          *(vu32*)0xF0005054
#define REG_GPIO_UNK58          *(vu32*)0xF0005058
#define REG_GPIO_UNK5C          *(vu32*)0xF000505C
#define REG_GPIO_UNK60          *(vu32*)0xF0005060
#define REG_GPIO_UNK64          *(vu32*)0xF0005064
#define REG_GPIO_UNK68          *(vu32*)0xF0005068
#define REG_GPIO_UNK6C          *(vu32*)0xF000506C
#define REG_GPIO_UNK70          *(vu32*)0xF0005070
#define REG_GPIO_UNK74          *(vu32*)0xF0005074
#define REG_GPIO_UNK78          *(vu32*)0xF0005078
#define REG_GPIO_UNK80          *(vu32*)0xF0005080
#define REG_GPIO_UNK84          *(vu32*)0xF0005084
#define REG_GPIO_UNK88          *(vu32*)0xF0005088
#define REG_GPIO_UNK8C          *(vu32*)0xF000508C
#define REG_GPIO_UNK90          *(vu32*)0xF0005090
#define REG_GPIO_UNK94          *(vu32*)0xF0005094
#define REG_GPIO_UNK98          *(vu32*)0xF0005098
#define REG_GPIO_I2C_SCL        *(vu32*)0xF000509C
#define REG_GPIO_I2C_SDA        *(vu32*)0xF00050A0
#define REG_GPIO_SDIO_CLOCK     *(vu32*)0xF00050AC
#define REG_GPIO_SDIO_CMD       *(vu32*)0xF00050B0
#define REG_GPIO_SDIO_DAT0      *(vu32*)0xF00050B4
#define REG_GPIO_SDIO_DAT1      *(vu32*)0xF00050B8
#define REG_GPIO_SDIO_DAT2      *(vu32*)0xF00050BC
#define REG_GPIO_SDIO_DAT3      *(vu32*)0xF00050C0
#define REG_GPIO_UNKC4          *(vu32*)0xF00050C4
#define REG_GPIO_UNKC8          *(vu32*)0xF00050C8
#define REG_GPIO_UNKCC          *(vu32*)0xF00050CC
#define REG_GPIO_UNKD0          *(vu32*)0xF00050D0
#define REG_GPIO_UNKD4          *(vu32*)0xF00050D4
#define REG_GPIO_UNKD8          *(vu32*)0xF00050D8
#define REG_GPIO_UNKDC          *(vu32*)0xF00050DC
#define REG_GPIO_UNKE0          *(vu32*)0xF00050E0
#define REG_GPIO_UNKE4          *(vu32*)0xF00050E4
#define REG_GPIO_UNKE8          *(vu32*)0xF00050E8
#define REG_GPIO_SPI_CLOCK      *(vu32*)0xF00050EC
#define REG_GPIO_SPI_MISO       *(vu32*)0xF00050F0
#define REG_GPIO_SPI_MOSI       *(vu32*)0xF00050F4
#define REG_GPIO_SPI_CS0        *(vu32*)0xF00050F8 // FLASH
#define REG_GPIO_SPI_CS1        *(vu32*)0xF00050FC // UIC
#define REG_GPIO_LCD_RESET      *(vu32*)0xF0005100
#define REG_GPIO_SPI_CS2        *(vu32*)0xF0005104 // NFC
#define REG_GPIO_UNK108         *(vu32*)0xF0005108 // used in bootloader
#define REG_GPIO_UNK10C         *(vu32*)0xF000510C // NFC related
#define REG_GPIO_UNK110         *(vu32*)0xF0005110 // NFC related
#define REG_GPIO_RUMBLE         *(vu32*)0xF0005114
#define REG_GPIO_SENSOR_BAR     *(vu32*)0xF0005118
#define REG_GPIO_CAMERA         *(vu32*)0xF000511C

// REG_GPIO_xxxx defines
// not all registers seem to have all these bits
#define GPIO_ALT_FUNCTION       (1<<0)
#define GPIO_OUTPUT_MODE        (1<<9)
#define GPIO_INPUT_MODE         (1<<11)

#define GPIO_OUTPUT_LOW         (GPIO_OUTPUT_MODE | (0<<8))
#define GPIO_OUTPUT_HIGH        (GPIO_OUTPUT_MODE | (1<<8))

#define GPIO_GET_INPUT_VAL(reg)     (((reg) >> 10) & 1)
#define GPIO_SET_OUTPUT_LOW(reg)    ((reg) &= ~(1<<8))
#define GPIO_SET_OUTPUT_HIGH(reg)   ((reg) |= (1<<8))
#define GPIO_TOGGLE_OUTPUT(reg)     ((reg) ^= (1<<8))
#define GPIO_SET_OUTPUT_VAL(reg, x) \
    do { if (x) GPIO_SET_OUTPUT_HIGH(reg); else GPIO_SET_OUTPUT_LOW(reg); } while(0)

// firmware tries to set these bits, but they don't seem to actually exist
// (no idea about bit16)
#define GPIO_UNK12              (1<<12)
#define GPIO_UNK13              (1<<13)
#define GPIO_UNK16              (1<<16)

// these bits are different for the Samsung SoC (REG_HARDWARE_ID LSB == 0x41)
#define GPIO_REN_UNK            (1<<14)
#define GPIO_REN_SLEW_FAST      (1<<15)

#define GPIO_SAM_SLEW_FAST      (1<<14)
#define GPIO_SAM_UNK            (1<<15)

#define GPIO_SLEW_FAST_AND_UNK  ((1<<14)|(1<<15))


// --- Audio ------------------------------------------------------------------

#define REG_AUDIO_CNT           *(vu32*)0xF0005400
#define REG_AUDIO_UNK04         *(vu32*)0xF0005404
#define REG_AUDIO_BUF_START     *(vu32*)0xF0005408
#define REG_AUDIO_BUF_END       *(vu32*)0xF000540C
#define REG_AUDIO_PLAY_END      *(vu32*)0xF0005410
#define REG_AUDIO_PLAY_POS      *(vu32*)0xF0005414
#define REG_AUDIO_UNK18         *(vu32*)0xF0005418
#define REG_AUDIO_UNK1C         *(vu32*)0xF000541C
#define REG_AUDIO_PLAY_CNT      *(vu32*)0xF0005420
#define REG_AUDIO_ALERT_COUNT   *(vu32*)0xF0005424
#define REG_AUDIO_PLAY_COUNT    *(vu32*)0xF0005428
#define REG_AUDIO_IRQ_ENABLE    *(vu32*)0xF000542C
#define REG_AUDIO_IRQ_STATUS    *(vu32*)0xF0005430
#define REG_AUDIO_UNK34         *(vu32*)0xF0005434
#define REG_AUDIO_SAMP_CNT      *(vu32*)0xF00054B8

// REG_AUDIO_CNT settings
#define AUDIO_ENABLE            (1<<0)
#define AUDIO_CHAN_LR           (0<<5)
#define AUDIO_CHAN_RL           (1<<5)
#define AUDIO_I2S_OFFSET(x)     (((x)&0x1F)<<6)
#define AUDIO_I2S_WIDTH(x)      (((x)&0x1F)<<11)
#define AUDIO_UNK19             (1<<19)
#define AUDIO_RATE_HALF         (1<<22) // halve incoming sample rate
#define AUDIO_FMT_PCM16         (0<<23)
#define AUDIO_FMT_PCM8          (1<<23)
#define AUDIO_PCM8_MULAW        (0<<24)
#define AUDIO_PCM8_ALAW         (1<<24)

// REG_AUDIO_PLAY_CNT settings
#define PLAYCNT_MUTE            (1<<0)
#define PLAYCNT_MUTE2           (1<<1)
#define PLAYCNT_STOP            (1<<2)

// Audio IRQ bits
#define AUDIO_IRQ_MUTE          (1<<0)
#define AUDIO_IRQ_MUTE2         (1<<1)
#define AUDIO_IRQ_STOP          (1<<2)
#define AUDIO_IRQ_ALERT         (1<<3)
#define AUDIO_IRQ_UNMUTE        (1<<4)
#define AUDIO_IRQ_UNMUTE2       (1<<5)


#define REG_MIC_CNT             *(vu32*)0xF0005444
#define REG_MIC_ALERT_COUNT     *(vu32*)0xF0005448
#define REG_MIC_BUF_START       *(vu32*)0xF00054A0
#define REG_MIC_BUF_END         *(vu32*)0xF00054A4
#define REG_MIC_REC_END         *(vu32*)0xF00054A8
#define REG_MIC_REC_COUNT_1     *(vu32*)0xF00054AC
#define REG_MIC_REC_COUNT_2     *(vu32*)0xF00054B0
#define REG_MIC_IRQ_STATUS      *(vu32*)0xF00054C0
#define REG_MIC_IRQ_DISABLE     *(vu32*)0xF00054C4
#define REG_MIC_IRQ_ACK         *(vu32*)0xF00054C8

// REG_MIC_CNT settings
#define MIC_ENABLE              (1<<0)
#define MIC_UNK3                (1<<3)
#define MIC_UNK5                (1<<5)
#define MIC_UNK8                (1<<8)
#define MIC_RATE_HALF           (1<<12) // halve incoming sample rate
#define MIC_FMT_PCM16           (0<<13)
#define MIC_FMT_PCM8            (1<<13)
#define MIC_PCM8_MULAW          (0<<14)
#define MIC_PCM8_ALAW           (1<<14)
#define MIC_UNK15               (1<<15)
#define MIC_I2S_OFFSET(x)       (((x)&0x1F)<<16)
#define MIC_UNK30               (1<<30)
#define MIC_RESET               (1<<31)

// Mic IRQ bits
#define MIC_IRQ_ALERT           (1<<0)
#define MIC_IRQ_UNK4            (1<<4)
#define MIC_IRQ_UNK8            (1<<8)
#define MIC_IRQ_ALL             ((1<<0)|(1<<4)|(1<<8))


// --- Video ------------------------------------------------------------------

#define REG_LCD_HTIMING         *(vu32*)0xF0009400
#define REG_LCD_VTIMING         *(vu32*)0xF0009404
#define REG_LCD_UNK08           *(vu32*)0xF0009408
#define REG_LCD_UNK0C           *(vu32*)0xF000940C
#define REG_LCD_HSTART          *(vu32*)0xF0009410
#define REG_LCD_VSTART          *(vu32*)0xF0009414
#define REG_LCD_HEND            *(vu32*)0xF0009418
#define REG_LCD_VEND            *(vu32*)0xF000941C
#define REG_LCD_UNK20           *(vu32*)0xF0009420
#define REG_LCD_UNK24           *(vu32*)0xF0009424
#define REG_LCD_UNK28           *(vu32*)0xF0009428
#define REG_LCD_FB_XOFFSET      *(vu32*)0xF0009460
#define REG_LCD_FB_WIDTH        *(vu32*)0xF0009464
#define REG_LCD_FB_YOFFSET      *(vu32*)0xF0009468
#define REG_LCD_FB_HEIGHT       *(vu32*)0xF000946C
#define REG_LCD_FB_STRIDE       *(vu32*)0xF0009470
#define REG_LCD_FB_MEMADDR      *(vu32*)0xF0009474
#define REG_LCD_DISP_CNT        *(vu32*)0xF0009480
#define REG_LCD_PIXEL_FMT       *(vu32*)0xF00094B0
#define REG_LCD_UNKB4           *(vu32*)0xF00094B4
#define REG_LCD_VCOUNT          *(vu32*)0xF00094C8

// REG_LCD_DISP_CNT settings
#define DISP_UNK0               (1<<0)
#define DISP_ENABLE_OVERLAY     (1<<1) // enable overlay (framebuffer)
#define DISP_UNK2               (1<<2) // maybe enable video decoder output?
#define DISP_UNK3               (1<<3)
#define DISP_ENABLE             (1<<4) // enable display

// REG_LCD_PIXEL_FMT settings
#define PIXEL_FMT_PAL_256       (0<<0) // 8bpp paletted
#define PIXEL_FMT_PAL_16        (1<<0) // 4bpp paletted
#define PIXEL_FMT_ARGB1555      (2<<0) // 16bit
#define PIXEL_FMT_RGB565        (3<<0) // 16bit


#define REG_PALETTE_ADDR        *(vu32*)0xF0009500
#define REG_PALETTE_DATA        *(vu32*)0xF0009504
#define REG_VCOUNT_MATCH(i)     *(vu32*)(0xF0009508 + ((i)<<2))


#define REG_GAMMA_LUT(i)        *(vu32*)(0xF0009600 + ((i)<<2))
#define REG_GAMMA_MASK          *(vu32*)0xF0009684

// REG_GAMMA_MASK settings
#define GAMMA_ENABLE_RED        (1<<0)
#define GAMMA_ENABLE_BLUE       (1<<1)
#define GAMMA_ENABLE_GREEN      (1<<2)
#define GAMMA_ENABLE_ALL        (7<<0)


#define REG_UNK9700             *(vu32*)0xF0009700
#define REG_UNK9704             *(vu32*)0xF0009704
#define REG_UNK9708             *(vu32*)0xF0009708
#define REG_UNK970C             *(vu32*)0xF000970C


// --- SDIO -------------------------------------------------------------------

#define REG_SDIO_BASE           (vu32*)0xE0010000

#define REG_SD_SYSADDR          *(vu32*)0xE0010000
#define REG_SD_BLOCKSIZE        *(vu16*)0xE0010004
#define REG_SD_BLOCKCOUNT       *(vu16*)0xE0010006
#define REG_SD_ARG              *(vu32*)0xE0010008
#define REG_SD_ARG0             *(vu16*)0xE0010008
#define REG_SD_ARG1             *(vu16*)0xE001000A
#define REG_SD_TRANSFERMODE     *(vu16*)0xE001000C
#define REG_SD_COMMAND          *(vu16*)0xE001000E
#define REG_SD_RESPONSE         ((vu32*)0xE0010010)
#define REG_SD_RESPONSE0        *(vu16*)0xE0010010
#define REG_SD_RESPONSE1        *(vu16*)0xE0010012
#define REG_SD_RESPONSE2        *(vu16*)0xE0010014
#define REG_SD_RESPONSE3        *(vu16*)0xE0010016
#define REG_SD_RESPONSE4        *(vu16*)0xE0010018
#define REG_SD_RESPONSE5        *(vu16*)0xE001001A
#define REG_SD_RESPONSE6        *(vu16*)0xE001001C
#define REG_SD_RESPONSE7        *(vu16*)0xE001001E
#define REG_SD_DATAPORT32       *(vu32*)0xE0010020
#define REG_SD_DATAPORT16       *(vu16*)0xE0010020
#define REG_SD_DATAPORT8        *(vu8 *)0xE0010020
#define REG_SD_PRESENTSTATE     *(vu32*)0xE0010024
#define REG_SD_HOSTCNT          *(vu8 *)0xE0010028
#define REG_SD_POWERCNT         *(vu8 *)0xE0010029
#define REG_SD_BLOCKGAPCNT      *(vu8 *)0xE001002A
#define REG_SD_WAKEUPCNT        *(vu8 *)0xE001002B
#define REG_SD_CLOCKCNT         *(vu16*)0xE001002C
#define REG_SD_TIMEOUTCNT       *(vu8 *)0xE001002E
#define REG_SD_SOFTRESET        *(vu8 *)0xE001002F
#define REG_SD_IRQSTATUS        *(vu16*)0xE0010030
#define REG_SD_EIRQSTATUS       *(vu16*)0xE0010032
#define REG_SD_IRQSTATUSENABLE  *(vu16*)0xE0010034
#define REG_SD_EIRQSTATUSENABLE *(vu16*)0xE0010036
#define REG_SD_IRQSIGNALENABLE  *(vu16*)0xE0010038
#define REG_SD_EIRQSIGNALENABLE *(vu16*)0xE001003A
#define REG_SD_CMD12ERROR       *(vu32*)0xE001003C
#define REG_SD_CAPS             *(vu32*)0xE0010040
#define REG_SD_CAP_RSVD         *(vu32*)0xE0010044
#define REG_SD_MAXCURCAP        *(vu32*)0xE0010048
#define REG_SD_MAXCURCAP_RSVD   *(vu32*)0xE001004C
#define REG_SD_SLOTIRQSTATUS    *(vu16*)0xE00100FC
#define REG_SD_CNTVERSION       *(vu16*)0xE00100FE

#define SD_CMD_RESP_NONE        (0<<0)
#define SD_CMD_RESP_136         (1<<0)
#define SD_CMD_RESP_48          (2<<0)
#define SD_CMD_RESP_48_BUSY     (3<<0)
#define SD_CMD_CRC_ENABLE       (1<<3)
#define SD_CMD_INDEX_ENABLE     (1<<4)
#define SD_CMD_DATA_ENABLE      (1<<5)
#define SD_CMD_TYPE_NORMAL      (0<<6)
#define SD_CMD_TYPE_SUSPEND     (1<<6)
#define SD_CMD_TYPE_RESUME      (2<<6)
#define SD_CMD_TYPE_ABORT       (3<<6)
#define SD_CMD_NUM(v)           ((v)<<8)

#define SD_PRES_CMD_INHIBIT     (1<<0)
#define SD_PRES_DAT_INHIBIT     (1<<1)
#define SD_PRES_DAT_BUSY        (1<<2)
#define SD_PRES_WRITE_ACTIVE    (1<<8)
#define SD_PRES_READ_ACTIVE     (1<<9)
#define SD_PRES_WRITE_READY     (1<<10)
#define SD_PRES_READ_READY      (1<<11)
#define SD_PRES_CARD_PRESENT    (1<<16)
#define SD_PRES_CARD_STABLE     (1<<17)
#define SD_PRES_CARD_PRES_RAW   (1<<18)
#define SD_PRES_WRITE_ENABLE    (1<<19)

#define SD_POWER_BUS_ENABLE     (1<<0)
#define SD_POWER_VOLTS(v)       ((v)<<1)

#define SD_RESET_ALL            (1<<0)
#define SD_RESET_CMD            (1<<1)
#define SD_RESET_DAT            (1<<2)

#define SD_IRQ_CMD_DONE         (1<<0)
#define SD_IRQ_TRANSFER_DONE    (1<<1)
#define SD_IRQ_BLOCK_GAP        (1<<2)
#define SD_IRQ_DMA              (1<<3)
#define SD_IRQ_WRITE_READY      (1<<4)
#define SD_IRQ_READ_READY       (1<<5)
#define SD_IRQ_CARD_INSERT      (1<<6)
#define SD_IRQ_CARD_REMOVE      (1<<7)
#define SD_IRQ_CARD_IRQ         (1<<8)
#define SD_IRQ_ERROR            (1<<15)

#define SD_EIRQ_CMD_TIMEOUT     (1<<0)
#define SD_EIRQ_CMD_CRC         (1<<1)
#define SD_EIRQ_CMD_ENDBIT      (1<<2)
#define SD_EIRQ_CMD_INDEX       (1<<3)
#define SD_EIRQ_DATA_TIMEOUT    (1<<4)
#define SD_EIRQ_DATA_CRC        (1<<5)
#define SD_EIRQ_DATA_ENDBIT     (1<<6)
#define SD_EIRQ_CURRENT_LIMIT   (1<<7)
#define SD_EIRQ_AUTO_CMD12      (1<<8)

#define SD_CAP_ADMA2            (1<<19)
#define SD_CAP_HISPEED          (1<<21)
#define SD_CAP_DMA              (1<<22)
#define SD_CAP_SUSPEND          (1<<23)
#define SD_CAP_33V              (1<<24)
#define SD_CAP_30V              (1<<25)
#define SD_CAP_18V              (1<<26)
#define SD_CAP_64BIT            (1<<28)

#endif // _REGS_H_
