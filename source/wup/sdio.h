#ifndef _SDIO_H_
#define _SDIO_H_

int SDIO_Init();

int SDIO_EnableClock(u16 div);
int SDIO_EnablePower();
int SDIO_SetClocks(int sdclk, int htclk);

int SDIO_SendCommand(u32 cmd, u32 arg);
void SDIO_ReadResponse(u32* resp, int len);

int SDIO_GetOCR(u32 arg, u32* resp);
int SDIO_ReadCardRegs(int func, u32 addr, int len, u8* val);
int SDIO_WriteCardRegs(int func, u32 addr, int len, u8* val);

void SDIO_SetBusWidth(int width);

int SDIO_SetF1Base(u32 addr);
int SDIO_ReadF1Memory(u32 addr, u8* data, int len);
int SDIO_WriteF1Memory(u32 addr, u8* data, int len);

#endif // _SDIO_H_
