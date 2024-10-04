#ifndef _WUP_H_
#define _WUP_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "types.h"
#include "regs.h"

#include "dma.h"
#include "spi.h"
#include "i2c.h"

#include "uic.h"
#include "lcd.h"
#include "gfx.h"

#include "console.h"
#include "input.h"

void EnableIRQ();
int DisableIRQ();
void RestoreIRQ(int irq);
void WaitForIRQ();

void DC_FlushRange(void* addr, int length);
void DC_InvalidateRange(void* addr, int length);
void DC_InvalidateAll();
void IC_InvalidateRange(void* addr, int length);
void IC_InvalidateAll();

void WUP_Init();

inline u8 WUP_HardwareType() { return REG_HARDWARE_ID & 0xFF; }

void WUP_SetIRQHandler(u8 irq, fnIRQHandler handler, void* userdata, int prio);
void WUP_EnableIRQ(u8 irq);
void WUP_DisableIRQ(u8 irq);

void WUP_DelayUS(int us);
void WUP_DelayMS(int ms);

#endif // _WUP_H_
