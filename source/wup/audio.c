#include <wup/wup.h>


static u8 VolumeMin, VolumeMax;
static u8 CurVolume;

static u8 CurMute;
static u8 CurOutput;


int Audio_Init()
{
    // retrieve volume min/max
    u8 tmp[4] = {0};
    UIC_ReadEEPROM(0x11A, tmp, 4);
    if (CheckCRC16(tmp, 2))
    {
        VolumeMin = tmp[0];
        VolumeMax = tmp[1];
    }
    else
    {
        memset(tmp, 0, 4);
        UIC_ReadEEPROM(0x19A, tmp, 4);
        if (CheckCRC16(tmp, 2))
        {
            VolumeMin = tmp[0];
            VolumeMax = tmp[1];
        }
        else
        {
            // fail-safe defaults
            VolumeMin = 1;
            VolumeMax = 254;
        }
    }

    if (VolumeMin > VolumeMax)
    {
        u8 tmp = VolumeMin;
        VolumeMin = VolumeMax;
        VolumeMax = tmp;
    }

    // start out muted, this will be updated later
    CurVolume = -1;
    CurMute = -1;
    CurOutput = -1;
    Audio_SetVolume(0);
    Audio_SetOutput(0);

    return 1;
}


void Audio_SetVolume(u8 vol)
{
    if (vol == CurVolume) return;
    CurVolume = vol;

    u8 vmin = VolumeMin + 14;
    u8 vmax = VolumeMax - 14;
    u8 vrange = vmax - vmin;
    u8 vmid = vmin + (vrange >> 1);

    int ampvol;
    if (vol < vmin)
        ampvol = -0x3500;
    else if (vol > vmax)
        ampvol = 0;
    else if (vol < vmid)
        ampvol = -0x3500 + (((vol - vmin) * 0x2600) / (vmid - vmin + 1));
    else
        ampvol = -0xF00 + (((vol - vmid) * 0xF00) / (vmax - vmid + 1));

    AudioAmp_SetVolume(ampvol);
    AudioAmp_SetMute(vol < vmin);
}

void Audio_SetMute(int mute)
{
    /*if (mute == CurMute) return;
    CurMute = mute;
    AudioAmp_SetMute(mute);*/
    // TODO do it at another level, keep things simple
}

void Audio_SetOutput(int output)
{
    if (output == CurOutput) return;
    CurOutput = output;
    AudioAmp_SetOutput(output);
}
