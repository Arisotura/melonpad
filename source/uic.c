#include "wup.h"


// This table defines which UIC state transitions are possible.
// The UIC firmware uses a similar table.
const u8 StateTransitions[15][15] = {
    {0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1},
    {1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1},
    {1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1},
    {1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1},
    {1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1},
    {1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0},
    {1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0}
};

static u8 UICState;


void UIC_Init()
{
    // TODO: check command 7F and see if we need to upload a binary

    // Note on UIC initialization:
    // When the gamepad is powered up, the UIC starts in state 11.
    // State 11 is an idle state, where, among other things, input scanning doesn't work.
    // One must switch the UIC to state 3 or 4 to put the gamepad in sleep mode.
    // (TODO: what are the differences between states 3 and 4?)
    // Then pressing the power button will wake up the gamepad, with the UIC in state 0.
    // The wakeup event will reset the CPU.

    UICState = UIC_GetState();
    if (UICState == 11)
    {
        UIC_SetState(3);
        for (;;);
    }
}


u8 UIC_GetFirmwareType()
{
    u8 buf = 0x7F;
    u8 ret = 0;

    SPI_Start(SPI_DEVICE_UIC, SPI_SPEED_UIC);
    SPI_Write(&buf, 1);
    WUP_DelayMS(1);
    SPI_Read(&ret, 1);
    SPI_Finish();

    return ret;
}


void UIC_SendCommand(u8 cmd, u8* in_data, int in_len, u8* out_data, int out_len)
{
    SPI_Start(SPI_DEVICE_UIC, 0x8018);

    SPI_Write(&cmd, 1);
    if (in_data)
    {
        WUP_DelayUS(60);
        SPI_Write(in_data, in_len);
    }

    if (out_data)
    {
        // TODO: figure out what REG_SPI_CNT bit6 does
        REG_SPI_CNT |= 0x40;
        WUP_DelayUS(60);
        REG_SPI_CNT &= ~0x40;
        SPI_Read(out_data, out_len);
    }

    SPI_Finish();
}


#if 0
// FIRMWARE UPLOAD CODE. DANGER
// this is untested, and a firmware upload gone wrong can and will brick the UIC.

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


u32 UIC_GetFirmwareVersion()
{
    u8 buf[4];
    UIC_SendCommand(0x0B, NULL, 0, buf, 4);
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}


u8 UIC_GetState()
{
    u8 ret;
    UIC_SendCommand(0x05, NULL, 0, &ret, 1);
    return ret;
}

int UIC_SetState(u8 state)
{
    if (state > 14) return 0;
    if (!StateTransitions[UICState][state]) return 0;

    UIC_SendCommand(0x01, &state, 1, NULL, 0);
    return 1;
}


void UIC_ReadEEPROM(u32 offset, u8* data, int length)
{
    offset += 0x1100;
    if (offset >= 0x1800) return;
    if (length < 1) return;
    if (length >= 256) return;

    u8 buf[3];
    buf[0] = (offset >> 8) & 0xFF;
    buf[1] = offset & 0xFF;
    buf[2] = length;

    UIC_SendCommand(0x03, buf, 3, data, length);
}

void UIC_SetBacklight(int enable)
{
    u8 buf = enable ? 1 : 0;
    UIC_SendCommand(0x12, &buf, 1, NULL, 0);
}
