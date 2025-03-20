#include <wup/wup.h>


static u8 AddrMode;

static u32 PartBase;
static int PartNumEntries;
static u32* PartHeader;


static u32 chkcode(u32 addr)
{
    addr &= 0x01FFFFFF;
    u32 code = addr & 0x7F;
    code ^= ((addr >> 7) & 0x7F);
    code ^= ((addr >> 14) & 0x7F);
    code ^= ((addr >> 21) & 0x7F);
    code ^= (addr >> 28);
    return (code << 25) | addr;
}

int Flash_Init()
{
    // TODO: check cmd 9F?

    Flash_Set4ByteAddr(1);

    u32 myaddr = *(vu32*)0x3FFFF8;
    if (myaddr != 0 && myaddr != 0xFFFFFFFF && myaddr == chkcode(myaddr))
    {
        PartBase = myaddr & 0x1FFFFFF;
    }
    else
    {
        u8 partsel = 0;
        Flash_Read(0xF000, &partsel, 1);
        if (partsel == 1)
            PartBase = 0x500000;
        else
            PartBase = 0x100000;
    }

    // load partition data

    u32 tbllen = 0;
    Flash_Read(PartBase+4, (u8*)&tbllen, 4);
    if ((tbllen < 16) || (tbllen > 0x10000) || (tbllen & 0xF))
        return 0;

    PartHeader = (u32*)malloc(tbllen);
    if (!PartHeader)
    {
        printf("Flash: failed to malloc() partheader\n");
        for (;;);
        return 0;
    }

    Flash_Read(PartBase, (u8*)PartHeader, tbllen);
    PartNumEntries = tbllen >> 4;

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

void Flash_Read(u32 addr, u8* data, int len)
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

    SPI_Read(data, len);
    SPI_Finish();
}


int Flash_GetEntryInfo(char* tag, u32* offset, u32* length, u32* version)
{
    u32 tag32 = tag[0] | (tag[1] << 8) | (tag[2] << 16) | (tag[3] << 24);
    u32* hdr = PartHeader;
    for (int i = 0; i < PartNumEntries; i++)
    {
        if (hdr[2] == tag32)
        {
            if (offset) *offset = PartBase + hdr[0];
            if (length) *length = hdr[1];
            if (version) *version = hdr[3];
            return 1;
        }

        hdr += 4;
    }

    return 0;
}
