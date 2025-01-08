#ifndef _AUDIO_H_
#define _AUDIO_H_

int Audio_Init();

void Audio_SetVolume(u8 vol);
void Audio_SetMute(int mute);
void Audio_SetOutput(int output);

#endif // _AUDIO_H_
