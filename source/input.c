#include "wup.h"


static sInputData InputData;


// REMOVE ME
static u32 derp_div(u32 a, u32 b)
{
    u32 ret = 0;

    while (a >= b)
    {
        a -= b;
        ret++;
    }

    return ret;
}


void Input_Init()
{
    // TODO
    InputData.ButtonsDown = 0;
    InputData.ButtonsPressed = 0;
    InputData.ButtonsReleased = 0;
}


sInputData* Input_Scan()
{
    u8 data[128];
    data[0x00] = 0;
    data[0x7F] = 0;

    do UIC_GetInputData(data);
    while ((data[0x00] ^ data[0x7F]) != 0xFF);

    u32 lastbtn = InputData.ButtonsDown;
    InputData.ButtonsDown = data[0x02] | (data[0x03] << 8) | (data[0x50] << 16);
    InputData.ButtonsPressed = (~lastbtn) & InputData.ButtonsDown;
    InputData.ButtonsReleased = lastbtn & (~InputData.ButtonsDown);

    InputData.PowerStatus = data[0x04];
    InputData.BatteryLevel = data[0x05];

    InputData.LeftStickX = data[0x06] | (data[0x07] << 8);
    InputData.LeftStickY = data[0x08] | (data[0x09] << 8);
    InputData.RightStickX = data[0x0A] | (data[0x0B] << 8);
    InputData.RightStickY = data[0x0C] | (data[0x0D] << 8);

    InputData.AudioVolume = data[0x0E];

    InputData.AccelX = (s16)(data[0x0F] | (data[0x10] << 8));
    InputData.AccelY = (s16)(data[0x11] | (data[0x12] << 8));
    InputData.AccelZ = (s16)(data[0x13] | (data[0x14] << 8));

    u32 tmp;
    tmp = (data[0x15] << 8) | (data[0x16] << 16) | (data[0x17] << 24);
    InputData.GyroRoll = ((s32)tmp) >> 8;
    tmp = (data[0x18] << 8) | (data[0x19] << 16) | (data[0x1A] << 24);
    InputData.GyroYaw = ((s32)tmp) >> 8;
    tmp = (data[0x1B] << 8) | (data[0x1C] << 16) | (data[0x1D] << 24);
    InputData.GyroPitch = ((s32)tmp) >> 8;

    for (int i = 0; i < 6; i++)
        InputData.MagnetData[i] = data[0x1E + i];

    int nvalid = 0;
    int touchX = 0,  touchY = 0;
    for (int i = 0; i < 10; i++)
    {
        u16 x = data[0x24 + (i*4)] | (data[0x25 + (i*4)] << 8);
        u16 y = data[0x26 + (i*4)] | (data[0x27 + (i*4)] << 8);

        // CHECKME: the last point doesn't have bit15 set on Y, probably a bug
        if ((x | y) & 0x8000)
        {
            touchX += (x & 0xFFF);
            touchY += (y & 0xFFF);
            nvalid++;
        }
    }

    if (nvalid > 0)
    {
        InputData.TouchX = derp_div(touchX, nvalid);
        InputData.TouchY = derp_div(touchY, nvalid);
        InputData.TouchPressed = 1;
    }
    else
    {
        InputData.TouchX = 0;
        InputData.TouchY = 0;
        InputData.TouchPressed = 0;
    }

    u16 press;
    press  = ((data[0x25] & 0x70) >> 4);
    press |= ((data[0x27] & 0x70) >> 1);
    press |= ((data[0x29] & 0x70) << 2);
    press |= ((data[0x2B] & 0x70) << 5);
    InputData.TouchPressure = press;

    return &InputData;
}
