#ifndef _SPI_H_
#define _SPI_H_

void SPI_Init();

void SPI_Lock();
void SPI_Unlock();

void SPI_Start(u32 device, u32 clock);
void SPI_Finish();
void SPI_Read(void* buf, int len);
void SPI_Write(void* buf, int len);

#endif // _SPI_H_
