#ifndef _GFX_H_
#define _GFX_H_

void GFX_Init();

void GFX_WaitForVBlank();

void GFX_SetPalette(u8 offset, u32* pal, int len);
void* GFX_GetFramebuffer();

#endif // _GFX_H_
