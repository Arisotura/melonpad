#include <wup/wup.h>


static u8 VolumeMin, VolumeMax;
static u8 CurVolume;

static u8 CurMute;
static u8 CurOutput;

static volatile u8 Playing;
static fnSampleCb SampleCB;
static fnStreamCb StreamCB;
static u32 StreamLength;
static u32 StreamWritePos;


static void AudioIRQ(int irq, void* userdata)
{
    u32 irqreg = REG_AUDIO_IRQ_STATUS;

    if (irqreg & AUDIO_IRQ_ALERT)
    {
        REG_AUDIO_IRQ_STATUS = AUDIO_IRQ_ALERT;

        if (Playing == 1)
        {
            // finished playing sample
            Playing = 0;

            REG_AUDIO_IRQ_ENABLE &= ~(AUDIO_IRQ_ALERT | AUDIO_IRQ_STOP);
            REG_AUDIO_PLAY_CNT |= PLAYCNT_MUTE;

            if (SampleCB)
                SampleCB();
        }
        else if (Playing == 2)
        {
            // stream alert

            // update the half of the buffer which isn't being played
            u32 bufpos = REG_AUDIO_BUF_START + StreamWritePos;
            int buflen = StreamLength >> 1;

            if (StreamCB)
                StreamCB((void*)bufpos, buflen);

            StreamWritePos += buflen;
            if (StreamWritePos >= StreamLength)
                StreamWritePos -= StreamLength;

            // update the alert count for the next IRQ
            REG_AUDIO_ALERT_COUNT -= (buflen >> 3);
        }
    }

    if (irqreg & AUDIO_IRQ_STOP)
    {
        REG_AUDIO_IRQ_STATUS = AUDIO_IRQ_STOP;

        if (Playing)
        {
            Playing = 0;
            SampleCB = NULL;
            StreamCB = NULL;
        }
    }
}

static void MicIRQ(int irq, void* userdata)
{
    //
}


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

    // setup mic
    AudioAmp_SetMicPGA(0x1900);
    //AudioAmp_SetMicVolume(0x80);
    AudioAmp_SetMicVolume(0x1400);
    AudioAmp_SetMicUnk(2);

    // initialize hardware

    Playing = 0;
    SampleCB = NULL;
    StreamCB = NULL;
    StreamLength = 0;
    StreamWritePos = 0;

    REG_MIC_IRQ_DISABLE = MIC_IRQ_ALL;
    REG_MIC_CNT = MIC_UNK3 | MIC_UNK5 | MIC_UNK8 | MIC_UNK15 | MIC_I2S_OFFSET(1) | MIC_UNK30;

    REG_AUDIO_CNT = AUDIO_ENABLE | AUDIO_I2S_OFFSET(1) | AUDIO_I2S_WIDTH(32) | AUDIO_UNK19;
    REG_AUDIO_UNK04 |= 0x20;
    REG_AUDIO_PLAY_CNT |= PLAYCNT_MUTE;
    REG_AUDIO_UNK1C |= 0x1;
    REG_AUDIO_IRQ_ENABLE = 0;

    WUP_SetIRQHandler(IRQ_AUDIO, AudioIRQ, NULL, 0);
    WUP_SetIRQHandler(IRQ_MIC, MicIRQ, NULL, 0);

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

void Audio_SetChanOrder(int order)
{
    u32 cnt = REG_AUDIO_CNT;
    cnt &= ~(1<<5);
    cnt |= (order & (1<<5));
    REG_AUDIO_CNT = cnt;
}


static int SetupAudioRegs(void* buffer, int length, int format, int freq, int chans)
{
    u32 bufptr = (u32)buffer;
    if (bufptr & 0xF)
        return 0;
    if (length & 0xF)
        return 0;

    REG_AUDIO_BUF_START = bufptr;
    REG_AUDIO_BUF_END = bufptr + length;

    u32 cnt = REG_AUDIO_CNT;

    cnt &= ~(3<<23);
    switch (format)
    {
        case AUDIO_FORMAT_PCM16:
            cnt |= AUDIO_FMT_PCM16;
            break;
        case AUDIO_FORMAT_PCM8_MULAW:
            cnt |= AUDIO_FMT_PCM8 | AUDIO_PCM8_MULAW;
            break;
        case AUDIO_FORMAT_PCM8_ALAW:
            cnt |= AUDIO_FMT_PCM8 | AUDIO_PCM8_ALAW;
            break;
        default:
            return 0;
    }

    if (freq == AUDIO_FREQ_24KHZ)
        cnt |= AUDIO_RATE_HALF;
    else
        cnt &= ~AUDIO_RATE_HALF;

    REG_AUDIO_CNT = cnt;

    // set mono flag
    if (chans == 1)
        REG_AUDIO_UNK04 |= 0x80;
    else
        REG_AUDIO_UNK04 &= ~0x80;

    REG_AUDIO_IRQ_ENABLE |= (AUDIO_IRQ_ALERT | AUDIO_IRQ_STOP);
    REG_AUDIO_PLAY_CNT &= ~PLAYCNT_MUTE;

    return 1;
}

int Audio_PlaySample(void* buffer, int length, int format, int freq, int chans, fnSampleCb callback)
{
    if (!callback)
        return 0;
    if (Playing)
        return 0;

    if (!SetupAudioRegs(buffer, length, format, freq, chans))
        return 0;

    // trigger IRQ at end of buffer
    REG_AUDIO_ALERT_COUNT = 0;

    SampleCB = callback;
    Playing = 1;

    // play buffer
    REG_AUDIO_PLAY_END = (u32)buffer;
    return 1;
}

int Audio_StartStream(void* buffer, int length, int format, int freq, int chans, fnStreamCb callback)
{
    if (!callback)
        return 0;
    if (Playing)
        return 0;

    if (!SetupAudioRegs(buffer, length, format, freq, chans))
        return 0;

    // trigger IRQ at midpoint of buffer
    REG_AUDIO_ALERT_COUNT = length >> 4;

    StreamCB = callback;
    StreamLength = length;
    StreamWritePos = 0;
    Playing = 2;

    // play buffer
    // set the address to the end of the buffer, this will loop the buffer
    REG_AUDIO_PLAY_END = (u32)buffer + length;
    return 1;
}

void Audio_Stop()
{
    if (!Playing)
        return;

    REG_AUDIO_IRQ_ENABLE &= ~AUDIO_IRQ_ALERT;

    // stop playback
    REG_AUDIO_PLAY_CNT |= PLAYCNT_STOP;
    while (Playing) WaitForIRQ();

    REG_AUDIO_IRQ_ENABLE &= ~AUDIO_IRQ_STOP;
    REG_AUDIO_PLAY_CNT |= PLAYCNT_MUTE;
}
