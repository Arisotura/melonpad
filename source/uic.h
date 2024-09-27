#ifndef _UIC_H_
#define _UIC_H_

void UIC_Write(u8* data, int length);
u8 UIC_WriteAndWait(u8* data, int length);
void UIC_WriteAndRead(u8* data, int length, u8* out, int outlen);
u8 UIC_Init();

void UIC_ReadEEPROM(u32 offset, u8* data, int length);
void UIC_SetBacklight(int enable);

#endif
