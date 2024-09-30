#include "wup.h"


int LCD_TryGetID();

void (*_LCD_Init)();
void (*_LCD_SetBrightness)(int brightness);

void LCD_JDI_Init();
void LCD_JDI_SetBrightness(int brightness);

void LCD_Panasonic_Init();
void LCD_Panasonic_SetBrightness(int brightness);

u8 BrightnessData[16];


void LCD_Init()
{
    UIC_ReadEEPROM(0x277, BrightnessData, 16);

    I2C_Start(I2C_BUS_LCD);
    *(vu32*)0xF0005100 = 0xC200;

    int good = 0;
    for (int i = 0; i < 100; i++)
    {
        if (LCD_TryGetID())
        {
            good = 1;
            break;
        }
    }

    if (!good)
    {
        // TODO: handle this somehow?
        I2C_Finish(I2C_BUS_LCD);
        return;
    }

    _LCD_Init();
    I2C_Finish(I2C_BUS_LCD);

    LCD_SetBrightness(BrightnessData[12]);
}

void LCD_SetBrightness(int brightness)
{
    if (brightness < -1) brightness = -1;
    else if (brightness > 5) brightness = 5;

    I2C_Start(I2C_BUS_LCD);

    // toggle LCD GPIO
    *(vu32*)0xF0005100 &= ~0x0100;
    WUP_DelayMS(5);
    *(vu32*)0xF0005100 |= 0x0100;
    WUP_DelayMS(15);

    _LCD_SetBrightness(brightness);
    I2C_Finish(I2C_BUS_LCD);

    UIC_SetBacklight(brightness >= 0);
}


int LCD_TryGetID()
{
    // toggle LCD GPIO
    *(vu32*)0xF0005100 &= ~0x0100;
    WUP_DelayMS(5);
    *(vu32*)0xF0005100 |= 0x0100;
    WUP_DelayMS(15);

    // I2C LCD init
    u8 buf[16];

    buf[0] = 0xB0; buf[1] = 0x02;
    if (!I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0)) return 0;

    buf[0] = 0xBF;
    if (!I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 1, 1)) return 0;
    if (!I2C_Read(I2C_BUS_LCD, I2C_DEV_LCD, buf, 5)) return 0;

    u32 lcdid = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

    buf[0] = 0xB0; buf[1] = 0x03;
    if (!I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0)) return 0;

    if (lcdid == LCD_ID_PANASONIC)
    {
        _LCD_Init = LCD_Panasonic_Init;
        _LCD_SetBrightness = LCD_Panasonic_SetBrightness;
    }
    else if (lcdid == LCD_ID_JDI)
    {
        _LCD_Init = LCD_JDI_Init;
        _LCD_SetBrightness = LCD_JDI_SetBrightness;
    }
    else
        return 0;

    return 1;
}

void LCD_JDI_Init()
{
    u8 buf[16];

    buf[0] = 0xB0; buf[1] = 0x02;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);

    buf[0] = 0xBB; buf[1] = 0x08; buf[2] = 0x7A; buf[3] = 0x01; buf[4] = 0x00;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 5, 0);

    buf[0] = 0xB0; buf[1] = 0x03;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);
}

void LCD_JDI_SetBrightness(int brightness)
{
    u8 buf[16];
    u8 brighttbl[10] = {0x00, 0x01, 0x02, 0x05, 0x0B, 0x17, 0x2F, 0x5F, 0xC5, 0xFF};

    int on = 1;
    if (brightness < 0)
    {
        on = 0;
        brightness = 0;
    }

    buf[0] = 0xB0; buf[1] = 0x02;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);

    u8 b2 = BrightnessData[brightness*2 + 1];
    if (b2 > 10) b2 = 10;

    buf[0] = 0xBB;
    buf[1] = 0x08;
    buf[2] = BrightnessData[brightness*2];
    buf[3] = brighttbl[b2];
    buf[4] = 0x00;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 5, 0);

    buf[0] = 0xB0; buf[1] = 0x03;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);

    buf[0] = 0xB0; buf[1] = 0x02;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);

    buf[0] = 0xB8; buf[1] = 0x01; buf[2] = 0x18; buf[3] = 0x01; buf[4] = 0x04; buf[5] = 0x40;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 6, 0);

    buf[0] = 0xB0; buf[1] = 0x03;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);

    buf[0] = on ? 0x29 : 0x28;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 1, 0);
}

void LCD_Panasonic_Init()
{
    u8 buf[16];

    buf[0] = 0x05; buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0x01; buf[4] = 0x7A;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 5, 0);
}

void LCD_Panasonic_SetBrightness(int brightness)
{
    u8 buf[16];
    u8 brighttbl[10] = {0x00, 0x01, 0x02, 0x05, 0x0B, 0x17, 0x2F, 0x5F, 0xC5, 0xFF};

    int on = 1;
    if (brightness < 0)
    {
        on = 0;
        brightness = 0;
    }

    u8 b2 = BrightnessData[brightness*2 + 1];
    if (b2 > 10) b2 = 10;

    buf[0] = 0x05;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = brighttbl[b2];
    buf[4] = BrightnessData[brightness*2];
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 5, 0);

    buf[0] = 0x0B; buf[1] = 0xAA;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);

    buf[0] = 0x02;
    buf[1] = on ? 0x01 : 0x00;
    I2C_Write(I2C_BUS_LCD, I2C_DEV_LCD, buf, 2, 0);
}
