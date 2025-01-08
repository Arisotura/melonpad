#include <stdio.h>
#include <string.h>

#include <wup/wup.h>


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

    SPI_Start(SPI_DEVICE_FLASH, 0x8400);//SPI_SPEED_FLASH);
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

    SPI_Start(SPI_DEVICE_FLASH, 0x8400);//SPI_SPEED_FLASH);
    SPI_Write(buf, 3);
    SPI_Write(data, len);
    SPI_Finish();
}



void ExceptionHandler()
{
    send_string("EXCEPTION\n");
}


void flash_testbench()
{
    u8* derp = (u8*)malloc(200);
    /*char str[200];
    sprintf(str, "sprintf test: %08X\n", *(vu32*)0xF0000000);
    send_string(str);*/

    printf("printf test: %08X\n", *(vu32*)0xF0000000);

    u8* test = (u8*)malloc(0x1008);
    Flash_Read(0, test, 0x1008);
    printf("TEST BEFORE: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd[5] = {0x20, 0x00, 0x00, 0x02, 0x34};
    SPI_Write(cmd, 5);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("subsec erased\n");

    Flash_Read(0, test, 0x1008);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    for (int i = 0; i < 0x100; i++)
        test[i] = 0x40+i;
    for (int i = 0x100; i < 0x200; i++)
        test[i] = 0x77;

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd2[5] = {0x02, 0x00, 0x00, 0x00, 0x01};
    SPI_Write(cmd2, 5);
    SPI_Write(test, 0x101);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("page programmed\n");

    Flash_Read(0, test, 0x1008);
    //printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xF8], *(u32*)&test[0xFC], *(u32*)&test[0x100], *(u32*)&test[0x104]);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0x48], *(u32*)&test[0x4C], *(u32*)&test[0x78], *(u32*)&test[0x7C], *(u32*)&test[0x100], *(u32*)&test[0x104]);

    printf("now testing elsewhere\n");
    Flash_Read(0x100000, test, 0x1008);
    printf("TEST BEFORE: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd1[5] = {0x20, 0x00, 0x10, 0x05, 0x67};
    SPI_Write(cmd1, 5);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("subsec erased\n");

    Flash_Read(0x100000, test, 0x1008);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    for (int i = 0; i < 0x100; i++)
        test[i] = 0x40+i;
    for (int i = 0x100; i < 0x200; i++)
        test[i] = 0x77;

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    //u8 cmd4[5] = {0x02, 0x00, 0x00, 0x00, 0x00};
    u8 cmd4[5] = {0x02, 0x00, 0x10, 0x00, 0x00};
    SPI_Write(cmd4, 5);
    SPI_Write(test, 0x100);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("page programmed\n");

    //Flash_Read(0, test, 0x1008);
    Flash_Read(0x100000, test, 0x1008);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xF8], *(u32*)&test[0xFC], *(u32*)&test[0x100], *(u32*)&test[0x104]);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0x48], *(u32*)&test[0x4C], *(u32*)&test[0x78], *(u32*)&test[0x7C], *(u32*)&test[0x100], *(u32*)&test[0x104]);


    for (int i = 0; i < 20; i++)
    {
        *(u32*)&test[0] = (i & 0xF) * 0x11111111;

        Flash_WaitForStatus(0x03, 0x00);
        Flash_WriteEnable();

        SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
        u8 cmd4[5] = {0x02, 0x00, 0x10, 0x05, 0x00};
        SPI_Write(cmd4, 5);
        SPI_Write(test, 0x4);
        SPI_Finish();

        Flash_WaitForStatus(0x03, 0x00);

        Flash_Read(0x100500, test, 4);
        printf("%d. res=%08X\n", i, *(u32*)&test[0]);
    }
}


volatile int uartflag;
u32 proat[8];
void uart_irq(int irq, void* fart)
{
    /*uartflag = irq;
    proat[0] = *(vu32*)0xF0004C78;
    proat[1] = *(vu32*)0xF0004C48;
    proat[2] = *(vu32*)0xF0004C4C;
    WUP_DisableIRQ(4);*/
    for (;;)
    {
        u32 state = *(vu32*)0xF0004C4C & 0xF;
        if (state == 1) break;

        if (state == 2)
        {
            // idle? finished sending?
            u32 txfifo = (*(vu32*)0xF0004C78 >> 8) & 0x1F;
            if (txfifo >= 16) continue;

            *(vu32*)0xF0004C48 &= ~2;
            uartflag = 1;

            *(vu32*)0xF0004C48 |= 2;
        }
    }
}

