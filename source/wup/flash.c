#include <wup/wup.h>


static u8 AddrMode;

static u32 PartBase;
static int PartNumEntries;
static u32* PartHeader;

static void* Mutex;


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
    Mutex = Mutex_Create();

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
    Mutex_Acquire(Mutex, NoTimeout);

    u8 cmd = 0x06;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x02);
    Mutex_Release(Mutex);
}

void Flash_WriteDisable()
{
    Mutex_Acquire(Mutex, NoTimeout);

    u8 cmd = 0x04;
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    Mutex_Release(Mutex);
}

void Flash_Set4ByteAddr(int val)
{
    Mutex_Acquire(Mutex, NoTimeout);
    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    u8 cmd = val ? 0xB7 : 0xE9;

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);
    SPI_Write(&cmd, 1);
    SPI_Finish();

    Flash_WriteDisable();

    AddrMode = val ? 1 : 0;
    Mutex_Release(Mutex);
}

void Flash_Read(u32 addr, void* data, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);
    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);

    u8 cmd[5];
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
    Mutex_Release(Mutex);
}

void Flash_EraseSector(u32 addr)
{
    Mutex_Acquire(Mutex, NoTimeout);
    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);

    u8 cmd[5];
    cmd[0] = 0xD8;
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

    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    Mutex_Release(Mutex);
}

void Flash_EraseSubsector(u32 addr)
{
    Mutex_Acquire(Mutex, NoTimeout);
    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);

    u8 cmd[5];
    cmd[0] = 0x20;
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

    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    Mutex_Release(Mutex);
}

void Flash_PageProgram(u32 addr, void* data, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);
    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_48MHZ);

    u8 cmd[5];
    cmd[0] = 0x02;
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

    SPI_Write(data, len);

    SPI_Finish();
    Mutex_Release(Mutex);
}

void Flash_Write(u32 addr, void* data, int len)
{
    Mutex_Acquire(Mutex, NoTimeout);

    u8* data8 = (u8*)data;
    for (int pos = 0; pos < len; )
    {
        int chunk = 0x100 - (addr & 0xFF);
        if ((pos + chunk) > len)
            chunk = len - pos;

        Flash_PageProgram(addr, data8, chunk);
        addr += chunk;
        data8 += chunk;
        pos += chunk;
    }

    Mutex_Release(Mutex);
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
