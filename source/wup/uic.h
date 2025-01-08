#ifndef _UIC_H_
#define _UIC_H_

u16 CRC16(u8* data, u32 len);
int CheckCRC16(u8* data, u32 len);

void UIC_Init();

u8 UIC_GetFirmwareType();
void UIC_SendCommand(u8 cmd, u8* in_data, int in_len, u8* out_data, int out_len);

u32 UIC_GetFirmwareVersion();

u8 UIC_GetState();
int UIC_SetState(u8 state);

void UIC_GetInputData(u8* data);

void UIC_WriteEnable();
void UIC_WriteDisable();
int UIC_ReadEEPROM(u32 offset, u8* data, int length);
int UIC_WriteEEPROM(u32 offset, u8* data, int length);

void UIC_SetBacklight(int enable);

#endif
