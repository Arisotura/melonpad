#ifndef _AUDIOAMP_H_
#define _AUDIOAMP_H_

int AudioAmp_Init();
void AudioAmp_DeInit();

int AudioAmp_GetType();

u8 AudioAmp_ReadReg(u8 reg);
void AudioAmp_WriteReg(u8 reg, u8 val);
void AudioAmp_SetPage(u8 page);

void AudioAmp_SetVolume(int vol); // volume range: from -0x3F80 to 0x1800
void AudioAmp_SetMute(int mute);
void AudioAmp_SetOutput(int output); // 0=speakers 1=headphones

void AudioAmp_SetMicVolume(int vol); // volume range: from -0xC00 to 0x1400
void AudioAmp_SetMicPGA(int pga); // PGA range: from 0 to 0x3B80
void AudioAmp_SetMicUnk(int val); // 0/1/2

#endif // _AUDIOAMP_H_