void uart_testbench()
{
    // reset UART?
    *(vu32*)0xF0000058 |= 0x100;
    *(vu32*)0xF0000058 &= ~0x100;

    *(vu32*)0xF0004C54 = 0;
    *(vu32*)0xF0004C48 = 0;

    printf("test\n");

    *(vu32*)0xF0004C70 = 1;
    *(vu32*)0xF0004C64 = 1;
    *(vu32*)0xF0004C68 = 2;
    *(vu32*)0xF0004C6C = 0xD903;
    *(vu32*)0xF0004C48 = 0;
    *(vu32*)0xF0004C50 = 0;
    *(vu32*)0xF0004C54 = 3;
    *(vu32*)0xF0004C58 = 0;
    *(vu32*)0xF0004C48 = 0;
    *(vu32*)0xF0004C48 |= 2; // seems to enable IRQ4
    *(vu32*)0xF0004C48 |= 5; // enable shit
printf("bb\n");
    uartflag = 0;
    WUP_SetIRQHandler(4, uart_irq, NULL, 0);
    //WUP_SetIRQHandler(5, uart_irq, NULL, 0);

    printf("aa\n");
    printf("irq=%d, %08X %08X %08X\n", uartflag, proat[0], proat[1], proat[2]);
    uartflag = 0;

    printf("ctl=%08X derp=%08X\n", *(vu32*)0xF0004C48, *(vu32*)0xF0004C4C);

    *(vu32*)0xF0004C44 = 0x11;
    *(vu32*)0xF0004C44 = 0x22;
    *(vu32*)0xF0004C44 = 0x33;
    u32 farp = *(vu32*)0xF0004C78;
    printf("farp=%08X\n", farp);
    printf("ctl=%08X derp=%08X\n", *(vu32*)0xF0004C48, *(vu32*)0xF0004C4C);

    // values for F0004C78:
    // 00000300
    // 00000202 ??
    // 00000201
    // 00000101
    // 00000001

    // F0004C4C:
    // 2 when not sending, 1 when sending?
    // when F0004C48 is 7 -- otherwise it's always 1?

    //*(vu32*)0xF0004C48 = 5;
    while ((*(vu32*)0xF0004C78 >> 8) == 3);
    farp = *(vu32*)0xF0004C78;
    printf("farp=%08X\n", farp);
    printf("ctl=%08X derp=%08X\n", *(vu32*)0xF0004C48, *(vu32*)0xF0004C4C);

    while ((*(vu32*)0xF0004C78 >> 8) == 2);
    farp = *(vu32*)0xF0004C78;
    printf("farp=%08X\n", farp);
    printf("ctl=%08X derp=%08X\n", *(vu32*)0xF0004C48, *(vu32*)0xF0004C4C);

    while ((*(vu32*)0xF0004C78 >> 8) != 0);
    farp = *(vu32*)0xF0004C78;
    printf("farp=%08X\n", farp);
    printf("ctl=%08X derp=%08X\n", *(vu32*)0xF0004C48, *(vu32*)0xF0004C4C);

    while (!uartflag);
    u32 farpi = *(vu32*)0xF0004C78;
    printf("irq=%d farpi=%08X\n", uartflag, farpi);
    printf("ctl=%08X derp=%08X\n", *(vu32*)0xF0004C48, *(vu32*)0xF0004C4C);
}

#include "uic_config.h"


int _write(int file, char *ptr, int len);

volatile int audioflag;
volatile int f1, f2;
volatile u32 schmi;
volatile u32 schmitime;
void audioirq(int irq, void* userdata)
{
    u32 irqreg = *(vu32*)0xF0005430;
    if (irqreg & (1<<3))
    {
        //
        schmi = *(vu32*)0xF0005414;
        schmitime = *(vu32*)0xF0000408;
        f1++;
        *(vu32*)0xF0005430 |= (1<<3);
    }
    if (irqreg & (1<<2))
    {
        //
        f2++;
        *(vu32*)0xF0005430 |= (1<<2);
    }
    //if (irqreg != 4)
    {
        //_write(1, "IRQ17", 5);
        char dorp[20];
        sprintf(dorp, "I=%08X\n", irqreg);
        _write(1, dorp, strlen(dorp));
    }
    audioflag++;
}

