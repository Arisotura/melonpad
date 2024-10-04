#ifndef _REGS_H_
#define _REGS_H_

// TODO: define all the registers here instead of using hardcoded addresses

// --- General registers ------------------------------------------------------

#define REG_HARDWARE_ID         *(vu32*)0xF0000000


// --- IRQ --------------------------------------------------------------------

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
#define IRQ_SPDMA0               0x08
#define IRQ_SPDMA1               0x09
#define IRQ_GPDMA2               0x0C
#define IRQ_GPDMA0               0x0D
#define IRQ_GPDMA1               0x0E
#define IRQ_I2C                 0x0F
#define IRQ_VBLANK_END          0x15
#define IRQ_VBLANK              0x16


// --- DMA --------------------------------------------------------------------

#define REG_DMA_CNT             *(vu32*)0xF0004000

// DMA channels:
// SPDMA: Peripheral DMA (from peripheral to memory, and vice versa)
// GPDMA: Memory DMA (from memory to memory)

#define REG_SPDMA_START(i)      *(vu32*)(0xF0004040 + ((i)*0x20))
#define REG_SPDMA_CNT(i)        *(vu32*)(0xF0004044 + ((i)*0x20))
#define REG_SPDMA_UNK08(i)      *(vu32*)(0xF0004048 + ((i)*0x20))
#define REG_SPDMA_UNK0C(i)      *(vu32*)(0xF000404C + ((i)*0x20))
#define REG_SPDMA_LEN(i)        *(vu32*)(0xF0004050 + ((i)*0x20))
#define REG_SPDMA_MEMADDR(i)    *(vu32*)(0xF0004054 + ((i)*0x20))

// REG_SPDMA_START settings
#define SPDMA_START             (1<<0)
#define SPDMA_STOP              (1<<1)

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
#define GPDMA_SIMPLE_FILL       (1<<10)


// --- SPI --------------------------------------------------------------------

#define REG_SPI_SPEED           *(vu32*)0xF0004400
#define REG_SPI_CNT             *(vu32*)0xF0004404
#define REG_SPI_IRQ_STATUS      *(vu32*)0xF0004408
#define REG_SPI_FIFO_LVL        *(vu32*)0xF000440C
#define REG_SPI_DATA            *(vu32*)0xF0004410
#define REG_SPI_UNK14           *(vu32*)0xF0004414
#define REG_SPI_IRQ_ENABLE      *(vu32*)0xF0004418
#define REG_SPI_READ_LEN        *(vu32*)0xF0004420
#define REG_SPI_DEVICE_SEL      *(vu32*)0xF0004424

// REG_SPI_SPEED settings for FLASH and UIC.
// Not yet sure how they correlate to the actual SPI clock speed.
// These settings mean 48MHz for FLASH and 248KHz for UIC.
// TODO: add the other UIC clock, 8MHz (0x8018)
#define SPI_SPEED_FLASH         0x808C
#define SPI_SPEED_UIC           0x8400

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


#endif // _REGS_H_
