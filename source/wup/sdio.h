#ifndef _SDIO_H_
#define _SDIO_H_

#define SDIO_CLOCK_FORCE_ALP        (1<<0)
#define SDIO_CLOCK_FORCE_HT         (1<<1)
#define SDIO_CLOCK_FORCE_ILP        (1<<2)
#define SDIO_CLOCK_REQ_ALP          (1<<3)
#define SDIO_CLOCK_REQ_HT           (1<<4)
#define SDIO_CLOCK_FORCE_HWREQ_OFF  (1<<5)
#define SDIO_CLOCK_ALP_AVAIL        (1<<6)
#define SDIO_CLOCK_HT_AVAIL         (1<<7)

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
