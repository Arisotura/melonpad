#include "wup.h"


u32 GetCP15Reg(int cn, int cm, int cp);


void rumble()
{
	*(vu32*)0xF0005114 |= 0x0100;
    WUP_DelayMS(250);
	*(vu32*)0xF0005114 &= ~0x0100;
    WUP_DelayMS(250);
}



#include "font.h"

void DrawText(int x, int y, char* str)
{
    u8* framebuf = (u8*)GFX_GetFramebuffer();

	int sx = x;
	for (int i = 0; str[i]; i++)
	{
		char ch = str[i];
		u8* glyph = &font[ch<<4];

		for (int cy = 0; cy < 16; cy++)
		{
			u8* line = &framebuf[((y + cy) * 854) + sx];
			u8 row = *glyph++;

			for (int cx = 0; cx < 8; cx++)
			{
				if (row & 0x80)
					*line = 0xFF;

				line++;
				row <<= 1;
			}
		}

		sx += 8;
	}
}

void DrawHex(int x, int y, u32 val)
{
	char str[9];
	for (int i = 0; i < 8; i++)
	{
		u32 n = val >> 28;
		if (n < 10)
			str[i] = '0' + n;
		else
			str[i] = 'A' + (n-10);

		val <<= 4;
	}
	str[8] = '\0';
	DrawText(x, y, str);
}

void ClearRect(int x, int y, int w, int h)
{
	u8* framebuf = (u8*)GFX_GetFramebuffer();

	for (int py = y; py < y+h; py++)
	{
		for (int px = x; px < x+w; px++)
		{
			framebuf[(py*854) + px] = 0;
		}
	}
}



void main()
{
	u32 i;

	WUP_Init();
	
	// setup GPIO
	*(vu32*)0xF0005114 = 0xC200;


	u8 buf[16];


    {
        // palette -- FFrrggbb
        u32 pal[256];
        pal[0] = 0xFF000044;
        for (i = 1; i < 256; i++)
            pal[i] = 0xFFFFFFFF;

        GFX_SetPalette(0, pal, 256);

        u8* framebuf = (u8*)GFX_GetFramebuffer();
        for (int y = 0; y < 480; y++)
        {
            for (int x = 0; x < 854; x++)
            {
                u8 pixel = 0;
                framebuf[(y * 854) + x] = pixel;
            }
        }
    }


	DrawText(8, 8, "Hello world");
	DrawText(8, 8+16, "haxing the WiiU gamepad ayyy");
	DrawHex(8, 8+32, 0x12345678);
	DrawHex(8, 8+48, *(vu32*)0xF0000000);
	DrawHex(8, 8+64, GetCP15Reg(0, 0, 0));
	DrawHex(8, 8+80, GetCP15Reg(0, 0, 1));
	DrawHex(8, 8+96, GetCP15Reg(0, 0, 2));
	DrawHex(8, 8+112, GetCP15Reg(1, 0, 0));
	//DrawHex(8, 8+128, exp[0]);
	//DrawHex(8, 8+144, uic_id);

	// 00041040
	// 41069265
	// 1D152152
	// 00000000
	// 00050078

	// 000 1110 1   000 101 010 0 10   000 101 010 0 10
	// len=2 m=0 assoc=2 size=5
	// cache type = E???
	// 8 << 2 = 32 bytes line len
	// 1 << 2 = 4-way assoc
	// 200 << 5 = 4000 = 16K

	rumble();

    //*(vu32*)0xF0005108 = 0x300;


    int frame = 0;
	for (;;)
	{
		frame++;
		if (frame >= 15) frame = 0;

        sInputData* test = Input_Scan();

        ClearRect(8+(16*8), 8+64, 8*8, 16*5);
        DrawHex(8+(16*8), 8  +64, test->ButtonsDown);
        DrawHex(8+(16*8), 8*3+64, test->LeftStickX);
        DrawHex(8+(16*8), 8*5+64, test->LeftStickY);
        DrawHex(8+(16*8), 8*7+64, test->RightStickX);
        DrawHex(8+(16*8), 8*9+64, test->RightStickY);

		//if (!frame)
		if (0)
		{
			//u32 base = 0x003FFE00;
			//u32 base = 0xE0010000;
			//u32 base = 0xF00050EC;
			u32 base = 0x200000;
			//u32 base = 0x1C0000;
			ClearRect(8+(16*8), 8+64, 8*(17*4), 16*(16));
			for (int r = 0; r < 16; r++)
			{
				u32 val = *(vu32*)(base+(r*4));

				DrawHex(8+(16*8), 8+64+(r*16), val);
			}
			for (int r = 0; r < 16; r++)
			{
				u32 val = *(vu32*)(base+0x40+(r*4));

				DrawHex(8+(26*8), 8+64+(r*16), val);
			}
			for (int r = 0; r < 16; r++)
			{
				u32 val = *(vu32*)(base+0x80+(r*4));

				DrawHex(8+(36*8), 8+64+(r*16), val);
			}
			for (int r = 0; r < 16; r++)
			{
				u32 val = *(vu32*)(base+0xC0+(r*4));

				DrawHex(8+(46*8), 8+64+(r*16), val);
			}
			/*for (int r = 0; r < 16; r++)
			{
				u32 val = oldregs[r];

				DrawHex(8+(56*8), 8+64+(r*16), val);
			}*/
			//DrawHex(8+(9*8), 8+32, *(vu32*)0xF0009484);
			// 00FF0000
		}

		GFX_WaitForVBlank();
	}
}



