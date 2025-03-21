#ifndef _AUDIO_H_
#define _AUDIO_H_

#define AUDIO_FORMAT_PCM16          0
#define AUDIO_FORMAT_PCM8_MULAW     1
#define AUDIO_FORMAT_PCM8_ALAW      2

#define AUDIO_FREQ_48KHZ            0
#define AUDIO_FREQ_24KHZ            1

typedef void (*fnSampleCb)();
typedef void (*fnStreamCb)(void* buffer, int length);

int Audio_Init();

void Audio_SetVolume(u8 vol);
void Audio_SetMute(int mute);
void Audio_SetOutput(int output);
void Audio_SetChanOrder(int order);

// audio buffers/lengths must be aligned to a 16-byte boundary
// lengths are in bytes

int Audio_PlaySample(void* buffer, int length, int format, int freq, int chans, fnSampleCb callback);
int Audio_StartStream(void* buffer, int length, int format, int freq, int chans, fnStreamCb callback);
void Audio_Stop();

//

#endif // _AUDIO_H_
