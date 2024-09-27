#ifndef _I2C_H_
#define _I2C_H_

#define I2C_BUS_AUDIO_AMP   3
#define I2C_DEV_AUDIO_AMP   0x18
#define I2C_BUS_CAMERA      3
#define I2C_DEV_CAMERA      0x21
#define I2C_BUS_LCD         3
#define I2C_DEV_LCD         0x39

void I2C_Init();

int I2C_Start(u32 bus);
void I2C_Finish(u32 bus);
int I2C_Read(u32 bus, u32 dev, u8* buf, u32 len);
int I2C_Write(u32 bus, u32 dev, u8* buf, u32 len, u32 dontstop);

#endif // _I2C_H_
