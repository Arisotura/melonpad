#include <wup/wup.h>


static u16 AAmpInit1[0x1E*2] = {
    0x0000, 0,
    0x0101, 2,
    0x0100, 0,
    0x207D, 0,
    0x0400, 0,
    0x7F08, 0,
    0xEC09, 0,
    0x800A, 0,
    0x140B, 0,
    0x7F0C, 0,
    0xD80D, 0,
    0x0000, 0,
    0x8043, 0,
    0x0000, 0,
    0x0304, 0,
    0x9105, 0,
    0x0506, 0,
    0x0E07, 0,
    0xB008, 15,
    0x0C1B, 0,
    0x8E1E, 0,
    0x033C, 0,
    //0x193C, 0,      // MODE FOR BEEPZORZ
    0x820B, 0,
    0x870C, 0,
    0x000D, 0,
    0x800E, 0,
    0x8712, 0,
    0x8213, 0,
    0x8014, 0,
    0x0D33, 0
};

static u16 AAmpInit2[0x17*2] = {
    0x0000, 0,
    0x0043, 0,
    0x0100, 0,
    0x3F21, 0,
    0x7022, 0,
    0x4423, 0,
    0x0100, 0,
    0xC41F, 0,
    0x0028, 0,
    0x0029, 0,
    0x0000, 0,
    0x0E40, 0,
    0x8141, 0,
    0x0100, 0,
    0x022E, 0,
    0xC033, 0,
    0x2030, 0,
    0x8031, 0,
    0x322F, 0,
    0x0000, 0,
    0x0053, 0,
    0x8051, 0,
    0x0052, 0
};

static u16 AAmpInit3[0xB*2] = {
    0x0100, 0,
    0x8024, 0,
    0x8025, 0,
    0x8026, 0,
    0x8027, 0,
    0x0428, 0,
    0x0429, 0,
    0x042A, 0,
    0x042B, 0,
    0x0000, 0,
    0xD53F, 0
};

static u16 AAmpCmdSpeakers[0xC*2] = {
    0x0000, 0,
    0x0E40, 0,
    0x0100, 0,
    0x041F, 0,
    0x0000, 0,
    0x153F, 10,
    0x0800, 0,
    0x0501, 10,
    0x0100, 0,
    0xC620, 0,
    0x0000, 0,
    0xD53F, 0
};

static u16 AAmpCmdHeadphones[0xD*2] = {
    0x0000, 0,
    0x0E40, 0,
    0x0100, 0,
    0x0620, 0,
    0x0000, 0,
    0x153F, 10,
    0x0800, 0,
    0x0501, 10,
    0x0100, 0,
    0xC41F, 0,
    0x0000, 0,
    0xD53F, 0,
    0x0C33, 0
};


static u16 AAmpCoefHeadphones0[0x1E] = {
    0xE852, 0xDB13, 0x65AC, 0x2E7D, 0x1DCF,
    0xE852, 0xDB13, 0x65AC, 0x2E7D, 0x1DCF,
    0xE852, 0xDB13, 0x65AC, 0x2E7D, 0x1DCF,
    0x6589, 0xCD3C, 0x0000, 0x3FD4, 0x0000,
    0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000,
    0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000
};

static u16 AAmpCoefHeadphones1[0x1E] = {
    0xE852, 0xDB13, 0x65AC, 0x2E7D, 0x1DCF,
    0xE852, 0xDB13, 0x65AC, 0x2E7D, 0x1DCF,
    0xE852, 0xDB13, 0x65AC, 0x2E7D, 0x1DCF,
    0x6589, 0x0000, 0x0000, 0x0000, 0x0000,
    0x7FFF, 0x811A, 0x7DCE, 0x7F29, 0x81AC,
    0x7FFF, 0x811A, 0x7DCE, 0x7F29, 0x81AC
};

static u16 AAmpCoefSpeakers[0x1E] = {
    0xEACB, 0xDEEE, 0x5B0E, 0x2E7D, 0x1DCF,
    0xEACB, 0xDEEE, 0x5B0E, 0x2E7D, 0x1DCF,
    0xEACB, 0xDEEE, 0x5B0E, 0x2E7D, 0x1DCF,
    0x7FFF, 0x819C, 0x7D0D, 0x7F07, 0x81AC,
    0x789C, 0xA633, 0x69C6, 0x59CD, 0x9D9D,
    0x7FFF, 0xE788, 0x3D1B, 0x1BA1, 0xAA7A
};

static u8 AAmpType;


static void AudioAmp_ApplyCmdTable(u16* table, int len)
{
    for (int i = 0; i < len; i++)
    {
        u16 addrval = table[i*2];
        u16 delay = table[(i*2)+1];

        AudioAmp_WriteReg(addrval & 0xFF, addrval >> 8);

        if (delay)
            WUP_DelayMS(delay);
    }
}

