#include <stdio.h>
#include <string.h>

#include "wup.h"


u32 GetCP15Reg(int cn, int cm, int cp);


void rumble()
{
	*(vu32*)0xF0005114 |= 0x0100;
    WUP_DelayMS(250);
	*(vu32*)0xF0005114 &= ~0x0100;
    WUP_DelayMS(250);
}


#if 0
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
#endif



// FPGA debug output

void send_binary(u8* data, int len)
{
    len &= 0x3FFF;
    u16 header = len;

    u8 buf[3];
    buf[0] = 0xF2;
    buf[1] = header >> 8;
    buf[2] = header & 0xFF;

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    SPI_Write(buf, 3);
    SPI_Write(data, len);
    SPI_Finish();
}

void send_string(char* str)
{
    int len = strlen(str);

    len &= 0x3FFF;
    u16 header = 0x8000 | len;

    u8 buf[3];
    buf[0] = 0xF2;
    buf[1] = header >> 8;
    buf[2] = header & 0xFF;

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    SPI_Write(buf, 3);
    SPI_Write((u8*)str, len);
    SPI_Finish();
}

void dump_data(u8* data, int len)
{
    len &= 0x3FFF;
    u16 header = 0x4000 | len;

    u8 buf[3];
    buf[0] = 0xF2;
    buf[1] = header >> 8;
    buf[2] = header & 0xFF;

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    SPI_Write(buf, 3);
    SPI_Write(data, len);
    SPI_Finish();
}



void ExceptionHandler()
{
    send_string("EXCEPTION\n");
}


void main()
{
	u32 i;
	
	// setup GPIO
	*(vu32*)0xF0005114 = 0xC200;


	u8 buf[16];


    /*{
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
	//DrawHex(8, 8+128, GetCP15Reg(6, 0, 0));
	//DrawHex(8, 8+144, GetCP15Reg(6, 0, 0));*/
    /*for (int i = 0; i < 8; i++)
    {
        DrawHex(8, 8 + 128 + (32*i), GetCP15Reg(6, i, 0));
        //DrawHex(8, 8 + 144 + (32*i), GetCP15Reg(6, i, 1));
    }*/

    Console_OpenDefault();




    buf[0] = *(u8*)0x3FFFFC;
    //buf[1] = UIC_GetState();

    //send_string("dfgfdg\n");
    //send_binary(buf, 2);

    //*(vu32*)0x80000000 = 2;

    //printf("printf test: %08X\n", *(vu32*)0xF0000000);
    u8* derp = (u8*)malloc(200);
    /*char str[200];
    sprintf(str, "sprintf test: %08X\n", *(vu32*)0xF0000000);
    send_string(str);*/

    printf("printf test: %08X\n", *(vu32*)0xF0000000);

    ((u8*)0x200000)[0] = *(u8*)0x3FFFFC;


    /*for (int i = 0; i < 256; i++)
        ((u8*)0x200000)[i] = i;

    dump_data((u8*)0x200000, 256);*/

    /*for (u32 i = 0; i < 0x700; i+=128)
    {
        UIC_ReadEEPROM(i, (u8*)(0x200000+i), 128);
    }
    dump_data((u8*)0x200000, 0x700);*/

    /*u8* fbtest = (u8*)GFX_GetFramebuffer();

    *(vu32*)0xF0000408 = 0;
    for (int y = 0; y < 480; y++)
    {
        for (int x = 0; x < 854; x++)
        {
            u32 pixel = 0;
            *(u8*)&fbtest[(y * 854) + x] = pixel;
        }
    }
    u32 time1 = *(vu32*)0xF0000408;

    *(vu32*)0xF0000408 = 0;
    *(vu32*)0xF0004184 = 0x430;
    *(vu32*)0xF0004188 = 854;
    *(vu32*)0xF000418C = 854; // source stride
    *(vu32*)0xF0004190 = 854; // destination stride
    *(vu32*)0xF0004194 = (854*480)-1; // byte count - 1
    *(vu32*)0xF0004198 = 0;//0x200000; // source
    *(vu32*)0xF000419C = (u32)fbtest; // destination
    *(vu32*)0xF00041A0 = 0;  // fill value
    *(vu32*)0xF00041A4 = 0;     // weird
    *(vu32*)0xF0004180 |= 1;
    while (!irqnum) WaitForIRQ();
    u32 time2 = *(vu32*)0xF0000408;*/


    int frame = 0;
    int d = 0;
	for (;;)
	{
		frame++;
		if (frame >= 15) frame = 0;

        sInputData* test = Input_Scan();

        if (test->ButtonsPressed & BTN_A)
            printf("Pressed A! frame=%d\n", frame);
        if (test->ButtonsPressed & BTN_B)
            printf("Pressed B! frame=%d\n", frame);
        if (test->ButtonsPressed & BTN_UP)
            printf("Pressed UP! frame=%d,\nthis is a multi-line printf\nI like it\n", frame);
        if (test->ButtonsPressed & BTN_DOWN)
            printf("Pressed DOWN! this is going to be a really long line, the point is to test how far the console thing can go and whether it handles this correctly\n");

#if 0
        ClearRect(8+(16*8), 8+64+256, 8*8, 16*2);
        DrawHex(8+(16*8), 8+64+256, (u32)derp);
        //DrawHex(8+(16*8), 8+64+256+16, time2);

		//if (!frame)
		//if (0)
        if (!d)
		{
            d = 1;
			//u32 base = 0x003FFE00;
			//u32 base = 0xE0010000;
			//u32 base = 0xF0004100;
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
#endif
		GFX_WaitForVBlank();
	}
}



