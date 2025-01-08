#include <wup/wup.h>
#include <malloc.h>


#ifndef OFFSETOF
#define	OFFSETOF(type, member)	((u32)&((type*)0)->member)
#endif

#define READ16LE(buf, off)  ((buf)[(off)] | ((buf)[(off)+1] << 8))
#define READ32LE(buf, off)  ((buf)[(off)] | ((buf)[(off)+1] << 8) | ((buf)[(off)+2] << 16) | ((buf)[(off)+3] << 24))

#define READ16BE(buf, off)  ((buf)[(off)+1] | ((buf)[(off)] << 8))
#define READ32BE(buf, off)  ((buf)[(off)+3] | ((buf)[(off)+2] << 8) | ((buf)[(off)+1] << 16) | ((buf)[(off)] << 24))


static u32 RAMSize;

#define Flag_RxMail     (1<<0)
#define Flag_RxCtl      (1<<1)
#define Flag_RxEvent    (1<<2)
#define Flag_RxData     (1<<3)

static u8 TxSeqno;
static u16 TxCtlId;

static volatile u8 RxFlags;
static u32 RxMail;
static u8 RxCtlBuffer[4096];
static u8 RxEventBuffer[512];
static u8 RxDataBuffer[4096];

typedef struct
{
    void* Next;
    u32 Length;
    u8 Data[1];

} sPacket;

sPacket* PktQueueHead;
sPacket* PktQueueTail;

void Wifi_ResetRxFlags(u8 flags);
void Wifi_WaitForRx(u8 flags);

void Wifi_InitIoctls();
void send_binary(u8* data, int len);

int Wifi_Init()
{
    printf("wifi init\n");

    PktQueueHead = NULL;
    PktQueueTail = NULL;

    u32 regval;

    regval = 0;
    SDIO_ReadCardRegs(1, 0x10009, 1, (u8*)&regval);
    if (regval & (1<<3))
    {
        // clear I/O isolation bit
        regval &= ~(1<<3);
        SDIO_WriteCardRegs(1, 0x10009, 1, (u8*)&regval);
    }

    SDIO_SetClocks(1, SDIO_CLOCK_FORCE_HWREQ_OFF | SDIO_CLOCK_REQ_ALP);
    SDIO_SetClocks(1, SDIO_CLOCK_FORCE_HWREQ_OFF | SDIO_CLOCK_FORCE_ALP);

    regval = 0;
    SDIO_WriteCardRegs(1, 0x1000F, 1, (u8*)&regval);

    if (!Wifi_AI_Enumerate())
        return 0;

    u32 tmp = 0;
    SDIO_ReadF1Memory(0x1800002C, (u8*)&tmp, 4);
    printf("CHIPSTATUS=%08X\n", tmp);
    tmp = 0;
    SDIO_ReadF1Memory(0x18000004, (u8*)&tmp, 4);
    printf("CAPS=%08X\n", tmp);

    u32 pmu = 0;
    SDIO_ReadF1Memory(0x18000604, (u8*)&pmu, 4);
    printf("PMU CAPS=%08X\n", pmu);

    Wifi_AI_SetCore(WIFI_CORE_ARMCM3);
    //Wifi_AI_IsCoreUp();
    Wifi_AI_DisableCore(0);
    //Wifi_AI_ResetCore(0, 0);

    // TODO: only do this if the backplane core revision is >= 20
    // our core is rev 35
    {
        Wifi_AI_SetCore(WIFI_CORE_BACKPLANE);
        //Wifi_AI_IsCoreUp();
        u32 base = Wifi_AI_GetCoreMemBase();
        printf("backplane base=%08X\n", base);

        regval = 0;
        SDIO_WriteF1Memory(base+0x058, (u8*)&regval, 4);
        regval = 0;
        SDIO_WriteF1Memory(base+0x05C, (u8*)&regval, 4);

        // only if revision >= 21
        regval = 0;
        SDIO_ReadF1Memory(base+0x008, (u8*)&regval, 4);
        regval |= 0x4;
        SDIO_WriteF1Memory(base+0x008, (u8*)&regval, 4);
    }

    // si_attach done

    // TODO? si_socram_size() potentially writes to regs
    // only if we got banks of memory, and we don't.
    RAMSize = Wifi_AI_GetRAMSize();
    if (!RAMSize)
        return 0;
    printf("WIFI RAM SIZE = %08X\n", RAMSize);

    // reset backplane upon SDIO reset
    Wifi_AI_SetCore(WIFI_CORE_SDIOD);
    u32 base = Wifi_AI_GetCoreMemBase();
    printf("SDIOD base=%08X\n", base);
    regval = 0;
    SDIO_ReadF1Memory(base+0x000, (u8*)&regval, 4);
    printf("SDIO shito regval=%08X\n", regval);
    regval |= (1<<1);
    SDIO_WriteF1Memory(base+0x000, (u8*)&regval, 4);

    // removeme?
    {
        // stupid shit
        regval = (1<<1); // enable F1
        if (!SDIO_WriteCardRegs(0, 0x2, 1, (u8*)&regval))
            return 0;
    }
    SDIO_SetClocks(1, 0);

    if (!Wifi_UploadFirmware())
        return 0;

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);
    SDIO_SetClocks(1, SDIO_CLOCK_FORCE_HT);

    {
        Wifi_AI_SetCore(WIFI_CORE_SDIOD);
        u32 base = Wifi_AI_GetCoreMemBase();

        // enable frame transfers
        regval = (4 << 16);
        SDIO_WriteF1Memory(base + 0x48, (u8*)&regval, 4);

        // enable F2
        u8 fn = (1<<1) | (1<<2);
        regval = fn;
        SDIO_WriteCardRegs(0, 0x2, 1, (u8*)&regval);

        // wait for it to be ready
        // TODO have a timeout here?
        for (;;)
        {
            regval = 0;
            SDIO_ReadCardRegs(0, 0x3, 1, (u8*)&regval);
            if (regval == fn) break;
            WUP_DelayUS(1);
        }

        printf("F2 came up? %02X = %02X\n", regval, 6);

        // enable interrupts
        regval = 0x200000F0;
        SDIO_WriteF1Memory(base + 0x24, (u8*)&regval, 4);

        // set watermark
        regval = 8;
        SDIO_WriteCardRegs(1, 0x10008, 1, (u8*)&regval);
    }

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);

    TxSeqno = 0xFF;
    TxCtlId = 1;
    RxFlags = 0;
    SDIO_EnableCardIRQ();

    Wifi_InitIoctls();

    /*for (int i = 0; i < 6; i++)
    {
        u32 addr = 0x18100500 + (i<<12);
        u32 val;
        SDIO_ReadF1Memory(addr, (u8*)&val, 4);
        printf("core %d: status=%08X\n", i, val);
    }*/

    SDIO_SetClocks(0, 0);
    printf("Wifi: ready to go\n");
    return 1;
}


