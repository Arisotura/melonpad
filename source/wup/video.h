#ifndef _VIDEO_H_
#define _VIDEO_H_

void Video_Init();

void Video_WaitForVBlank();
int Video_IsVBlank();

void Video_SetDisplayEnable(int enable);

void Video_SetOvlEnable(int enable);
int Video_SetOvlFramebuffer(void* buffer, int x, int y, int width, int height, int stride, int format);
void* Video_GetOvlFramebuffer();

void Video_SetOvlPalette(u8 offset, u32* pal, int len);
void Video_GetOvlPalette(u8 offset, u32* pal, int len);

#endif // _VIDEO_H_
