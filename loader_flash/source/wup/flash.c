#include <wup/wup.h>


static u8 AddrMode;


int Flash_Init()
{
    // TODO: check cmd 9F?
    Flash_Set4ByteAddr(1);

    return 1;
}


void Flash_ReadID(u8* id, int len)
{
    u8 cmd = 0x9F;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);
    SPI_Read(id, len);
    SPI_Finish();
}

void Flash_WaitForStatus(u8 mask, u8 val)
{
    u8 cmd = 0x05;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);

    for (;;)
    {
        u8 status;
        SPI_Read(&status, 1);
        if ((status & mask) == val)
            break;
    }

    SPI_Finish();
}

void Flash_WriteEnable()
{
    u8 cmd = 0x06;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x02);
}

void Flash_WriteDisable()
{
    u8 cmd = 0x04;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
}

void Flash_Set4ByteAddr(int val)
{
    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    u8 cmd = val ? 0xB7 : 0xE9;

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();

    Flash_WriteDisable();

    AddrMode = val ? 1 : 0;
}

void Flash_Read(u32 addr, void* data, int len)
{
    u8 cmd[5];

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);

    cmd[0] = 0x03;
    if (AddrMode)
    {
        cmd[1] = (addr >> 24) & 0xFF;
        cmd[2] = (addr >> 16) & 0xFF;
        cmd[3] = (addr >> 8) & 0xFF;
        cmd[4] = addr & 0xFF;
        SPI_Write(cmd, 5);
    }
    else
    {
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        SPI_Write(cmd, 4);
    }

    u8* dst = (u8*)data;
    int kChunkLen = 0x100000; // DMA chunk
    for (int i = 0; i < len; i += kChunkLen)
    {
        int chunk = kChunkLen;
        if ((i + chunk) > len)
            chunk = len - i;

        SPI_Read(dst, chunk);
        dst += chunk;
    }

    SPI_Finish();
}


int Flash_GetCodeAddr(u32 partaddr, u32* codeaddr, u32* codelen)
{
    // look for LVC_ blob

    u32 tbllen = 0;
    Flash_Read(partaddr+4, (u8*)&tbllen, 4);
    if ((tbllen < 16) || (tbllen > 0x10000) || (tbllen & 0xF))
        return 0;

    for (u32 entry = 0x10; entry < tbllen; entry += 0x10)
    {
        u32 entrydata[4];
        Flash_Read(partaddr+entry, (u8*)entrydata, 16);

        if (entrydata[0] > 0x2000000) // offset
            continue;
        if (entrydata[1] > 0x400000) // length
            continue;
        if (entrydata[2] != 0x5F43564C) // LVC_
            continue;

        *codeaddr = partaddr + entrydata[0];
        *codelen = entrydata[1];
        return 1;
    }

    return 0;
}