volatile int flag18;
void irq18(int irq, void* userdata)
{
    flag18++;
}

volatile int flag1E;
u32 last1E;
volatile int intv1E;
void irq1E(int irq, void* userdata)
{
    flag1E++;
    u32 t = *(vu32*)0xF0000408;
    intv1E = t - last1E;
    last1E = t;
}


extern u32 irqlog[16];
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
    //flash_testbench();
    //uart_testbench();
    /*u32 zart = REG_IRQ_CURRENT;
    printf("current IRQ = %08X\n", zart);
    printf("reg=%08X\n", *(vu32*)0xF00013FC);
    printf("reg=%08X\n", *(vu32*)0xF00019FC);
    printf("reg=%08X\n", *(vu32*)0xF00013F8);
    printf("reg=%08X\n", *(vu32*)0xF00019F8);

    *(vu32*)0xF00019D8 = 1;
    *(vu32*)0xF00019DC = 0;
    *(vu32*)0xF00013FC = 0;
    *(vu32*)0xF00013F8 = 6;
    printf("13FC=%08X\n", *(vu32*)0xF00013FC);
    printf("13F8=%08X\n", *(vu32*)0xF00013F8);
    printf("19FC=%08X\n", *(vu32*)0xF00019FC);
    printf("19F8=%08X\n", *(vu32*)0xF00019F8);

    *(vu32*)0xF00013FC = 2;
    printf("13FC=%08X\n", *(vu32*)0xF00013FC);
    printf("13F8=%08X\n", *(vu32*)0xF00013F8);
    printf("19FC=%08X\n", *(vu32*)0xF00019FC);
    printf("19F8=%08X\n", *(vu32*)0xF00019F8);

    *(vu32*)0xF00013F8 = 3;
    printf("13FC=%08X\n", *(vu32*)0xF00013FC);
    printf("13F8=%08X\n", *(vu32*)0xF00013F8);
    printf("19FC=%08X\n", *(vu32*)0xF00019FC);
    printf("19F8=%08X\n", *(vu32*)0xF00019F8);

    *(vu32*)0xF00019FC = 4;
    printf("13FC=%08X\n", *(vu32*)0xF00013FC);
    printf("13F8=%08X\n", *(vu32*)0xF00013F8);
    printf("19FC=%08X\n", *(vu32*)0xF00019FC);
    printf("19F8=%08X\n", *(vu32*)0xF00019F8);

    *(vu32*)0xF00019F8 = 5;
    printf("13FC=%08X\n", *(vu32*)0xF00013FC);
    printf("13F8=%08X\n", *(vu32*)0xF00013F8);
    printf("19FC=%08X\n", *(vu32*)0xF00019FC);
    printf("19F8=%08X\n", *(vu32*)0xF00019F8);*/

    /*u32 reg = 0xF0000404;
    u32 old = *(vu32*)reg;
    *(vu32*)reg = 0xFFFFFFFF;
    u32 fazr = *(vu32*)reg;
    *(vu32*)reg = old;
    printf("mask = %08X\n", fazr);*/

    /* *(vu32*)0xF0004054 = 0x11223344;
    printf("write32: %08X\n", *(vu32*)0xF0004054);
    *(vu16*)0xF0004056 = 0x5678;
    printf("write16: %08X\n", *(vu32*)0xF0004054);
    *(vu8*)0xF0004057 = 0x9A;
    printf("write8: %08X\n", *(vu32*)0xF0004054);

    // 0xE0010000
    *(vu32*)0xF0009474 = 0x11223344;
    printf("write32: %08X\n", *(vu32*)0xF0009474);
    *(vu16*)0xF0009474 = 0x5678;
    printf("write16: %08X\n", *(vu32*)0xF0009474);
    *(vu8*)0xF0009474 = 0x9A;
    printf("write8: %08X\n", *(vu32*)0xF0009474);*/

    //printf("mask=%08X %08X\n", *(vu32*)0xF00013F8, *(vu32*)0xF00013FC);
    //*(vu32*)0xF00013F8 = 9;
    //printf("mask=%08X %08X\n", *(vu32*)0xF00013F8, *(vu32*)0xF00013FC);

    /*WUP_DelayMS(10);
    printf("after IRQ:\n");
    for (int i = 0; i <= 15; i++)
        printf("%d. %08X\n", i, irqlog[i]);*/
    // 13F8 and 19F8 read as zero
    // 13FC and 19FC read as the last value

    printf("caps1: %08X\n", *(vu32*)0xE0010040);
    printf("caps2: %08X\n", *(vu32*)0xE0010044);
    printf("version: %04X\n", *(vu16*)0xE00100FE);
    printf("ver2: %08X\n", *(vu32*)0xE00100FC);

    /*UIC_WriteEnable();
    WUP_DelayMS(1);
    printf("write enable\n");
    for (u32 i = 0; i < 0x700; i+=0x80)
    //for (u32 i = 0x100; i < 0x700; i+=0x80)
    {
        UIC_WriteEEPROM(i, &uic_config_bin[i], 0x80);
        printf("wrote %04X\n", i);
       // break;
    }
    printf("written\n");
    WUP_DelayMS(1);
    UIC_WriteDisable();
    WUP_DelayMS(140);
    printf("dsiable\n");*/
    // writing 0200 causes reset??

    /*u32 derp1, derp2;
    //UIC_test(0, (u8*)&derp1, 4);
    //printf("test1=%08X\n", derp1);
    //UIC_test(0x700, (u8*)&derp2, 4);
    //UIC_ReadEEPROM(0, (u8*)&derp2, 4);
    UIC_test(0, (u8*)&derp2, 4);
    printf("test2=%08X\n", derp2);

    u8* eep = (u8*)malloc(0x700);
    for (u32 i = 0; i < 0x700; i+=0x80)
    {
        UIC_ReadEEPROM(i, &eep[i], 0x80);
    }
    dump_data(eep, 0x700);
    printf("eeprom dumped\n");*/

    for (;;)
    {
        u8 fark;
        UIC_SendCommand(0x13, NULL, 0, &fark, 1);
        WUP_DelayMS(5);
        if (fark != 1) continue;
        printf("UIC13=%02X\n", fark);
        break;
    }
    printf("UIC came up\n");