/*
 * ProcessVars:Takes a buffer of "<var>=<value>\n" lines read from a file and ending in a NUL.
 * also accepts nvram files which are already in the format of <var1>=<value>\0\<var2>=<value2>\0
 * Removes carriage returns, empty lines, comment lines, and converts newlines to NULs.
 * Shortens buffer as needed and pads with NULs.  End of buffer is marked by two NULs.
*/

unsigned int
process_nvram_vars(char *varbuf, unsigned int len)
{
    char *dp;
    int findNewline;
    int column;
    unsigned int buf_len, n;
    unsigned int pad = 0;

    dp = varbuf;

    findNewline = 0;
    column = 0;

    for (n = 0; n < len; n++) {
        if (varbuf[n] == '\r')
            continue;
        if (findNewline && varbuf[n] != '\n')
            continue;
        findNewline = 0;
        if (varbuf[n] == '#') {
            findNewline = 1;
            continue;
        }
        if (varbuf[n] == '\n') {
            if (column == 0)
                continue;
            *dp++ = 0;
            column = 0;
            continue;
        }

        *dp++ = varbuf[n];
        column++;
    }
    buf_len = (unsigned int)(dp - varbuf);
    if (buf_len % 4) {
        pad = 4 - buf_len % 4;
        if (pad && (buf_len + pad <= len)) {
            buf_len += pad;
        }
    }

    while (dp < varbuf + n)
        *dp++ = 0;

    return buf_len;
}


int Wifi_SetUploadState(int state)
{
    if (state)
    {
        if (!Wifi_AI_SetCore(WIFI_CORE_ARMCM3))
            return 0;

        Wifi_AI_DisableCore(0);

        if (!Wifi_AI_SetCore(WIFI_CORE_SOCRAM))
            return 0;

        Wifi_AI_ResetCore(0, 0);

        u32 zero = 0;
        SDIO_WriteF1Memory(RAMSize - 4, (u8*)&zero, 4);
    }
    else
    {
        if (!Wifi_AI_SetCore(WIFI_CORE_SDIOD))
            return 0;

        u32 base = Wifi_AI_GetCoreMemBase();
        u32 ff = 0xFFFFFFFF;
        SDIO_WriteF1Memory(base+0x020, (u8*)&ff, 4);

        if (!Wifi_AI_SetCore(WIFI_CORE_ARMCM3))
            return 0;

        Wifi_AI_ResetCore(0, 0);
    }

    return 1;
}

int Wifi_UploadFirmware()
{
    u32 fw_offset, fw_length;
    u32 nv_offset, nv_length;

    if (!Flash_GetEntryInfo("WIFI", &fw_offset, &fw_length, NULL))
        return 0;
    if (!Flash_GetEntryInfo("WNVR", &nv_offset, &nv_length, NULL))
        return 0;

    int tmplen = 0x2000;
    u8* tmpbuf = (u8*)memalign(16, tmplen);

    if (!SDIO_SetClocks(1, SDIO_CLOCK_REQ_ALP)) return 0;
    if (!Wifi_SetUploadState(1)) return 0;

    for (int i = 0; i < fw_length; )
    {
        int chunk = tmplen;
        if ((i + chunk) > fw_length)
            chunk = fw_length - i;

        Flash_Read(fw_offset + i, tmpbuf, chunk);

        if (!SDIO_WriteF1Memory(i, tmpbuf, chunk))
            return 0;

        i += chunk;
    }

#if 0
    printf("firmware upload done, verifying\n");

    u8* verbuf = (u8*)memalign(16, tmplen);
    for (int i = 0; i < fw_length; )
    {
        int chunk = tmplen;
        if ((i + chunk) > fw_length)
            chunk = fw_length - i;

        Flash_Read(fw_offset + i, tmpbuf, chunk);

        if (!SDIO_ReadF1Memory(i, verbuf, chunk))
        {
            printf("read failed\n");
            return 0;
        }

        if (memcmp(tmpbuf, verbuf, chunk))
        {
            printf("%08X: error\n", i);
            return 0;
        }

        i += chunk;
    }
    printf("OK\n");
    free(verbuf);
#endif
    printf("firmware uploaded\n");

    free(tmpbuf);

    tmpbuf = (u8*)memalign(16, nv_length + 16);
    Flash_Read(nv_offset, tmpbuf, nv_length);
    u32 var_length = process_nvram_vars((char*)tmpbuf, nv_length);
    tmpbuf[var_length++] = 0;

    var_length = (var_length + 3) & ~3;
    u32 var_addr = RAMSize - 4 - var_length;
    if (!SDIO_WriteF1Memory(var_addr, tmpbuf, var_length))
        return 0;

    u32 len_token = (var_length >> 2) & 0xFFFF;
    len_token = len_token | ((~len_token) << 16);
    if (!SDIO_WriteF1Memory(RAMSize - 4, (u8*)&len_token, 4))
        return 0;

    free(tmpbuf);
    printf("NVRAM uploaded\n");

    Wifi_SetUploadState(0);
    SDIO_SetClocks(1, 0);

    return 1;
}

