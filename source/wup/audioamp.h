#ifndef _AUDIOAMP_H_
#define _AUDIOAMP_H_

int AudioAmp_Init();

u8 AudioAmp_ReadReg(u8 reg);
void AudioAmp_WriteReg(u8 reg, u8 val);
void AudioAmp_SetPage(u8 page);

void AudioAmp_SetVolume(int vol); // volume range: from -0x3F80 to 0x1800
void AudioAmp_SetMute(int mute);
void AudioAmp_SetOutput(int output); // 0=speakers 1=headphones

#endif // _AUDIOAMP_H_
