#ifndef _UIC_H_
#define _UIC_H_

void UIC_Init();

u8 UIC_GetFirmwareType();
void UIC_SendCommand(u8 cmd, u8* in_data, int in_len, u8* out_data, int out_len);

u32 UIC_GetFirmwareVersion();

u8 UIC_GetState();
int UIC_SetState(u8 state);

void UIC_ReadEEPROM(u32 offset, u8* data, int length);
void UIC_SetBacklight(int enable);

#endif