u8 fazil[2048+28];
int Wifi_SendIoctl(u32 opc, u16 flags, u8* data_in, u32 len_in, u8* data_out, u32 len_out)
{
    int ret = 1;

    //u8 buf[256];
    u8* buf = &fazil[0];
    u16 wanted_id = TxCtlId;

    int totallen = len_in + 16;
    if (totallen > 1518)
        totallen = 1518;
    totallen += 12;

    //u16 totallen = len_in + 28;
    *(u16*)&buf[0] = totallen;
    *(u16*)&buf[2] = ~totallen;
    buf[4] = TxSeqno++;
    buf[5] = 0; // channel (control)
    buf[6] = 0;
    buf[7] = 12; // offset to data
    *(u32*)&buf[8] = 0;
    *(u32*)&buf[12] = opc;
    *(u32*)&buf[16] = len_in;
    *(u32*)&buf[20] = (TxCtlId++ << 16) | flags;
    *(s32*)&buf[24] = 0; // status
    memcpy(&buf[28], data_in, len_in);

    Wifi_ResetRxFlags(Flag_RxCtl);

    u16 len_rounded = (totallen + 0x3F) & ~0x3F;
    printf("sending %d bytes\n", len_rounded);
    //send_binary(buf, len_rounded);
    SDIO_WriteCardData(2, 0x8000, buf, len_rounded, 0);
printf("sent\n");
    for (;;)
    {
        Wifi_WaitForRx(Flag_RxCtl);
        //if ((vu8)RxCtlBuffer[4] == TxSeqno)
        //    break;
        //send_binary(RxCtlBuffer, totallen);
        break;

        Wifi_ResetRxFlags(Flag_RxCtl);
    }

    //if (data_out)
    {
        int irq = DisableIRQ();

        u8 replyoffset = RxCtlBuffer[7];
        printf("len=%04X cpl=%04X off=%02X\n", *(u16*)&RxCtlBuffer[0], *(u16*)&RxCtlBuffer[2], replyoffset);
        u32 replyopc = *(u32*)&RxCtlBuffer[replyoffset];
        u32 replyflags = *(u32*)&RxCtlBuffer[replyoffset+8];
        s32 replystatus = *(s32*)&RxCtlBuffer[replyoffset+12];

        if (replyopc != opc)
        {
            printf("Wifi_SendIoctl: wrong opcode %d, expected %d\n", replyopc, opc);
            ret = 0;
        }
        if ((replyflags >> 16) != wanted_id)
        {
            printf("Wifi_SendIoctl: wrong control ID %d, expected %d\n", (replyflags>>16), wanted_id);
            ret = 0;
        }
        if (replyflags & 1)
        {
            printf("Wifi_SendIoctl: error in opcode %d, status=%d\n", opc, replystatus);
            ret = 0;
        }

        u32 replylen = *(u32*)&RxCtlBuffer[replyoffset+4];
        if (replylen > len_out)
        {
            printf("Wifi_SendIoctl: response length of %d exceeds provided length (%d), truncating\n", replylen, len_out);
            replylen = len_out; // aaaa
        }

        if (data_out)
            memcpy(data_out, &RxCtlBuffer[replyoffset+16], replylen);

        RestoreIRQ(irq);
    }

    return ret;
}

int Wifi_GetVar(char* var, u8* data, u32 len)
{
    u8 buf[128];

    int varlen = strlen(var);
    memcpy(buf, var, varlen);
    buf[varlen] = 0;

    if (!Wifi_SendIoctl(262, 0, buf, 128, buf, 128))
        return 0;

    memcpy(data, buf, len);
    return 1;
}

int Wifi_SetVar(char* var, u8* data, u32 len)
{
    u8 buf[128];

    printf("Wifi_SetVar(%s)\n", var);
    int varlen = strlen(var);
    memcpy(buf, var, varlen);
    buf[varlen] = 0;
    memcpy(&buf[varlen+1], data, len);

    if (!Wifi_SendIoctl(263, 2, buf, 128, buf, 128))
        return 0;

    return 1;
}

