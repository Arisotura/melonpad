#include "wup.h"


#if 0
u8 UIC_WaitForReply(u8 unwanted)
{
    u8 ret = unwanted;
    for (int i = 0; i < 200; i++)
    {
        SPI_Read(&ret, 1);
        if (ret != unwanted) break;
    }
    return ret;
}

int UIC_UploadFirmware(u8* data, int len)
{
    u8 buf[16];

    buf[0] = 0x00;
    buf[1] = 0xFF;

    SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
    SPI_Write(buf, 2);
    if (UIC_WaitForReply(0x00) != 0x79) return 0;
    SPI_Finish();

    SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
    SPI_Read(buf, 7);
    SPI_Finish();

    u32 addr = 0x9000;
    for (int i = 0; i < len; )
    {
        int chunk = 128;
        if ((i + chunk) > len)
            chunk = len - i;

        buf[0] = 0x31;
        buf[1] = 0xCE;

        SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
        SPI_Write(buf, 2);
        if (UIC_WaitForReply(0x00) != 0x79) return 0;
        SPI_Finish();

        buf[0] = (addr >> 24) & 0xFF;
        buf[1] = (addr >> 16) & 0xFF;
        buf[2] = (addr >> 8) & 0xFF;
        buf[3] = addr & 0xFF;
        buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];

        SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
        SPI_Write(buf, 5);
        if (UIC_WaitForReply(0x00) != 0x79) return 0;
        SPI_Finish();

        buf[0] = chunk;

        SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
        SPI_Write(buf, 1);
        if (UIC_WaitForReply(0x00) != 0x79) return 0;
        SPI_Finish();

        buf[0] = chunk;
        for (int j = 0; j < chunk; j++)
            buf[0] ^= data[i+j];

        SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
        SPI_Write(&data[i], chunk);
        SPI_Write(buf, 1);
        if (UIC_WaitForReply(0x00) != 0x79) return 0;
        SPI_Finish();

        i += chunk;
        addr += chunk;
    }

    buf[0] = 0x21;
    buf[1] = 0xDE;

    SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
    SPI_Write(buf, 2);
    if (UIC_WaitForReply(0x79) != 0x51) return 0;
    SPI_Finish();

    return 1;
}
#endif


void UIC_ReadEEPROM(u32 offset, u8* data, int length)
{
    offset += 0x1100;
    if (offset >= 0x1800) return;
    if (length >= 256) return;

    u8 buf[4];
    buf[0] = 0x03;
    buf[1] = (offset >> 8) & 0xFF;
    buf[2] = offset & 0xFF;
    buf[3] = length;

    SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
    SPI_Write(buf, 4);
    SPI_Read(data, length);
    SPI_Finish();
}

void UIC_SetBacklight(int enable)
{
    u8 buf[2];
    buf[0] = 0x12;
    buf[1] = enable;

    SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
    SPI_Write(buf, 2);
    SPI_Finish();
}
