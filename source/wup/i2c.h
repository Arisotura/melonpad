#ifndef _I2C_H_
#define _I2C_H_

#define I2C_DEV_AUDIO_AMP   0x18
#define I2C_DEV_CAMERA      0x21
#define I2C_DEV_LCD         0x39

void I2C_Init();

int I2C_Start();
void I2C_Finish();
int I2C_Read(u32 dev, u8* buf, u32 len);
int I2C_Write(u32 dev, u8* buf, u32 len, int dontstop);

#endif // _I2C_H_
