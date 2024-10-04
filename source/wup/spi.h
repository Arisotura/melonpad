#ifndef _SPI_H_
#define _SPI_H_

void SPI_Init();

void SPI_Start(u32 device, u32 speed);
void SPI_Finish();
void SPI_Read(u8* buf, int len);
void SPI_Write(u8* buf, int len);

#endif // _SPI_H_