static void AudioAmp_ApplyCoefTable(u8 addr1, u8 addr2, u16* table, int len)
{
    for (int i = 0; i < len; i++)
    {
        u16 val = table[i];

        AudioAmp_WriteReg(addr1, val >> 8);
        AudioAmp_WriteReg(addr1 + 1, val & 0xFF);
        AudioAmp_WriteReg(addr2, val >> 8);
        AudioAmp_WriteReg(addr2 + 1, val & 0xFF);

        addr1 += 2;
        addr2 += 2;
    }
}


int AudioAmp_Init()
{
    AudioAmp_ApplyCmdTable(AAmpInit1, 0x1E);

    // NOTE: the headset detect register (page 0 register 67) is misused here
    // it serves to identify the motherboard revision (and likely, differences in audio circuitry)
    // 0x80 == revision 20 (VOL/MICDET tied to Vcc)
    // 0xA0 == revision 01 (VOL/MICDET tied to ground)
    u8 headphone = AudioAmp_ReadReg(0x43);
    if (headphone == 0x80)
        AAmpType = 1;
    else
        AAmpType = 0;

    AudioAmp_ApplyCmdTable(AAmpInit2, 0x17);

    AudioAmp_SetPage(0x08);
    AudioAmp_WriteReg(0x01, 0x04);

    if (AAmpType == 1)
        AudioAmp_ApplyCoefTable(0x02, 0x42, AAmpCoefHeadphones1, 0x1E);
    else
        AudioAmp_ApplyCoefTable(0x02, 0x42, AAmpCoefHeadphones0, 0x1E);

    AudioAmp_ApplyCmdTable(AAmpInit3, 0xB);

    return 1;
}

void AudioAmp_DeInit()
{
    AudioAmp_SetPage(0x00);
    AudioAmp_WriteReg(0x40, 0x0E);
    AudioAmp_SetPage(0x01);
    AudioAmp_WriteReg(0x1F, 0x04);
    AudioAmp_WriteReg(0x20, 0x06);
}


u8 AudioAmp_ReadReg(u8 reg)
{
    I2C_Start(I2C_BUS_AUDIO_AMP);

    u8 data = reg;
    u8 ret = 0;
    I2C_Write(I2C_BUS_AUDIO_AMP, I2C_DEV_AUDIO_AMP, &data, 1, 0);
    I2C_Read(I2C_BUS_AUDIO_AMP, I2C_DEV_AUDIO_AMP, &ret, 1);

    I2C_Finish(I2C_BUS_AUDIO_AMP);
    return ret;
}

void AudioAmp_WriteReg(u8 reg, u8 val)
{
    I2C_Start(I2C_BUS_AUDIO_AMP);

    u8 data[2] = {reg, val};
    I2C_Write(I2C_BUS_AUDIO_AMP, I2C_DEV_AUDIO_AMP, data, 2, 0);

    I2C_Finish(I2C_BUS_AUDIO_AMP);
}

void AudioAmp_SetPage(u8 page)
{
    AudioAmp_WriteReg(0x00, page);
}


void AudioAmp_SetVolume(int vol)
{
    u8 volreg;
    if (vol < -0x3F80)
        volreg = 0x81;      // minimum volume (-63.5dB)
    else if (vol > 0x1800)
        volreg = 0x30;      // maximum volume (24dB)
    else
        volreg = ((vol << 1) + 0x80) >> 8;

    AudioAmp_SetPage(0x00);
    AudioAmp_WriteReg(0x41, volreg);
}

void AudioAmp_SetMute(int mute)
{
    AudioAmp_SetPage(0x00);
    AudioAmp_WriteReg(0x40, mute ? 0x0E : 0x02);
}

void AudioAmp_SetOutput(int output)
{
    AudioAmp_SetPage(0x00);
    u8 muted = AudioAmp_ReadReg(0x40);

    AudioAmp_SetPage(0x08);
    if (output == 0)
    {
        // speakers
        AudioAmp_ApplyCoefTable(0x02, 0x42, AAmpCoefSpeakers, 0x1E);
        AudioAmp_ApplyCmdTable(AAmpCmdSpeakers, 0xC);
    }
    else
    {
        // headphones
        if (AAmpType == 1)
            AudioAmp_ApplyCoefTable(0x02, 0x42, AAmpCoefHeadphones1, 0x1E);
        else
            AudioAmp_ApplyCoefTable(0x02, 0x42, AAmpCoefHeadphones0, 0x1E);
        AudioAmp_ApplyCmdTable(AAmpCmdHeadphones, 0xD);
    }

    if (muted != 0x0E)
    {
        AudioAmp_SetPage(0x00);
        AudioAmp_WriteReg(0x40, 0x02);
    }
}
