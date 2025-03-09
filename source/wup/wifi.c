#include <wup/wup.h>
#include <malloc.h>

#include "wifi_ioctl.h"


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

static u8 Scanning;
static fnScanCb ScanCB;
static sScanInfo* ScanList;
static int ScanNum;

void Wifi_ResetRxFlags(u8 flags);
void Wifi_WaitForRx(u8 flags);

int Wifi_SendIoctl(u32 opc, u16 flags, u8* data_in, u32 len_in, u8* data_out, u32 len_out);
void Wifi_InitIoctls();
void send_binary(u8* data, int len);
void dump_data(u8* data, int len);
void Wifi_DumpRAM();


int Wifi_Init()
{
    printf("wifi init\n");

    PktQueueHead = NULL;
    PktQueueTail = NULL;

    ScanList = NULL;
    ScanNum = 0;

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
    // if return here: allows reset but only once?
    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);

    TxSeqno = 0xFF;
    TxCtlId = 1;
    RxFlags = 0;
    SDIO_EnableCardIRQ();
    // if return here: allows reset but wifi fail
    Wifi_InitIoctls();

    /*for (int i = 0; i < 6; i++)
    {
        u32 addr = 0x18100500 + (i<<12);
        u32 val;
        SDIO_ReadF1Memory(addr, (u8*)&val, 4);
        printf("core %d: status=%08X\n", i, val);
    }*/

    //Wifi_DumpRAM();

    SDIO_SetClocks(0, 0);
    printf("Wifi: ready to go\n");
    return 1;
}