#if 0
    // 69EF30B0
    // 01101001 11101111 00110000 10110000
    // TODO move this to WUP_Init()
    int a = SDIO_Init();
    printf("a=%d\n", a);
    int b = Wifi_Init();
    printf("%d caps: %08X\n", b, *(vu32*)0xE0010040);

    WUP_DelayMS(2000);
    //for (;;)
    {
        //Wifi_StartScan(1);
        Wifi_StartScan2(1);
        WUP_DelayMS(10);
        //WUP_DelayMS(100);
    }
    //Wifi_StartScan(1);
    Wifi_JoinNetwork();
    //Wifi_JoinNetwork2();
    printf("joined network, maybe\n");

    //dump_data((u8*)0x200000, 0xE00);

    ((u8*)0x200000)[0] = *(u8*)0x3FFFFC;
#endif

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

#if 0
    AudioAmp_SetPage(0x01);
    u8 fa = AudioAmp_ReadReg(0x2F);
    printf("fa=%02X\n", fa);
    AudioAmp_SetPage(0x00);
    u8 zi = AudioAmp_ReadReg(0x41);
    printf("zi=%02X\n", zi);
    AudioAmp_SetPage(0x01);
    AudioAmp_WriteReg(0x2F, 0x32);
    AudioAmp_SetPage(0x00);
    AudioAmp_WriteReg(0x53, 0x01);
    AudioAmp_WriteReg(0x41, 0x00);
    //AudioAmp_WriteReg(0x41, 0x30);
    AudioAmp_WriteReg(0x40, 0x02);

    //

    //AudioAmp_WriteReg()


    AudioAmp_SetPage(0x01);
    AudioAmp_WriteReg(0x33, 0x40);
    AudioAmp_SetPage(0x00);
    u8 muted = AudioAmp_ReadReg(0x40);
    printf("muted=%02X\n", muted);