void Wifi_InitIoctls()
{
    u32 var, resp;
    int res;

    // get MAC address
    u8 macaddr[6];
    res = Wifi_GetVar("cur_etheraddr", macaddr, 6);
    printf("res=%d, mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
           res, macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

    // set roam_off
    var = 1;
    res = Wifi_SetVar("roam_off", (u8*)&var, 4);
    printf("res=%d\n", res);

    var = 23456;
    res = Wifi_SendIoctl(109, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("-- res=%d, gmode=%d\n", res, var);

    var = 23456;
    res = Wifi_GetVar("nmode", (u8*)&var, 4);
    printf("-- res=%d, nmode=%d\n", res, var);

    var = 23456;
    res = Wifi_GetVar("nmode_protection", (u8*)&var, 4);
    printf("-- res=%d, nmodeproot=%d\n", res, var);

    var = 1;
    res = Wifi_SetVar("nmode", (u8*)&var, 4);
    printf("-- set nmode, res=%d\n", res);

    var = 23456;
    res = Wifi_GetVar("nmode", (u8*)&var, 4);
    printf("-- res=%d, nmode=%d\n", res, var);

    u32 bands[25];
    res = Wifi_SendIoctl(140, 0, (u8*)bands, 100, (u8*)bands, 100);
    printf("bands res=%d -- %d %d %d\n", res, bands[0], bands[1], bands[2]);
    // wlfirmware.bin:       1 1 1
    // wlfirmware_maybeopen: 1 2 1
    // wlfirmware2:          1 2 1
    // wlfirmware2 does not set MAC addr properly?

    // set band
    //var = 1;
    var = 1; // 0=auto 1=5GHz 2=2.4GHz
    res = Wifi_SendIoctl(142, 2, (u8*)&var, 4, (u8*)&resp, 4);
    printf("res=%d\n", res);

    // sgi_tx & sgi_rx
    var = 0;
    res = Wifi_SetVar("sgi_tx", (u8*)&var, 4);
    printf("res=%d\n", res);
    var = 0;
    res = Wifi_SetVar("sgi_rx", (u8*)&var, 4);
    printf("res=%d\n", res);

    var = 4;
    res = Wifi_SetVar("ampdu_txfail_event", (u8*)&var, 4);
    printf("res=%d\n", res);

    var = 1;
    res = Wifi_SetVar("ac_remap_mode", (u8*)&var, 4);
    printf("res=%d\n", res);

    u8 event_msgs[16];
    res = Wifi_GetVar("event_msgs", event_msgs, 16);
    printf("res=%d evt=%08X/%08X/%08X/%08X\n",
           res, *(u32*)&event_msgs[0], *(u32*)&event_msgs[4],
           *(u32*)&event_msgs[8], *(u32*)&event_msgs[12]);
    // 00000000/00400000/00000000/3100CD7C
    *(u32*)&event_msgs[0] |= 0x04001FFF;
    *(u32*)&event_msgs[8] |= 0x20;

    //memset(event_msgs, 0, 16);
    res = Wifi_SetVar("event_msgs", event_msgs, 16);
    printf("res=%d\n", res);

    // get radio disable
    var = 1;
    res = Wifi_SendIoctl(37, 0, (u8*)&var, 1, (u8*)&var, 1);
    printf("res=%d radiodisable=%02X\n", res, (u8)var);

    // get SRL
    var = 0;
    res = Wifi_SendIoctl(31, 0, (u8*)&var, 1, (u8*)&var, 1);
    printf("res=%d SRL=%02X\n", res, (u8)var);

    var = 0;
    res = Wifi_GetVar("ampdu", (u8*)&var, 4);
    printf("res=%d ampdu=%08X\n", res, var);

    var = 0;
    res = Wifi_GetVar("country", (u8*)&var, 4);
    printf("res=%d country=%08X\n", res, var);

    // get channel
    var = 0xFF;
    res = Wifi_SendIoctl(29, 0, (u8*)&var, 1, (u8*)&resp, 1);
    printf("res=%d chan=%02X\n", res, (u8)resp);

    // get TX antenna
    var = 0xFF;
    res = Wifi_SendIoctl(61, 0, (u8*)&var, 1, (u8*)&resp, 1);
    printf("res=%d txant=%02X\n", res, (u8)resp);

    // get antenna div
    var = 0xFF;
    res = Wifi_SendIoctl(63, 0, (u8*)&var, 1, (u8*)&resp, 1);
    printf("res=%d antdiv=%02X\n", res, (u8)resp);

    // system up
    var = 1;
    res = Wifi_SendIoctl(2, 0, (u8*)&var, 4, (u8*)&resp, 4);
    printf("res=%d resp=%08X\n", res, resp);

    // set lifetime
    u32 lifetimes[] = {5, 25, 15, 16, 0};
    for (int i = 0; lifetimes[i]; i++)
    {
        u32 data[2] = {i, lifetimes[i]};
        res = Wifi_SetVar("lifetime", (u8*)data, 8);
        printf("res=%d\n", res);
    }

    /*var = 0;
    res = Wifi_GetVar("devid", (u8*)&var, 2);
    printf("res=%d devid=%04X\n", res, var);*/
    for (int i = 0; i < 6; i++)
    {
        u32 base = 0x18100000 + (i << 12);
        u32 lo, hi;
        SDIO_ReadF1Memory(base+0x408, (u8*)&lo, 4);
        SDIO_ReadF1Memory(base+0x500, (u8*)&hi, 4);
        printf("%d: lo=%08X hi=%08X\n", i, lo, hi);
    }

    /*SDIO_ReadF1Memory(0x18101408, (u8*)&var, 4);
    var |= 0x2000; // band switch
    SDIO_WriteF1Memory(0x18101408, (u8*)&var, 4);
    WUP_DelayMS(50);*/

    /*SDIO_ReadF1Memory(0x18101408, (u8*)&var, 4);
    var &= ~0x1;
    SDIO_WriteF1Memory(0x18101408, (u8*)&var, 4);
    WUP_DelayMS(50);

    SDIO_ReadF1Memory(0x18101408, (u8*)&var, 4);
    var &= ~0x2000; // band switch
    var |= 0x8; // reset
    SDIO_WriteF1Memory(0x18101408, (u8*)&var, 4);
    WUP_DelayMS(50);

    SDIO_ReadF1Memory(0x18101408, (u8*)&var, 4);
    var |= 0x2000; // band switch
    var &= ~0x8;
    SDIO_WriteF1Memory(0x18101408, (u8*)&var, 4);
    WUP_DelayMS(50);

    SDIO_ReadF1Memory(0x18101408, (u8*)&var, 4);
    printf("new flags: %08X\n", var);

    res = Wifi_SendIoctl(140, 0, (u8*)bands, 100, (u8*)bands, 100);
    printf("bands res=%d -- %d %d %d\n", res, bands[0], bands[1], bands[2]);*/
}



void Wifi_ResetRxFlags(u8 flags)
{
    RxFlags &= ~flags;
}
volatile int derp = 0; volatile u32 farpo;
void Wifi_WaitForRx(u8 flags)
{
    while (!(RxFlags & flags))
    {
        WaitForIRQ();
        if (derp) printf("derp=%d %08X\n", derp, farpo);
    }
}

void Wifi_RxHostMail()
{
    u32 base = Wifi_AI_GetCoreMemBase();
    SDIO_ReadF1Memory(base + 0x04C, (u8*)&RxMail, 4);
    u32 ack = (1<<1);
    SDIO_WriteF1Memory(base + 0x040, (u8*)&ack, 4);

    RxFlags |= Flag_RxMail;
}
int _write(int file, char *ptr, int len);
u8 fazilpet[64];

void Wifi_RxData()
{
    // read header
    //u8 header[64];
    u8* header = &fazilpet[0];
    SDIO_ReadCardData(2, 0x8000, header, 64, 0);

    u16 len = *(u16*)&header[0];
    u16 not_len = *(u16*)&header[2];
    if (len < 12) derp |= 1;
    if (len != (u16)(~not_len)) derp |= 2;
    if (len < 12) return;
    if (len != (u16)(~not_len)) return;

    u8 seqno = header[4];
    u8 chan = header[5];
    u8 dataoffset = header[7];
    if (dataoffset < 12) derp |= 4;
    if (dataoffset < 12) return;

    // TODO account for credits

    if (chan == 0)
    {
        // control channel

        u16 len_rounded = (len + 0x3F) & ~0x3F;
        //if (len_rounded > 512) derp |= 8;
        if (len_rounded > 4096) derp |= 8;
        farpo = len;
        //if (len_rounded > 512) return;
        if (len_rounded > 4096) return;

        memcpy(RxCtlBuffer, header, 64);
        if (len_rounded > 64)
            SDIO_ReadCardData(2, 0x8000, &RxCtlBuffer[64], len_rounded-64, 0);

        //_write(1, "ctl__\n", 6);
        RxFlags |= Flag_RxCtl;
    }
    else if (chan == 1)
    {
        // event channel

        u16 len_rounded = (len + 0x3F) & ~0x3F;

        int pkt_len = OFFSETOF(sPacket, Data) + len_rounded;
        sPacket* pkt = (sPacket*)malloc(pkt_len);
        if (!pkt)
        {
            // TODO signal the error somehow
            _write(1, "FAIL1\n", 6);
            return;
        }

        pkt->Next = NULL;
        pkt->Length = len_rounded;// - dataoffset;

        // copy all the data first
        // the length we read from the FIFO needs to be aligned to the block size boundary
        memcpy(pkt->Data, header, 64);
        if (len_rounded > 64)
            SDIO_ReadCardData(2, 0x8000, &pkt->Data[64], len_rounded - 64, 0);

        // knock out the headers
        /*int datalen = len - dataoffset;
        memmove(pkt->Data, &pkt->Data[dataoffset], datalen);

        // reallocate the buffer
        int pkt_newlen = OFFSETOF(sPacket, Data) + datalen;
        sPacket* newpkt = (sPacket*)realloc(pkt, pkt_newlen);
        if (!newpkt)
        {
            free(pkt);
            // TODO signal the error somehow
            _write(1, "FAIL2\n", 6);
            return;
        }

        pkt = newpkt;*/
        if (!PktQueueHead)
            PktQueueHead = pkt;
        if (PktQueueTail)
            PktQueueTail->Next = pkt;
        PktQueueTail = pkt;

        RxFlags |= Flag_RxEvent;
        _write(1, "event\n", 6);

        /*if (len_rounded > 512) return;

        memcpy(RxEventBuffer, header, 64);
        if (len_rounded > 64)
            SDIO_ReadCardData(2, 0x8000, &RxEventBuffer[64], len_rounded-64, 0);

        RxFlags |= Flag_RxEvent;
        _write(1, "event\n", 6);*/

        /*got event
00000000: 5A 00 A5 FF 19 01 00 0E 00 1D 00 00 00 00 10 00
00000010: 00 00 40 F4 07 EA 66 19 42 F4 07 EA 66 19 88 6C
00000020: 80 01 00 64 00 00 10 18 00 01 00 01 00 00 00 00
00000030: 00 00 00 00 00 03 00 00 00 00 00 00 00 00 00 00
00000040: 00 00 00 00 00 00 00 00 77 6C 30 00 00 00 00 00
00000050: 00 00 00 00 00 00 00 00 00 00 */
        // format is ethernet frame-like
        // 886C == broadcom eth type
        // 8 bytes header then event data
        // see bcm_event_t and wl_event_msg_t for formats
    }
    else
    {
        //_write(1, "?????\n", 6);
    }
}

void Wifi_CardIRQ()
{
    u32 irqstatus = 0;
    u32 base = Wifi_AI_GetCoreMemBase();
    SDIO_ReadF1Memory(base + 0x020, (u8*)&irqstatus, 4);
    if (irqstatus == 0) return;
    SDIO_WriteF1Memory(base + 0x020, (u8*)&irqstatus, 4); // ack

    if (irqstatus & (1<<7)) Wifi_RxHostMail();
    if (irqstatus & (1<<6)) Wifi_RxData();

    /*if (irqstatus & (1<<6))
    {
        if (!(RxFlags & Flag_RxCtl))
            derp++;
    }*/
}


sPacket* Wifi_ReadRxPacket()
{
    int irq = DisableIRQ();

    sPacket* pkt = PktQueueHead;
    if (!pkt)
    {
        RestoreIRQ(irq);
        return NULL;
    }

    PktQueueHead = pkt->Next;
    if (!PktQueueHead)
        PktQueueTail = NULL;

    RestoreIRQ(irq);
    return pkt;
}


u8 derpderp[256];
int Wifi_StartScan(int passive)
{
    int res;

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);


    /*u32 var = (u32)passive;
    res = Wifi_SendIoctl(49, 2, (u8*)&var, 4, (u8*)&var, 4);
    if (!res) return 0;*/

    // u32 SSID length
    // then SSID
    /*u8 ssid_buf[36];
    memset(ssid_buf, 0, 36);
    res = Wifi_SendIoctl(50, 2, ssid_buf, 36, ssid_buf, 36);
    if (!res) return 0;

   // RxFlags = 0;
    printf("scanning, I guess\n");
    WUP_DelayMS(5000);
    printf("checking res\n");
    //printf("status = %02X\n", RxFlags);


    u8 derp[128];
    res = Wifi_SendIoctl(51, 0, derp, 128, derp, 128);
    printf("res=%d\n", res);*/

    /*u8 macaddr[6];
    res = Wifi_GetVar("cur_etheraddr", macaddr, 6);
    printf("res=%d, mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
           res, macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);*/

    /*u32 chan = 11;
    res = Wifi_SendIoctl(30, 2, (u8*)&chan, 4, (u8*)&chan, 4);
    printf("chan res = %d\n", res);*/

    //u8 scanparams[256];
    u8* scanparams = &derpderp[0];

    //memset(scanparams, 0, sizeof(scanparams));
    memset(scanparams, 0, 256);
    // bsstype = 2
    *(u32*)&scanparams[0] = 1; // version
    *(u16*)&scanparams[4] = 1; // action: start
    *(u16*)&scanparams[6] = 0; // duration
    //*(u16*)&scanparams[6] = 1000; // duration

    memset(&scanparams[8+36], 0xFF, 6); // BSSID
    scanparams[8+42] = 2; // BSS type
    scanparams[8+43] = 1; // scan type (passive)
    //scanparams[8+43] = 0; // scan type (active)
    *(s32*)&scanparams[8+44] = -1; // nprobes
    *(s32*)&scanparams[8+48] = -1; // active_time
    *(s32*)&scanparams[8+52] = -1; // passive_time
    *(s32*)&scanparams[8+56] = -1; // home_time
    *(u32*)&scanparams[8+60] = 0; // channel_num

    Wifi_ResetRxFlags(Flag_RxEvent);

    //int paramsize = 256;//72;
    int paramsize = 72;
    //res = Wifi_SetVar("iscan", scanparams, paramsize-6);
    res = Wifi_SetVar("iscan", scanparams, paramsize);
    printf("scan res=%d\n", res);

    Wifi_WaitForRx(Flag_RxEvent);

    //send_binary(RxEventBuffer, 512);
    printf("scan complete\n");

    {
        u8 scanbuf[256];
        memset(scanbuf, 0, 256);
        *(u32*)&scanbuf[4] = 2048; // max len

        char* iovar = "iscanresults";
        int iovlen = strlen(iovar);
        //u8 iobuf[300];
        u8* iobuf = (u8*)malloc(2048);
        memcpy(iobuf, iovar, iovlen);
        iobuf[iovlen] = 0;
        memcpy(&iobuf[iovlen + 1], scanbuf, 256);
        //send_binary(iobuf, 300);

        printf("sending\n");
        //Wifi_SendIoctl(262, 0, iobuf, iovlen + 1 + 16, scanbuf, 256);
        //Wifi_SendIoctl(262, 0, iobuf, 2048, iobuf, 2048);
        int fz = 256;//2048 - (64*3);
        //Wifi_SendIoctl(262, 0, iobuf, 1518, iobuf, 2048);
        Wifi_SendIoctl(262, 0, iobuf, 2048, iobuf, 2048);
        u32 status = *(u32*)&iobuf[0];
        u32 buflen = *(u32*)&iobuf[4];
        printf("STATUS=%08X LEN=%08X\n", status, buflen);
        send_binary(iobuf, 0x800);

        if ((status == 0) && (buflen > 0xC))
        {
            u32 numap = *(u32*)&iobuf[0xC];
            printf("found %d APs (%08X %08X %08X %08X)\n",
                   numap, *(u32*)&iobuf[0x0], *(u32*)&iobuf[0x4], *(u32*)&iobuf[0x8], *(u32*)&iobuf[0xC]);

            u8* apdata = &iobuf[0x10];
            u8* apend = &iobuf[buflen];
            for (u32 j = 0; j < numap && apdata < apend; j++)
            {
                u32 aplen = *(u32*)&apdata[0x04];

                u8 bssid[6];
                memcpy(bssid, &apdata[0x08], 6);

                char ssid[33];
                u8 ssidlen = apdata[0x12];
                if (ssidlen > 32) ssidlen = 32;
                if (ssidlen) memcpy(ssid, &apdata[0x13], ssidlen);
                ssid[ssidlen] = 0;

                u16 chanspec = *(u16*)&apdata[0x48];

                printf("%d. (len=%04X) ch=%04X BSSID=%02X:%02X:%02X:%02X:%02X:%02X, SSID=%s\n",
                       j, aplen, chanspec,
                       bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                       ssid);

                apdata += aplen;
            }
        }

        free(iobuf);
    }

#if 0
    //for (;;)
    {
        for (int i = 0; i < 100; i++)
        {
            WUP_DelayMS(100);
            //WUP_DelayMS(2000);
            printf("waited\n");

            /*u8 derp[256];
            memset(derp, 0, 256);
            res = Wifi_GetVar("iscanresults", derp, 256);
            printf("results res=%d\n", res);*/

            u8 scanbuf[256];
            memset(scanbuf, 0, 256);
            *(u32*)&scanbuf[4] = 2048; // max len

            // buffer: strlen("iscanresults")+1 + 16
            // total length: 2048
            char* iovar = "iscanresults";
            int iovlen = strlen(iovar);
            //u8 iobuf[300];
            u8* iobuf = (u8*)malloc(2048);
            memcpy(iobuf, iovar, iovlen);
            iobuf[iovlen] = 0;
            memcpy(&iobuf[iovlen + 1], scanbuf, 256);
            //send_binary(iobuf, 300);

            printf("sending\n");
            //Wifi_SendIoctl(262, 0, iobuf, iovlen + 1 + 16, scanbuf, 256);
            //Wifi_SendIoctl(262, 0, iobuf, 2048, iobuf, 2048);
            int fz = 256;//2048 - (64*3);
            //Wifi_SendIoctl(262, 0, iobuf, 1518, iobuf, 2048);
            Wifi_SendIoctl(262, 0, iobuf, 2048, iobuf, 2048);
            u32 status = *(u32*)&iobuf[0];
            printf("STATUS=%08X\n", status);

            if (status == 0)
            {
                u32 numap = *(u32*)&iobuf[0xC];
                printf("found %d APs (%08X %08X %08X %08X)\n",
                       numap, *(u32*)&iobuf[0x0], *(u32*)&iobuf[0x4], *(u32*)&iobuf[0x8], *(u32*)&iobuf[0xC]);

                u8* apdata = &iobuf[0x10];
                for (u32 j = 0; j < numap; j++)
                {
                    u32 aplen = *(u32*)&apdata[0x04];

                    u8 bssid[6];
                    memcpy(bssid, &apdata[0x08], 6);

                    char ssid[33];
                    u8 ssidlen = apdata[0x12];
                    if (ssidlen > 32) ssidlen = 32;
                    if (ssidlen) memcpy(ssid, &apdata[0x13], ssidlen);
                    ssid[ssidlen] = 0;

                    u16 chanspec = *(u16*)&apdata[0x48];

                    printf("%d. ch=%04X BSSID=%02X:%02X:%02X:%02X:%02X:%02X, SSID=%s\n",
                           j, chanspec,
                           bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                           ssid);

                    apdata += aplen;
                }
            }

            free(iobuf);
            if (status == 0) break;
        }

        //WUP_DelayMS(1000);
    }
#endif

    /*
     typedef struct wl_iscan_results {
	uint32 status;
	wl_scan_results_t results;
} wl_iscan_results_t;

     typedef struct wl_scan_results {
	uint32 buflen;
	uint32 version;
	uint32 count;
	wl_bss_info_t bss_info[1];
} wl_scan_results_t;

     typedef struct wl_bss_info {
	uint32		version;
	uint32		length;
	struct ether_addr BSSID;
	uint16		beacon_period;
	uint16		capability;
	uint8		SSID_len;
	uint8		SSID[32];
	struct {
		uint	count;
		uint8	rates[16];
	} rateset;
	chanspec_t	chanspec;
	uint16		atim_window;
	uint8		dtim_period;
	int16		RSSI;
	int8		phy_noise;

	uint8		n_cap;
	uint32		nbss_cap;
	uint8		ctl_ch;
	uint32		reserved32[1];
	uint8		flags;
	uint8		reserved[3];
	uint8		basic_mcs[MCSSET_LEN];

	uint16		ie_offset;
	uint32		ie_length;


} wl_bss_info_t;

     memset(&list, 0, sizeof(list));
	list.results.buflen = htod32(WLC_IW_ISCAN_MAXLEN);
     */

    /*memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = 0;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;*/

    /*
     * typedef struct wl_scan_params {
	wlc_ssid_t ssid;
	struct ether_addr bssid;
	int8 bss_type;
	int8 scan_type;
	int32 nprobes;
	int32 active_time;
	int32 passive_time;
	int32 home_time;
	int32 channel_num;
	uint16 channel_list[1];
} wl_scan_params_t;

     #define WL_SCAN_PARAMS_COUNT_MASK 0x0000ffff
#define WL_SCAN_PARAMS_NSSID_SHIFT 16

#define WL_SCAN_ACTION_START      1
#define WL_SCAN_ACTION_CONTINUE   2
#define WL_SCAN_ACTION_ABORT      3

#define ISCAN_REQ_VERSION 1


typedef struct wl_iscan_params {
	uint32 version;
	uint16 action;
	uint16 scan_duration;
	wl_scan_params_t params;
} wl_iscan_params_t;

#define WL_ISCAN_PARAMS_FIXED_SIZE (OFFSETOF(wl_iscan_params_t, params) + sizeof(wlc_ssid_t))

typedef struct wl_scan_results {
	uint32 buflen;
	uint32 version;
	uint32 count;
	wl_bss_info_t bss_info[1];
} wl_scan_results_t;

#define WL_SCAN_RESULTS_FIXED_SIZE 12


#define WL_SCAN_RESULTS_SUCCESS	0
#define WL_SCAN_RESULTS_PARTIAL	1
#define WL_SCAN_RESULTS_PENDING	2
#define WL_SCAN_RESULTS_ABORTED	3*/

    SDIO_SetClocks(0, 0);
    return 1;
}


int Wifi_StartScan2(int passive)
{
    int res;

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);


    /*u32 chan = 48;
    res = Wifi_SendIoctl(30, 2, (u8*)&chan, 4, (u8*)&chan, 4);
    printf("chan res = %d\n", res);*/



    //u8 scanparams[256];
    u8* scanparams = &derpderp[0];

    //memset(scanparams, 0, sizeof(scanparams));
    memset(scanparams, 0, 256);
    // bsstype = 2
    *(u32*)&scanparams[0] = 1; // version
    *(u16*)&scanparams[4] = 1; // action: start
    *(u16*)&scanparams[6] = 1; // duration
    //*(u16*)&scanparams[6] = 1000; // duration

    memset(&scanparams[8+36], 0xFF, 6); // BSSID
    scanparams[8+42] = 2; // BSS type
    scanparams[8+43] = 1; // scan type (passive)
    //scanparams[8+43] = 0; // scan type (active)
    *(s32*)&scanparams[8+44] = -1; // nprobes
    *(s32*)&scanparams[8+48] = -1; // active_time
    *(s32*)&scanparams[8+52] = -1; // passive_time
    *(s32*)&scanparams[8+56] = -1; // home_time
    *(u32*)&scanparams[8+60] = 0; // channel_num

    Wifi_ResetRxFlags(Flag_RxEvent);

    //int paramsize = 256;//72;
    int paramsize = 72;
    res = Wifi_SetVar("escan", scanparams, paramsize);
    printf("scan res=%d\n", res);

    for (;;)
    {
        Wifi_WaitForRx(Flag_RxEvent);

        //send_binary(RxEventBuffer, 512);
        sPacket* pkt;
        while ((pkt = Wifi_ReadRxPacket()))
        {
            printf("got packet: %08X, len=%04X\n", (u32)pkt, pkt->Length);
            //send_binary(pkt->Data, pkt->Length);

            /*int irq = DisableIRQ();
            send_binary(pkt->Data, pkt->Length);
            RestoreIRQ(irq);*/

            u8 dataoffset = pkt->Data[7];
            if (dataoffset >= pkt->Length)
            {printf("bad data offset %02X\n", dataoffset);
                free(pkt);
                continue;
            }

            u8* pktdata = &pkt->Data[dataoffset];

            // TODO more sanity checks
            u16 evtid = READ16BE(pktdata, 0x22);
            if (evtid != 69) // ESCAN data event
            {printf("bad event %04X\n", evtid);
                free(pkt);
                continue;
            }

            if (pkt->Length > 0x58)
            {
                //u32 aplen = READ32LE(pktdata, 0x4A);
                //u16 numap = READ16LE(pktdata, 0x54);
                u32 aplen = READ32LE(pktdata, 0x4C);
                u16 numap = READ16LE(pktdata, 0x56);
                if ((aplen > 0xC) && (numap > 0))
                {
                    // we got AP data
                    printf("-- got %d APs (%d bytes) --\n", numap, aplen);

                    //u8* apdata = &pktdata[0x56];
                    u8* apdata = &pktdata[0x58];
                    u8* apend = &apdata[aplen];
                    for (u32 j = 0; j < numap && apdata < apend; j++)
                    {
                        u32 aplen = READ32LE(apdata, 0x04);

                        u8 bssid[6];
                        memcpy(bssid, &apdata[0x08], 6);

                        char ssid[33];
                        u8 ssidlen = apdata[0x12];
                        if (ssidlen > 32) ssidlen = 32;
                        if (ssidlen) memcpy(ssid, &apdata[0x13], ssidlen);
                        ssid[ssidlen] = 0;

                        u16 chanspec = READ16LE(apdata, 0x48);

                        printf("%d. (len=%04X) ch=%04X BSSID=%02X:%02X:%02X:%02X:%02X:%02X, SSID=%s\n",
                               j, aplen, chanspec,
                               bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                               ssid);

                        apdata += aplen;
                    }
                }
                else
                {
                    // no data: scan end
                    printf("-- scan end --\n");
                    free(pkt);
                    return 1;
                }
            }

            free(pkt);
        }

        Wifi_ResetRxFlags(Flag_RxEvent);

        // 58 = total length of stuff
        // 5C = version??
        // 64 = start of wl_scan_results_t
        /*typedef struct wl_escan_result {
	uint32 buflen;
	uint32 version;
	uint16 sync_id;
	uint16 bss_count;
	wl_bss_info_t bss_info[1];
} wl_escan_result_t;*/
    }
    printf("scan complete\n");


    SDIO_SetClocks(0, 0);
    return 1;
}