void Wifi_DeInit()
{
    SDIO_SetClocks(1, 0);

    /*u32 var = 1;
    int res = Wifi_SendIoctl(3, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("down: res=%d resp=%08X\n", res, var);

    SDIO_DisableCardIRQ();

    Wifi_AI_SetCore(WIFI_CORE_ARMCM3);
    Wifi_AI_DisableCore(0);*/

    //SDIO_SetClocks(0, 0);
    u8 regval;
    /*printf("11\n");
    u8 regval = 0;
    SDIO_WriteCardRegs(1, 0x1000E, 1, &regval);
printf("22\n");*/
    // dhdsdio_sdclk TODO

    regval = 0;
    SDIO_ReadCardRegs(1, 0x10009, 1, (u8*)&regval);
    printf("33\n");
    if (!(regval & (1<<3)))
    {
        regval |= (1<<3);
        SDIO_WriteCardRegs(1, 0x10009, 1, (u8*)&regval);
    }
    printf("44\n");
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

void Wifi_DumpRAM()
{
    printf("dumping wifi RAM\n");

    int tmplen = 0x2000;
    u8* verbuf = (u8*)memalign(16, tmplen);
    u32 fw_length = RAMSize;
    for (int i = 0; i < fw_length; )
    {
        int chunk = tmplen;
        if ((i + chunk) > fw_length)
            chunk = fw_length - i;

        if (!SDIO_ReadF1Memory(i, verbuf, chunk))
        {
            printf("read failed\n");
            return;
        }

        dump_data(verbuf, chunk);

        i += chunk;
    }

    printf("OK\n");
    free(verbuf);
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
    //printf("sending %d bytes\n", len_rounded);
    //send_binary(buf, len_rounded);
    SDIO_WriteCardData(2, 0x8000, buf, len_rounded, 0);

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
        //printf("len=%04X cpl=%04X off=%02X\n", *(u16*)&RxCtlBuffer[0], *(u16*)&RxCtlBuffer[2], replyoffset);
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

int Wifi_SendPacket(u8* data_in, u32 len_in)
{
    int ret = 1;

    //u8 buf[256];
    u8* buf = &fazil[0];

    int totallen = len_in;
    if (totallen > 1518)
        totallen = 1518;
    //totallen += 14;
    totallen += 18;

    //u16 totallen = len_in + 28;
    *(u16*)&buf[0] = totallen;
    *(u16*)&buf[2] = ~totallen;
    buf[4] = TxSeqno++;
    buf[5] = 0x02; // channel (data)
    buf[6] = 0;
    buf[7] = 14; // offset to data
    *(u32*)&buf[8] = 0; // ??
    //*(u16*)&buf[12] = 0;
    //memcpy(&buf[14], data_in, len_in);
    *(u16*)&buf[14] = 0x20; // ??? must be 0x10 or 0x20
    *(u16*)&buf[16] = 0;
    memcpy(&buf[18], data_in, len_in);

    u16 len_rounded = (totallen + 0x3F) & ~0x3F;
    //printf("sending %d bytes\n", len_rounded);
    send_binary(buf, len_rounded);
    SDIO_WriteCardData(2, 0x8000, buf, len_rounded, 0);
    //printf("packet sent\n");

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

    //printf("Wifi_SetVar(%s)\n", var);
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

    /*var = 23456;
    res = Wifi_SendIoctl(109, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("-- res=%d, gmode=%d\n", res, var);

    var = 23456;
    res = Wifi_GetVar("nmode", (u8*)&var, 4);
    printf("-- res=%d, nmode=%d\n", res, var);

    var = 23456;
    res = Wifi_GetVar("nmode_protection", (u8*)&var, 4);
    printf("-- res=%d, nmodeproot=%d\n", res, var);*/

    /*var = 1;
    res = Wifi_SetVar("nmode", (u8*)&var, 4);
    printf("-- set nmode, res=%d\n", res);*/

    /*var = 23456;
    res = Wifi_GetVar("nmode", (u8*)&var, 4);
    printf("-- res=%d, nmode=%d\n", res, var);*/

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
    //var = 2; // 0=auto 1=5GHz 2=2.4GHz
    res = Wifi_SendIoctl(142, 2, (u8*)&var, 4, (u8*)&resp, 4);
    printf("res=%d\n", res);

    // sgi_tx & sgi_rx
    var = 0;
    res = Wifi_SetVar("sgi_tx", (u8*)&var, 4);
    printf("res=%d\n", res);
    var = 0;
    res = Wifi_SetVar("sgi_rx", (u8*)&var, 4);
    printf("res=%d\n", res);

    // not supported
    var = 4;
    res = Wifi_SetVar("ampdu_txfail_event", (u8*)&var, 4);
    printf("res=%d\n", res);

    // not supported
    var = 1;
    res = Wifi_SetVar("ac_remap_mode", (u8*)&var, 4);
    printf("res=%d\n", res);

    u8 event_msgs[16];
    res = Wifi_GetVar("event_msgs", event_msgs, 16);
    printf("res=%d evt=%08X/%08X/%08X/%08X\n",
           res, *(u32*)&event_msgs[0], *(u32*)&event_msgs[4],
           *(u32*)&event_msgs[8], *(u32*)&event_msgs[12]);
    // 00000000/00400000/00000000/3100CD7C
    // 0..12, 26, 69
    *(u32*)&event_msgs[0] |= 0x04001FFF;
    *(u32*)&event_msgs[8] |= 0x20;

    //memset(event_msgs, 0, 16);
    res = Wifi_SetVar("event_msgs", event_msgs, 16);
    printf("res=%d\n", res);

    // get radio disable
    var = 1;
    res = Wifi_SendIoctl(37, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("res=%d radiodisable=%02X (%08X)\n", res, (u8)var, var);

    // get SRL
    var = 0;
    res = Wifi_SendIoctl(31, 0, (u8*)&var, 1, (u8*)&var, 1);
    printf("res=%d SRL=%02X\n", res, (u8)var);

    var = 0;
    res = Wifi_GetVar("ampdu", (u8*)&var, 4);
    printf("res=%d ampdu=%08X\n", res, var);

    var = 0;
    res = Wifi_GetVar("country", (u8*)&var, 4);
    //res = res = Wifi_SendIoctl(83, 0, (u8*)&var, 4, (u8*)&var, 4);
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

    // set country
    char country_data[12] = {0x45, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x55, 0x00, 0x00};
    //char country_data[12] = {0x46, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x52, 0x00, 0x00};
    res = Wifi_SetVar("country", (u8*)country_data, sizeof(country_data));
    //res = Wifi_SendIoctl(84, 2, (u8*)country_data, 12, (u8*)country_data, 12);
    printf("country. res=%d\n", res);

    /* EU: immediate
     * got event
00000000: 5C 00 A3 FF 2E 01 00 0E 00 2B 00 00 00 00 20 00
00000010: 00 00 40 F4 07 EA 66 19 42 F4 07 EA 66 19 88 6C
00000020: 80 01 00 68 00 00 10 18 00 01 00 02 00 18 00 00
00000030: 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00
00000040: 00 00 FF 00 00 00 00 00 77 6C 30 00 00 00 00 00
00000050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000060: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000070: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

     FR: late
     got event
00000000: 5C 00 A3 FF 2C 01 00 0E 00 2B 00 00 00 00 20 00
00000010: 00 00 40 F4 07 EA 66 19 42 F4 07 EA 66 19 88 6C
00000020: 80 01 00 68 00 00 10 18 00 01 00 02 00 18 00 00
00000030: 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00
00000040: 00 00 FF 00 00 00 00 00 77 6C 30 00 00 00 00 00
00000050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000060: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00000070: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

*/

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

    /*var = 0xF;
    res = Wifi_SetVar("msglevel", (u8*)&var, 4);
    printf("set level res=%d\n", res);

    char* dump = (char*)malloc(8192);
    res = Wifi_GetVar("dump", (u8*)dump, 8192);
    printf("res=%d\n", res);
    puts(dump);*/
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
        //_write(1, "event\n", 6);

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
    else if (chan == 2)
    {
        _write(1, "DATA!\n", 6);
    }
    else
    {
        //
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




int Wifi_StartScan(fnScanCb callback)
{
    int res;

    if (Scanning) return 0;

    Wifi_CleanupScanList();

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);

    u8 scanparams[72];
    memset(scanparams, 0, sizeof(scanparams));
    *(u32*)&scanparams[0] = 1; // version
    *(u16*)&scanparams[4] = 1; // action: start
    *(u16*)&scanparams[6] = 1; // duration
    memset(&scanparams[8+36], 0xFF, 6); // BSSID
    scanparams[8+42] = 2; // BSS type
    scanparams[8+43] = 1; // scan type (passive)
    *(s32*)&scanparams[8+44] = -1; // nprobes
    *(s32*)&scanparams[8+48] = -1; // active_time
    *(s32*)&scanparams[8+52] = -1; // passive_time
    *(s32*)&scanparams[8+56] = -1; // home_time
    *(u32*)&scanparams[8+60] = 0; // channel_num

    Wifi_ResetRxFlags(Flag_RxEvent);

    res = Wifi_SetVar("escan", scanparams, sizeof(scanparams));
    if (!res) return 0;

    Scanning = 1;
    ScanCB = callback;
    return 1;
}

static void ParseWPAInfo(int type, u8* info, u32 len, u8* auth, u8* sec)
{
    u8 oui[3];
    if      (type == 1) { oui[0] = 0x00; oui[1] = 0x50; oui[2] = 0xF2; }
    else if (type == 2) { oui[0] = 0x00; oui[1] = 0x0F; oui[2] = 0xAC; }
    else return;

    u8* infoend = &info[len];

    // multicast cipher
    if (info[0] == oui[0] && info[1] == oui[1] && info[2] == oui[2])
    {
        if (info[3] == 0x02)
            *sec |= WIFI_SEC_TKIP;
        else if (info[3] == 0x04)
            *sec |= WIFI_SEC_AES;
    }
    info += 4;

    // unicast ciphers
    u16 num_uni = info[0] | (info[1] << 8);
    if ((info + 2 + (num_uni * 4)) > infoend) return;
    info += 2;
    for (int j = 0; j < num_uni; j++)
    {
        if (info[0] == oui[0] && info[1] == oui[1] && info[2] == oui[2])
        {
            if (info[3] == 0x02)
                *sec |= WIFI_SEC_TKIP;
            else if (info[3] == 0x04)
                *sec |= WIFI_SEC_AES;
        }
        info += 4;
    }

    // auth types
    u16 num_auth = info[0] | (info[1] << 8);
    if ((info + 2 + (num_uni * 4)) > infoend) return;
    info += 2;
    for (int j = 0; j < num_auth; j++)
    {
        if (info[0] == oui[0] && info[1] == oui[1] && info[2] == oui[2])
        {
            if (info[3] == 0x02)
            {
                if (type == 1)
                    *auth = WIFI_AUTH_WPA_PSK;
                else if (type == 2)
                    *auth = WIFI_AUTH_WPA2_PSK;
            }
        }
        info += 4;
    }
}

static int CompareScanInfo(sScanInfo* a, sScanInfo* b)
{
    if (a->RSSI > b->RSSI) return -1;
    else if (a->RSSI < b->RSSI) return 1;
    else
        return strncmp(a->SSID, b->SSID, 32);
}

void Wifi_AddScanResult(u8* apdata, u32 len)
{
    u8 bssid[6];
    memcpy(bssid, &apdata[0x08], 6);

    char ssid[33];
    u8 ssidlen = apdata[0x12];
    if (ssidlen > 32) return;

    if (ssidlen == 0)
    {
        // hidden network
        return;
    }

    u16 capabilities = READ16LE(apdata, 0x10);

    if (ssidlen > 32) ssidlen = 32;
    if (ssidlen) memcpy(ssid, &apdata[0x13], ssidlen);
    ssid[ssidlen] = 0;

    u16 chanspec = READ16LE(apdata, 0x48);
    s16 rssi = (s16)READ16LE(apdata, 0x4E); // CHECKME

    u32 ie_offset = READ32LE(apdata, 0x74);
    u32 ie_len = READ32LE(apdata, 0x78);

    int has_rsn = 0;
    int has_wpa = 0;
    u8 rsn_auth = WIFI_AUTH_WPA2_UNSPEC, rsn_sec = 0;
    u8 wpa_auth = WIFI_AUTH_WPA_UNSPEC, wpa_sec = 0;

    // parse 802.11 information elements
    u8* ie_data = &apdata[ie_offset];
    u8* ie_end = &apdata[ie_offset + ie_len];
    if (ie_end > &apdata[len]) ie_end = &apdata[len];
    while (ie_data < ie_end)
    {
        u8 ietype = *ie_data++;
        u8 ielen = *ie_data++;
        if (&ie_data[ielen] > ie_end) break;

        switch (ietype)
        {
        case 0x30:
            if (ielen < 2) break;
            if (ie_data[0] == 0x01 && ie_data[1] == 0x00 &&
                ielen >= 18)
            {
                has_rsn = 1;
                ParseWPAInfo(2, &ie_data[2], ielen-2, &rsn_auth, &rsn_sec);
            }
            break;

        case 0xDD:
            if (ielen < 6) break;
            if (ie_data[0] == 0x00 && ie_data[1] == 0x50 &&
                ie_data[2] == 0xF2 && ie_data[3] == 0x01 &&
                ie_data[4] == 0x01 && ie_data[5] == 0x00 &&
                ielen >= 22)
            {
                has_wpa = 1;
                ParseWPAInfo(1, &ie_data[6], ielen-6, &wpa_auth, &wpa_sec);
            }
            break;
        }

        ie_data += ielen;
    }

    sScanInfo* info = NULL;

    // see if we already have this network in the list
    sScanInfo* chk = ScanList;
    int isworst = 0;
    while (chk)
    {
        int ssidmatch = !strncmp(chk->SSID, ssid, ssidlen);
        int bssidmatch = !memcmp(chk->BSSID, bssid, 6);
        int rssibetter = (chk->RSSI < rssi);
        int good = 0;

        if (ssidmatch)
        {
            if (bssidmatch) good = 1;
            else
            {
                if (rssibetter) good = 1;
                else isworst = 1;
            }
        }

        if (good)
        {
            info = chk;
            break;
        }

        chk = chk->Next;
    }

    if (!info)
    {
        if (isworst)
        {
            // this AP is worse than what we have in the list - ignore it
            return;
        }

        int infolen = sizeof(sScanInfo);
        info = (sScanInfo*)malloc(infolen);
        if (!info) return;
        memset(info, 0, infolen);
        ScanNum++;
    }
    else
    {
        if (!info->Prev) ScanList = NULL;
        if (info->Prev) info->Prev->Next = info->Next;
        if (info->Next) info->Next->Prev = info->Prev;
    }

    // fill in AP info

    info->SSIDLength = ssidlen;
    memcpy(info->SSID, ssid, 32);
    memcpy(info->BSSID, bssid, 6);

    info->Capabilities = capabilities;
    info->Channel = chanspec & 0xFF;

    if (capabilities & (1<<4))
    {
        if (has_rsn)
        {
            // WPA2
            info->AuthType = rsn_auth;
            info->Security = rsn_sec;
        }
        else if (has_wpa)
        {
            // WPA
            info->AuthType = wpa_auth;
            info->Security = wpa_sec;
        }
        else
        {
            // WEP, not going to bother with it for now
            info->AuthType = 0;
            info->Security = 0;
        }
    }
    else
    {
        info->AuthType = WIFI_AUTH_OPEN;
        info->Security = 0;
    }

    info->RSSI = rssi;
    if (rssi <= -91)
        info->SignalQuality = 0;
    else if (rssi <= -80)
        info->SignalQuality = 1;
    else if (rssi <= -70)
        info->SignalQuality = 2;
    else if (rssi <= -68)
        info->SignalQuality = 3;
    else if (rssi <= -58)
        info->SignalQuality = 4;
    else
        info->SignalQuality = 5;

    info->Timestamp = WUP_GetTicks();

    // add it to the list, in order
    // we want to have the APs with the highest RSSI first
    // then sort alphanumerically
    if (!ScanList)
    {
        ScanList = info;
        info->Prev = NULL;
        info->Next = NULL;
    }
    else
    {
        // try to insert our new entry before a lower ranked entry

        sScanInfo* cur = ScanList;
        sScanInfo* last = cur;
        int inserted = 0;
        while (cur)
        {
            int cmp = CompareScanInfo(info, cur);
            if (cmp <= 0)
            {
                info->Prev = cur->Prev;
                if (cur->Prev) cur->Prev->Next = info;
                else           ScanList = info;
                cur->Prev = info;
                info->Next = cur;

                inserted = 1;
                break;
            }

            last = cur;
            cur = cur->Next;
        }

        if (!inserted)
        {
            // if there was no lower ranked entry, insert it at the end of the list
            last->Next = info;
            info->Prev = last;
            info->Next = NULL;
        }
    }
}

void Wifi_CleanupScanList()
{
    /*u32 time = WUP_GetTicks();
    const u32 age_max = 5000;

    sScanInfo* cur = ScanList;
    while (cur)
    {
        sScanInfo* next = cur->Next;

        if ((time - cur->Timestamp) >= age_max)
        {
            if (!cur->Prev) ScanList = cur->Next;
            if (cur->Prev) cur->Prev->Next = cur->Next;
            if (cur->Next) cur->Next->Prev = cur->Prev;

            free(cur);
            ScanNum--;
        }

        cur = next;
    }*/

    sScanInfo* cur = ScanList;
    while (cur)
    {
        sScanInfo* next = cur->Next;
        free(cur);
        cur = next;
    }

    ScanList = NULL;
    ScanNum = 0;
}





// TEST
int Wifi_JoinNetwork(u32 wsec, u32 wpaauth)
{
    u32 var;
    int res;
    u8 tmp[100];
    //char* ssid = "ElectricAngel";
    //char* ssid = "Freebox-375297"; // TEST
    char* ssid = "tarteauxpommes"; // TEST
    //char* ssid = "tarteauxfraises"; // TEST
    //char* ssid = "WiiU182a7bd2f30f"; // TEST
    //char* pass = "hdfgfdgjdtyygdf";
    char* pass = "putain j'ai mal au cul";
    //char* pass = "66826b1d30f5c41dcc7552e9a1b6b84047828d4958a794f2e22a36b786688d0e";
    /*char pass[32] = {0x66, 0x82, 0x6b, 0x1d, 0x30, 0xf5, 0xc4, 0x1d, 0xcc, 0x75, 0x52,
                     0xe9, 0xa1, 0xb6, 0xb8, 0x40, 0x47, 0x82, 0x8d, 0x49, 0x58, 0xa7,
                     0x94, 0xf2, 0xe2, 0x2a, 0x36, 0xb7, 0x86, 0x68, 0x8d, 0x0e};*/
    // WPA key 66826b1d30f5c41dcc7552e9a1b6b84047828d4958a794f2e22a36b786688d0e
    // WPA2

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);

    var = 1;
    res = Wifi_SendIoctl(3, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("res=%d resp=%08X\n", res, var);

    /*char zorp[12];
    Wifi_GetVar("country", zorp, 20);
    send_binary(zorp, 12);*/

    var = 0;
    res = Wifi_SendIoctl(37, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("radio: res=%d resp=%08X\n", res, var);

    // WLC_SET_CHANNEL
    /*var = 48;
    res = Wifi_SendIoctl(30, 2, (u8*)&var, 4, (u8*)&var, 4);
    printf("set channel: %d\n", res);
    if (!res) return 0;*/

    /*var = 3;
    res = Wifi_SetVar("bcn_timeout", (u8*)&var, 4);
    printf("set bcn: res=%d resp=%08X\n", res, var);

    var = 0;
    res = Wifi_GetVar("bcn_timeout", (u8*)&var, 4);
    printf("bcn: res=%d resp=%08X\n", res, var);*/

    // WLC_SET_INFRA
    var = 1;
    res = Wifi_SendIoctl(20, 2, (u8*)&var, 4, (u8*)&var, 4);
    printf("set infra: %d\n", res);
    if (!res) return 0;

    var = 1;
    res = Wifi_SendIoctl(2, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("res=%d resp=%08X\n", res, var);

    var = 0;
    res = Wifi_GetVar("bcn_timeout", (u8*)&var, 4);
    printf("bcn: res=%d resp=%08X\n", res, var);
    var = 0;
    res = Wifi_SendIoctl(37, 0, (u8*)&var, 4, (u8*)&var, 4);
    printf("radio: res=%d resp=%08X\n", res, var);

    //return 1;

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

    var = 0xC8;
    res = Wifi_GetVar("pm2_sleep_ret", (u8*)&var, 4);
    printf("2. res=%d var=%08X\n", res, var);

    var = 1;
    res = Wifi_GetVar("bcn_li_bcn", (u8*)&var, 4);
    printf("2. res=%d var=%08X\n", res, var);

    var = 1;
    res = Wifi_GetVar("bcn_li_dtim", (u8*)&var, 4);
    printf("2. res=%d var=%08X\n", res, var);

    var = 10;
    res = Wifi_GetVar("assoc_listen", (u8*)&var, 4);
    printf("2. res=%d var=%08X\n", res, var);

    // WLC_SET_AUTH
    // 1 == only accept encrypted frames
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

    if (1)
    {
        // WPA-PSK TKIP:  2    4
        // WPA-PSK CCMP:  4/6  4
        //

        // WLC_SET_WSEC
        //var = 2; // TKIP
        //var = 4; // AES
        //var = 6;
        var = wsec;
        res = Wifi_SendIoctl(134, 2, (u8 * ) & var, 4, (u8 * ) & var, 4);
        printf("set wsec: %d\n", res);
        if (!res) return 0;

        // WLC_SET_WPA_AUTH
        //var = 4; // WPA-PSK
        //var = 0x80; //
        var = wpaauth;
        res = Wifi_SendIoctl(165, 2, (u8 * ) & var, 4, (u8 * ) & var, 4);
        printf("set wpa auth: %d\n", res);
        if (!res) return 0;

        // sup_wpa
        var = 1;
        res = Wifi_SetVar("sup_wpa", (u8 * ) & var, 4);
        printf("res: %d\n", res);
        if (!res) return 0;

        WUP_DelayMS(2);

        // WLC_SET_WSEC_PMK
        // supported by: wlfirmware.bin wlfirmware2.bin
        // NOT SUPPORTED by: wlfirmware1.bin wlfirmware3.bin wlfirmware4.bin
        int passlen = strlen(pass);
        if (passlen > 96) passlen = 96;
        memset(tmp, 0, sizeof(tmp));
        *(u16 * ) & tmp[0] = passlen;
        *(u16 * ) & tmp[2] = 1; // flags
        strncpy((char *) &tmp[4], pass, 96);
        res = Wifi_SendIoctl(268, 2, tmp, 100, tmp, 100);
        printf("set key: %d\n", res);
        if (!res) return 0;
    }
    else
    {
        // open wifi

        // WLC_SET_WSEC
        var = 0;
        res = Wifi_SendIoctl(134, 2, (u8 * ) & var, 4, (u8 * ) & var, 4);
        printf("set wsec: %d\n", res);
        if (!res) return 0;

        /*var = 0;
        res = Wifi_SendIoctl(165, 2, (u8 * ) & var, 4, (u8 * ) & var, 4);
        printf("set wpa auth: %d\n", res);
        if (!res) return 0;

        var = 0;
        res = Wifi_SetVar("sup_wpa", (u8 * ) & var, 4);
        printf("res: %d\n", res);
        if (!res) return 0;*/
    }

    Wifi_ResetRxFlags(Flag_RxEvent);

    //u8 event_msgs[16] = {0xEB, 0x1F, 0xBB, 0x04, 0x50, 0x40, 0x40, 0x80, 0x00, 0x80, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    //u8 event_msgs[16] = {0xFF, 0x1F, 0xBB, 0x04, 0x50, 0x40, 0x40, 0x80, 0x00, 0x80, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    u8 event_msgs[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    res = Wifi_SetVar("event_msgs", event_msgs, 16);
    printf("res=%d\n", res);

    // WLC_SET_SSID
    // 4+32   36
    // 6+2    8
    // 4+2    6
    //        50
    int ssidlen = strlen(ssid);
    if (ssidlen > 32) ssidlen = 32;
    memset(tmp, 0, sizeof(tmp));
    *(u32 * ) & tmp[0] = ssidlen;
    strncpy((char *) &tmp[4], ssid, 32);
    memset(&tmp[4+32], 0xFF, 6);
    res = Wifi_SendIoctl(26, 2, tmp, 50, tmp, 50);
    printf("set SSID: %d\n", res);
    if (!res) return 0;

    return 1;
}



void Wifi_Update()
{
    if (RxFlags & Flag_RxEvent)
    {
        printf("got event(s)\n");
        sPacket* pkt;
        while ((pkt = Wifi_ReadRxPacket()))
        {
            //send_binary(pkt->Data, pkt->Length);

            u8 dataoffset = pkt->Data[7];
            if (dataoffset >= pkt->Length)
            {
                printf("* bad data offset %02X\n", dataoffset);
                free(pkt);
                continue;
            }

            u8* pktdata = &pkt->Data[dataoffset];
            u8* pktend = &pkt->Data[pkt->Length];

            u16 flags = READ16BE(pktdata, 0x1E);
            u32 type = READ32BE(pktdata, 0x20);
            u32 status = READ32BE(pktdata, 0x24);
            u32 reason = READ32BE(pktdata, 0x28);

            switch (type)
            {
            case WLC_E_ESCAN_RESULT:
                {
                    if (!Scanning) break;
                    if (pkt->Length < 0x58) break;

                    u32 aplen = READ32LE(pktdata, 0x4C);
                    u16 numap = READ16LE(pktdata, 0x56);
                    if ((aplen > 0xC) && (numap > 0))
                    {
                        // we got AP data
                        u8* apdata = &pktdata[0x58];
                        u8* apend = &apdata[aplen];
                        for (u32 j = 0; j < numap && apdata < apend; j++)
                        {
                            if ((apdata + aplen) > pktend) break;

                            u32 aplen = READ32LE(apdata, 0x04);
                            Wifi_AddScanResult(apdata, aplen);
                            apdata += aplen;
                        }
                    }
                    else
                    {
                        // empty frame: scan end

                        Scanning = 0;
                        if (ScanCB) ScanCB(ScanList, ScanNum);
                        ScanCB = NULL;
                    }
                }
                break;
            }

            if (type != 0x2C)
                printf("* type=%d flags=%04X status=%08X reason=%08X\n", type, flags, status, reason);

            free(pkt);
        }

        Wifi_ResetRxFlags(Flag_RxEvent);
    }
}

