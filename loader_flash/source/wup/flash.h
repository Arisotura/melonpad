#ifndef _FLASH_H_
#define _FLASH_H_

int Flash_Init();

void Flash_ReadID(u8* id, int len);

void Flash_WaitForStatus(u8 mask, u8 val);

void Flash_WriteEnable();
void Flash_WriteDisable();

void Flash_Set4ByteAddr(int val);

void Flash_Read(u32 addr, void* data, int len);

int Flash_GetCodeAddr(u32 partaddr, u32* codeaddr, u32* codelen);

#endif // _FLASH_H_