#endif

    AudioAmp_SetPage(0x01);
    u8 blah1 = AudioAmp_ReadReg(0x2F);
    AudioAmp_SetPage(0x00);
    u8 blah2 = AudioAmp_ReadReg(0x41);
    AudioAmp_SetPage(0x01);
    u8 blah3 = AudioAmp_ReadReg(0x2F);
    blah3 &= ~0x7F;
    blah3 |= 0x32; // ??
    AudioAmp_WriteReg(0x2F, blah3);
    AudioAmp_SetPage(0x01);
    u8 blah4 = AudioAmp_ReadReg(0x30);
    printf("init more: %02X %02X %02X %02X\n", blah1, blah2, blah3, blah4);

    // SET VOLUME
#if 0
    AudioAmp_SetPage(0x00);
    AudioAmp_WriteReg(0x41, 0x00); // volume
    AudioAmp_SetPage(0x00);
    AudioAmp_WriteReg(0x40, 0x02); // unmute
    AudioAmp_SetPage(0x00);
    u8 zorp1 = AudioAmp_ReadReg(0x41);
    printf("volume: %02X\n", zorp1);

    AudioAmp_SetPage(0x01);
    AudioAmp_WriteReg(0x33, 0x40); // mic bias?


    //init more: 32 81 32 20
    //volume: 00

    void pett();
    pett();
#endif

    /*printf("initial audio regs:\n");
    for (u32 r = 0xF0005400; r < 0xF0005500; r+=0x10)
    {
        printf("%08X:  %08X %08X %08X %08X\n",
               r, *(vu32*)(r), *(vu32*)(r+4), *(vu32*)(r+8), *(vu32*)(r+12));
    }*/
    // F0005400 = 8000
    // F0005404 = 1F00
    // F000542C = F
    // F00054B8 = 10100000
    // F00054C4 = 111
    // rest is 0


    // F00054C4 reads as 111
    printf("praat %08X %08X\n", *(vu32*)0xF00054C4,*(vu32*)0xF0005444);
    *(vu32*)0xF00054C4 = 0x111;
    //*(vu32*)0xF0005444 = 0x40018128;
    // starts at 8000
    u32 fazil = *(vu32*)0xF0005400;
    printf("fazil1 = %08X\n", fazil);
    fazil &= ~(1<<4);
    fazil &= ~(1<<5);
    fazil &= ~(0x1F<<6);
    fazil |= (1<<6);
    fazil &= ~(0x1F<<11);
    fazil &= ~(1<<20);
    fazil &= ~(1<<21);
    fazil |= (1<<19);
    fazil &= ~(1<<18);
    fazil |= (1<<0);
    *(vu32*)0xF0005400 = fazil;
    //printf("fazil2 = %08X\n", fazil);
    *(vu32*)0xF0005420 |= 1;
    //printf("fazil3 = %08X\n", fazil);
    *(vu32*)0xF000541C |= 1;
    //printf("fazil4 = %08X\n", fazil);
    *(vu32*)0xF000542C = 0;
    //printf("fazil5 = %08X\n", fazil);
    WUP_SetIRQHandler(0x17, audioirq, NULL, 0);
    //WUP_SetIRQHandler(0x18, irq18, NULL, 0);
    //WUP_SetIRQHandler(0x1E, irq1E, NULL, 0);
    audioflag = 0;
    f1 = 0; f2 = 0;
    flag18 = 0;
    flag1E = 0;
    printf("fazil6 = %08X\n", *(vu32*)0xF0005400);
    // 00080041


    u8* tmp = (u8*)malloc(0x40010);
    u32 atmp = (u32)&tmp[0];
    atmp = (atmp + 0xF) & ~0xF;
    u16* buffer = (u16*)atmp;
    printf("buffer=%08X (tmp=%08X)\n", (u32)&buffer[0], (u32)&tmp[0]);

    for (int i = 0; i < 0x20000; i++)
    {
        //buffer[i] = (i & 0x40) ? 0x7FFF : 0;
        buffer[i] = (i & 0x40) ? 0x7FFF : 0x8000;
        //buffer[i] = (i & 0x40) ? ((i&1)?0x7FFF:0x8000) : 0x8000; // odd only - right
        //buffer[i] = (i & 0x40) ? ((i&1)?0x8000:0x7FFF) : 0x8000; // even only - left
        //buffer[i] = (i & 0x40) ? ((i&1)?0x6666:0x42D2) : 0;
    }
    DC_FlushRange(buffer, 0x40000);


    //*(vu32*)0xF0005408 = 0x380000;
    //*(vu32*)0xF000540C = 0x381000;
    //*(vu32*)0xF000540C = 0x3F0000;
    *(vu32*)0xF0005408 = (u32)&buffer[0];
    *(vu32*)0xF000540C = (u32)&buffer[0x20000];
    /**(vu32*)0xF00054A0 = 0x38200;
    printf("pet=%08X\n", *(vu32*)0xF00054A0);
    *(vu32*)0xF00054A4 = (*(vu32*)0xF00054A0) + 0xC0;
    *(vu32*)0xF00054A8 = *(vu32*)0xF00054A0;*/
    // read A8 to init FIFO from
    printf("a8=%08X\n", *(vu32*)0xF00054A8);
    printf("04=%08X\n", *(vu32*)0xF0005404);
    printf("44=%08X\n", *(vu32*)0xF0005444);
    *(vu32*)0xF0005404 |= 0x20;
    //*(vu32*)0xF0005444 = 0x40018128; // FIXME
    // read at 35CC, check bit7
    printf("04=%08X\n", *(vu32*)0xF0005404);
    printf("44=%08X\n", *(vu32*)0xF0005444);
    printf("04=%08X\n", *(vu32*)0xF0005404);
    *(vu32*)0xF0005424 = 0x14;//8;
    printf("2c=%08X\n", *(vu32*)0xF000542C);
    *(vu32*)0xF000542C |= 0xC;
    printf("08=%08X\n", *(vu32*)0xF0005408);
    *(vu32*)0xF0005410 = *(vu32*)0xF0005408;
    printf("fazil7 = %08X\n", *(vu32*)0xF0005400);