// TEST
int Wifi_JoinNetwork()
{
    u32 var;
    int res;
    u8 tmp[100];
    //char* ssid = "ElectricAngel";
    char* ssid = "Freebox-375297"; // TEST
    char* pass = "hdfgfdgjdtyygdf";

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);

    // system up
    /*var = 1;
    res = Wifi_SendIoctl(2, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("res=%d resp=%08X\n", res, var);*/

    /*char country_data[20] = "XX\x00\x00\xFF\xFF\xFF\xFFXX";
    //res = Wifi_SetVar("country", (u8*)country_data, sizeof(country_data));
    res = Wifi_SendIoctl(84, 2, (u8*)country_data, sizeof(country_data), (u8*)country_data, sizeof(country_data));
    printf("country. res=%d\n", res);
    if (!res) return 0;*/

    /*var = 0xC8;
    res = Wifi_SetVar("pm2_sleep_ret", (u8*)&var, 4);
    printf("1. res=%d\n", res);
    if (!res) return 0;

    var = 1;
    res = Wifi_SetVar("bcn_li_bcn", (u8*)&var, 4);
    printf("1. res=%d\n", res);
    if (!res) return 0;

    var = 1;
    res = Wifi_SetVar("bcn_li_dtim", (u8*)&var, 4);
    printf("1. res=%d\n", res);
    if (!res) return 0;

    var = 10;
    res = Wifi_SetVar("assoc_listen", (u8*)&var, 4);
    printf("1. res=%d\n", res);
    if (!res) return 0;*/

    // WLC_SET_INFRA
    var = 1;
    res = Wifi_SendIoctl(20, 2, (u8*)&var, 4, (u8*)&var, 4);
    printf("set infra: %d\n", res);
    if (!res) return 0;

    // WLC_SET_AUTH
    var = 0;
    res = Wifi_SendIoctl(22, 2, (u8*)&var, 4, (u8*)&var, 4);
    printf("set auth: %d\n", res);
    if (!res) return 0;

    /*u32 var2[2];

    var2[0] = 0; var2[1] = 1;
    res = Wifi_SetVar("bsscfg:sup_wpa", (u8*)var2, 8);
    printf("res: %d\n", res);
    if (!res) return 0;

    var2[0] = 0; var2[1] = 0xFFFFFFFF;
    res = Wifi_SetVar("bsscfg:sup_wpa2_eapver", (u8*)var2, 8);
    printf("res: %d\n", res);
    if (!res) return 0;

    var2[0] = 0; var2[1] = 0x9C4;
    res = Wifi_SetVar("bsscfg:sup_wpa_tmo", (u8*)var2, 8);
    printf("res: %d\n", res);
    if (!res) return 0;*/

    // WLC_SET_WSEC
    var = 2; // TKIP
    res = Wifi_SendIoctl(134, 2, (u8*)&var, 4, (u8*)&var, 4);
    printf("set wsec: %d\n", res);
    if (!res) return 0;

    // WLC_SET_WPA_AUTH
    var = 4; // WPA-PSK
    res = Wifi_SendIoctl(165, 2, (u8*)&var, 4, (u8*)&var, 4);
    printf("set wpa auth: %d\n", res);
    if (!res) return 0;

    // WLC_SET_WSEC_PMK
    // supported by: wlfirmware.bin wlfirmware2.bin
    // NOT SUPPORTED by: wlfirmware1.bin wlfirmware3.bin wlfirmware4.bin
    int passlen = strlen(pass);
    if (passlen > 96) passlen = 96;
    memset(tmp, 0, sizeof(tmp));
    *(u16*)&tmp[0] = passlen;
    *(u16*)&tmp[2] = 1; // flags
    strncpy((char*)&tmp[4], pass, 96);
    res = Wifi_SendIoctl(268, 2, tmp, 100, tmp, 100);
    printf("set key: %d\n", res);
    if (!res) return 0;

    for (int i = 0; i < 20; i++)
    {
        Wifi_ResetRxFlags(Flag_RxEvent);

        // WLC_SET_SSID
        int ssidlen = strlen(ssid);
        if (ssidlen > 96) ssidlen = 96;
        memset(tmp, 0, sizeof(tmp));
        *(u32 * ) & tmp[0] = ssidlen;
        strncpy((char *) &tmp[4], ssid, 96);
        res = Wifi_SendIoctl(26, 2, tmp, 4+ssidlen, tmp, 4+ssidlen);
        printf("set SSID: %d\n", res);
        if (!res) return 0;

        // TODO wait for shit
        for (;;)
        {
            Wifi_WaitForRx(Flag_RxEvent);

            //printf("event: %08X %08X %08X\n", shaz[0], shaz[1], shaz[2]);
            printf("got event\n");
            sPacket* pkt;
            while ((pkt = Wifi_ReadRxPacket()))
            {
                send_binary(pkt->Data, pkt->Length);
                free(pkt);
            }

            //u16 len = *(u16*)&RxEventBuffer[0];
            //send_binary(RxEventBuffer, len);
            Wifi_ResetRxFlags(Flag_RxEvent);
        }

        WUP_DelayMS(500);

        // B2:0D:43:EB:E6:78
        u8 bssid[6];
        res = Wifi_SendIoctl(23, 0, bssid, 6, bssid, 6);
        printf("BSSID res=%d\n", res);
        printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
               bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

        if (res) break;
    }

    return 1;
}