#if 1
    printf("2c=%08X\n", *(vu32*)0xF000542C);
    *(vu32*)0xF000542C &= ~0x8;
    printf("20=%08X\n", *(vu32*)0xF0005420);
    *(vu32*)0xF0005420 |= 0x1;
    //*(vu32*)0xF0005410 = 0x380000;
    *(vu32*)0xF0005410 = (u32)&buffer[0];
    printf("20=%08X\n", *(vu32*)0xF0005420);
    printf("pet\n");
    // causes IRQ 17 to start triggering
    *(vu32*)0xF0005420 |= 0x4;
    //while (!(*(vu32*)0xF0005434 & (1<<2)));
    printf("poot %08X\n", *(vu32*)0xF0005434);
    printf("2c=%08X\n", *(vu32*)0xF000542C);
    *(vu32*)0xF000542C &= ~4;
#endif
    //*(vu32*)0xF0005420 |= 0x1;
    //*(vu32*)0xF0005420 |= 0x4;

    //*(vu32*)0xF0005400 &= ~(1<<19);
    *(vu32*)0xF0005400 |= (1<<19);
    *(vu32*)0xF0005400 |= (1<<22);
    /**(vu32*)0xF0005400 &= ~(0x1F<<6);
    *(vu32*)0xF0005400 |= (1<<6);
    *(vu32*)0xF0005400 &= ~(0x1F<<11);
    *(vu32*)0xF0005400 |= (31<<11);*/
    //*(vu32*)0xF0005400 |= (1<<18);
    printf("04=%08X\n", *(vu32*)0xF0005404);
    *(vu32*)0xF0005404 &= ~0x80;
    printf("04=%08X\n", *(vu32*)0xF0005404);
    *(vu32*)0xF0005424 = 8;
    printf("2c=%08X\n", *(vu32*)0xF000542C);
    *(vu32*)0xF000542C |= 0xC;
    printf("08=%08X\n", *(vu32*)0xF0005408);
    printf("10=%08X\n", *(vu32*)0xF0005410);
    //*(vu32*)0xF0005404 = 0x1FA0;
    printf("zarmf=%08X\n", *(vu32*)0xF0005404);
    //*(vu32*)0xF0005400 |= 0x00800000;
    //*(vu32*)0xF0005420 |= 0xFFFFFFFF; // breaks playback
    //*(vu32*)0xF0005444 = 0;// |= 0xFFFFFFFF;
    // must be both cleared for audio to work
    *(vu32*)0xF0005400 &= ~0x400000;
    *(vu32*)0xF0005400 &= ~0x80000;
    // causes IRQ 17
    // starts playback?
    // IRQ triggers 40537 microseconds after this
    u32 startpos = *(vu32*)0xF0000408;
    *(vu32*)0xF0005410 = *(vu32*)0xF0005408;
    /*for (u32 r = 0xF0005400; r < 0xF0005500; r+=16)
        printf("%08X/%08X/%08X/%08X\n", *(vu32*)(r), *(vu32*)(r+4), *(vu32*)(r+8), *(vu32*)(r+12));
    WUP_DelayMS(2000);*/
    WUP_DelayMS(8);
    for (u32 r = 0xF0005400; r < 0xF0005500; r+=16)
        printf("%08X/%08X/%08X/%08X\n", *(vu32*)(r), *(vu32*)(r+4), *(vu32*)(r+8), *(vu32*)(r+12));

    // when IRQ bit3 comes up:
    // pos=00380F60 when 5424=0x14
    // pos=00380FC0 when 5424=0x08

    // bit6-10:
    // time to play 1968 samples
    // 0 = 40523
    // 1 = 40527
    // 2 = 40537
    // 3 = 40529
    // 31 = 40522
    //
    // 11-15/6-10
    // 1/1 = 40538
    // 2/1 = 40521
    // 31/1 = 40541


    int frame = 0;
    int d = 0;
    u32 regaddr = 0xF0005000;
    u16 oldval = 0;
	for (;;)
	{
		frame++;
		if (frame >= 15) frame = 0;

        sInputData* test = Input_Scan();

        // TODO: integrate this elsewhere
        Audio_SetVolume(test->AudioVolume);
        Audio_SetOutput((test->PowerStatus & (1<<5)) ? 1:0);

        /*if (test->ButtonsPressed & BTN_X)
        {
            printf("Pressed X\n");
            UIC_SetBacklight(0);
            WUP_DelayMS(10);
            UIC_SendCommand(0x14, NULL, 0, NULL, 0);
        }*/

        /*if (test->ButtonsDown & BTN_A)
            printf("Pressed A! frame=%d\n", frame);
        if (test->ButtonsPressed & BTN_B)
            printf("Pressed B! frame=%d\n", frame);
        if (test->ButtonsDown & BTN_UP)
            printf("Pressed UP! frame=%d,\nthis is a multi-line printf\nI like it\n", frame);
        if (test->ButtonsDown & BTN_DOWN)
            printf("Pressed DOWN! this is going to be a really long line, the point is to test how far the console thing can go and whether it handles this correctly\n");
*/

        if (test->ButtonsPressed & BTN_UP)
        {
            *(vu32*)regaddr = oldval;
            regaddr += 4;
            oldval = *(vu32*)regaddr;
            printf("CUR IO: %08X = %04X\n", regaddr, oldval);
        }
        if (test->ButtonsPressed & BTN_DOWN)
        {
            *(vu32*)regaddr = oldval;
            regaddr -= 4;
            oldval = *(vu32*)regaddr;
            printf("CUR IO: %08X = %04X\n", regaddr, oldval);
        }
        if (test->ButtonsPressed & BTN_A)
        {
            /*oldval = *(vu32*)regaddr;
            *(vu32*)regaddr = 0xC300;
            printf("SET: %08X = %04X -> %04X\n", regaddr, oldval, *(vu32*)regaddr);*/
            //AudioAmp_SetPage(0);
            /*u8 fart = AudioAmp_ReadReg(0x43);
            printf("fart=%02X\n", fart);*/
            /*for (int a = 0; a < 0x100; a+=0x10)
            {
                printf("%02X:  ", a);
                for (int b = 0; b < 0x10; b++)
                {
                    u8 reg = a+b;
                    u8 val = AudioAmp_ReadReg(reg);
                    printf("%02X ", val);
                }
                printf("\n");
            }*/
            /*
             * register mask:
             *  F0005400:  01FFFFFF 0000FFFF 0FFFFFF8 0FFFFFF8
                F0005410:  0FFFFFF8 00000000 0000FFFF 0000001F
                F0005420:  00000103 01FFFFFF 00000000 0000003F
                F0005430:  00000000 00008008 00000000 00000000
                F0005440:  00000000 C01FF3FD 01FFFFFF 00000000
                F0005450:  00000000 00000000 00000000 00000000
                F0005460:  00000000 00000000 00000000 00000000
                F0005470:  00000000 00000000 00000000 00000000
                F0005480:  00000000 00000000 00000000 00000000
                F0005490:  00000000 00000000 00000000 00000000
                F00054A0:  01FFFFFF 01FFFFFF 01FFFFFF 00000000
                F00054B0:  00000000 00000000 10100000 00000000
                F00054C0:  00000000 00000111 00000000 00000000
                F00054D0:  00000000 00000000 00000000 00000000
                F00054E0:  00000000 00000000 00000000 00000000
                F00054F0:  00000000 00000000 00000000 00000000
             */
            for (u32 r = 0xF0005400; r < 0xF0005500; r+=0x4)
            {
                if (r==0xF0005408) continue;
                if (r==0xF000540C) continue;
                if (r==0xF0005410) continue;
                if (r==0xF0005414) continue;
                *(vu32 * )(r) = 0xFFFFFFFF;
            }
            for (u32 r = 0xF0005400; r < 0xF0005500; r+=0x10)
            {
                printf("%08X:  %08X %08X %08X %08X\n",
                       r, *(vu32*)(r), *(vu32*)(r+4), *(vu32*)(r+8), *(vu32*)(r+12));
            }
        }
        if (test->ButtonsPressed & BTN_B)
        {
            /**(vu32*)regaddr = oldval;
            printf("RESET: %08X = %04X\n", regaddr, oldval);*/
            printf("reg=%08X pos=%08X (%08X) cnt=%08X time=%d vol=%d audioflag=%d/%d/%d %d %d  intv=%d  pwr=%02X\n",
                   *(vu32*)0xF0005400,
                   *(vu32*)0xF0005414, schmi,
                   *(vu32*)0xF0005428,
                   schmitime-startpos,
                   test->AudioVolume,
                   audioflag, f1, f2,
                   flag18, flag1E, intv1E,
                   test->PowerStatus);
        }
        if (test->ButtonsPressed & BTN_X)
        {
            printf("BEEP!\n");
            u32 duration = 0x10000;//0x100000;
            u16 sin = 0x10D8;//71;
            u16 cos = 0x7EE3;//65534;
            AudioAmp_SetPage(0);
            AudioAmp_WriteReg(73, (duration>>16)&0xFF);
            AudioAmp_WriteReg(74, (duration>>8)&0xFcF);
            AudioAmp_WriteReg(75, duration&0xFF);
            AudioAmp_WriteReg(76, sin>>8);
            AudioAmp_WriteReg(77, sin&0xFF);
            AudioAmp_WriteReg(78, cos>>8);
            AudioAmp_WriteReg(79, cos&0xFF);
            AudioAmp_WriteReg(72, 0x80);

            AudioAmp_WriteReg(0x40, 0x0E);
            for (;;)
            {
                u8 derp = AudioAmp_ReadReg(0x26);
                if ((derp & 0x11) == 0x11) break;
            }
            AudioAmp_WriteReg(0x0B, 0x02);
            //AudioAmp_WriteReg(71, 0x80);
            AudioAmp_WriteReg(71, 0x80 | 0x14);
            AudioAmp_WriteReg(0x0B, 0x82);
            AudioAmp_WriteReg(0x40, 0x02);
        }
        if (test->ButtonsPressed & BTN_Y)
        {
            /*AudioAmp_SetPage(0);
            u8 reg = AudioAmp_ReadReg(71);
            printf("beep status = %02X\n", reg);*/
            *(vu32*)0xF0005410 = *(vu32*)0xF0005408;
        }

        //*(vu32*)regaddr |= 0xC200;
        //*(vu32*)regaddr ^= 0x100;

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



