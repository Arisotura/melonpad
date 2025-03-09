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

static u8 MACAddr[6];

#define Flag_RxMail     (1<<0)
#define Flag_RxCtl      (1<<1)
#define Flag_RxEvent    (1<<2)
#define Flag_RxData     (1<<3)

static u8 TxSeqno;
static u16 TxCtlId;
static u8 TxBuffer[4096];

static volatile u8 RxFlags;
static u32 RxMail;
static u8 RxCtlBuffer[4096];
static u8 RxEventBuffer[512];
static u8 RxDataBuffer[4096];

typedef struct sPacket
{
    struct sPacket* Next;
    u32 Length;
    u8 Data[1];

} sPacket;

sPacket* PktQueueHead;
sPacket* PktQueueTail;

static u8 Scanning;
static fnScanCb ScanCB;
static sScanInfo* ScanList;
static int ScanNum;

int Wifi_InitIoctls();

void Wifi_ResetRxFlags(u8 flags);
void Wifi_WaitForRx(u8 flags);

int Wifi_SendIoctl(u32 opc, u16 flags, u8* data_in, u32 len_in, u8* data_out, u32 len_out);
int Wifi_GetVar(char* var, u8* data, u32 len);
int Wifi_SetVar(char* var, u8* data, u32 len);

void send_binary(u8* data, int len);
void dump_data(u8* data, int len);


int Wifi_Init()
{
    printf("wifi init\n");

    memset(MACAddr, 0, sizeof(MACAddr));

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

    Wifi_AI_SetCore(WIFI_CORE_ARMCM3);
    Wifi_AI_DisableCore(0);

    if (Wifi_AI_GetCoreRevision() >= 20)
    {
        Wifi_AI_SetCore(WIFI_CORE_BACKPLANE);

        // GPIO pullup/pulldown
        Wifi_AI_WriteCoreMem(0x058, 0);
        Wifi_AI_WriteCoreMem(0x05C, 0);

        // only if revision >= 21
        // ??
        regval = Wifi_AI_ReadCoreMem(0x008);
        Wifi_AI_WriteCoreMem(0x008, regval | 0x4);

    }

    RAMSize = Wifi_AI_GetRAMSize();
    if (!RAMSize)
        return 0;
    printf("WIFI RAM SIZE = %08X\n", RAMSize);

    // reset backplane upon SDIO reset
    Wifi_AI_SetCore(WIFI_CORE_SDIOD);
    regval = Wifi_AI_ReadCoreMem(0x000);
    Wifi_AI_WriteCoreMem(0x000, regval | (1<<1));

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

        // enable frame transfers
        Wifi_AI_WriteCoreMem(0x048, (4<<16));

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
        Wifi_AI_WriteCoreMem(0x024, 0x200000F0);

        // set watermark
        regval = 8;
        SDIO_WriteCardRegs(1, 0x10008, 1, (u8*)&regval);
    }

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);

    TxSeqno = 0xFF;
    TxCtlId = 1;
    RxFlags = 0;
    SDIO_EnableCardIRQ();

    if (!Wifi_InitIoctls())
        return 0;

    SDIO_SetClocks(0, 0);
    printf("Wifi: ready to go\n");
    return 1;
}

void Wifi_DeInit()
{
    // TODO: disconnect if needed

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

        Wifi_AI_WriteCoreMem(0x020, 0xFFFFFFFF);

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

int Wifi_InitIoctls()
{
    u32 var, resp;
    int res;

    // get MAC address
    res = Wifi_GetVar("cur_etheraddr", MACAddr, 6);
    if (!res) return 0;

    // disable roaming
    var = 1;
    res = Wifi_SetVar("roam_off", (u8*)&var, 4);
    if (!res) return 0;

    // set band
    var = 1; // 0=auto 1=5GHz 2=2.4GHz
    res = Wifi_SendIoctl(WLC_SET_BAND, 2, (u8*)&var, 4, NULL, 0);
    if (!res) return 0;

    // sgi_tx & sgi_rx
    var = 0;
    res = Wifi_SetVar("sgi_tx", (u8*)&var, 4);
    if (!res) return 0;

    var = 0;
    res = Wifi_SetVar("sgi_rx", (u8*)&var, 4);
    if (!res) return 0;

    var = 4;
    res = Wifi_SetVar("ampdu_txfail_event", (u8*)&var, 4);
    if (!res) return 0;

    var = 1;
    res = Wifi_SetVar("ac_remap_mode", (u8*)&var, 4);
    if (!res) return 0;

    u8 event_msgs[16] = {0};
#define setevent(i) event_msgs[(i)>>3] |= (1 << ((i)&0x7))
    setevent(WLC_E_SET_SSID);
    setevent(WLC_E_JOIN);
    setevent(WLC_E_DEAUTH);
    setevent(WLC_E_DEAUTH_IND);
    setevent(WLC_E_REASSOC);
    setevent(WLC_E_REASSOC_IND);
    setevent(WLC_E_DISASSOC);
    setevent(WLC_E_DISASSOC_IND);
    setevent(WLC_E_LINK);
    setevent(WLC_E_TXFAIL);
    setevent(WLC_E_PRUNE);
    setevent(WLC_E_PSK_SUP);
    setevent(WLC_E_ESCAN_RESULT);

    res = Wifi_SetVar("event_msgs", event_msgs, 16);
    if (!res) return 0;

    // set country
    u16 countrycode;
    UIC_ReadEEPROM(0x66, (u8*)&countrycode, 2);

    u8 country_data[12] = {0};
    *(u16*)&country_data[0] = countrycode;
    *(u16*)&country_data[8] = countrycode;
    res = Wifi_SetVar("country", (u8*)country_data, sizeof(country_data));
    if (!res) return 0;

    var = 1;
    res = Wifi_SendIoctl(WLC_SET_INFRA, 2, (u8*)&var, 4, NULL, 0);
    if (!res) return 0;

    // system up
    var = 1;
    res = Wifi_SendIoctl(WLC_UP, 0, (u8*)&var, 4, NULL, 0);
    if (!res) return 0;

    // set lifetime
    u32 lifetimes[] = {5, 25, 15, 16, 0};
    for (int i = 0; lifetimes[i]; i++)
    {
        u32 data[2] = {i, lifetimes[i]};
        res = Wifi_SetVar("lifetime", (u8*)data, 8);
        if (!res) return 0;
    }

    var = 0xC8;
    res = Wifi_GetVar("pm2_sleep_ret", (u8*)&var, 4);
    if (!res) return 0;

    var = 1;
    res = Wifi_GetVar("bcn_li_bcn", (u8*)&var, 4);
    if (!res) return 0;

    var = 1;
    res = Wifi_GetVar("bcn_li_dtim", (u8*)&var, 4);
    if (!res) return 0;

    var = 10;
    res = Wifi_GetVar("assoc_listen", (u8*)&var, 4);
    if (!res) return 0;

    return 1;
}


int Wifi_SendIoctl(u32 opc, u16 flags, u8* data_in, u32 len_in, u8* data_out, u32 len_out)
{
    int ret = 1;

    u8* buf = &TxBuffer[0];
    u16 wanted_id = TxCtlId;

    // actual frame length can be smaller than requested data length
    // for ioctls that respond with a large amount of data
    int totallen = len_in + 16;
    if (totallen > 1518)
        totallen = 1518;
    totallen += 12;

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

    // send control frame
    // length should be rounded to a multiple of the block size
    u16 len_rounded = (totallen + 0x3F) & ~0x3F;
    SDIO_WriteCardData(2, 0x8000, buf, len_rounded, 0);

    // wait for response
    Wifi_WaitForRx(Flag_RxCtl);

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

    int varlen = strlen(var);
    memcpy(buf, var, varlen);
    buf[varlen] = 0;
    memcpy(&buf[varlen+1], data, len);

    if (!Wifi_SendIoctl(263, 2, buf, 128, buf, 128))
        return 0;

    return 1;
}


int Wifi_SendPacket(u8* data_in, u32 len_in)
{
    int ret = 1;

    u8* buf = &TxBuffer[0];

    int totallen = len_in;
    totallen += 18;

    *(u16*)&buf[0] = totallen;
    *(u16*)&buf[2] = ~totallen;
    buf[4] = TxSeqno++;
    buf[5] = 0x02; // channel (data)
    buf[6] = 0;
    buf[7] = 14; // offset to data
    *(u32*)&buf[8] = 0; // ??
    *(u16*)&buf[14] = 0x20; // ??? must be 0x10 or 0x20
    *(u16*)&buf[16] = 0;
    memcpy(&buf[18], data_in, len_in);

    // send frame
    u16 len_rounded = (totallen + 0x3F) & ~0x3F;
    SDIO_WriteCardData(2, 0x8000, buf, len_rounded, 0);

    return ret;
}


void Wifi_ResetRxFlags(u8 flags)
{
    RxFlags &= ~flags;
}

void Wifi_WaitForRx(u8 flags)
{
    while (!(RxFlags & flags))
    {
        WaitForIRQ();
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

void Wifi_RxData()
{
    // read header
    u8 header[64];
    SDIO_ReadCardData(2, 0x8000, header, 64, 0);

    u16 len = *(u16*)&header[0];
    u16 not_len = *(u16*)&header[2];
    if (len < 12) return;
    if (len != (u16)(~not_len)) return;

    u8 seqno = header[4];
    u8 chan = header[5];
    u8 dataoffset = header[7];
    if (dataoffset < 12) return;

    // TODO account for credits

    if (chan == 0)
    {
        // control channel

        u16 len_rounded = (len + 0x3F) & ~0x3F;
        if (len_rounded > 4096) return;

        memcpy(RxCtlBuffer, header, 64);
        if (len_rounded > 64)
            SDIO_ReadCardData(2, 0x8000, &RxCtlBuffer[64], len_rounded-64, 0);

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
            //_write(1, "FAIL1\n", 6);
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
        //_write(1, "DATA!\n", 6);
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


int Wifi_JoinNetwork(const char* ssid, u8 auth, u8 security, const char* pass)
{
    u32 var;
    int res;

    SDIO_SetClocks(1, SDIO_CLOCK_REQ_HT);

    // set it to 0 for open system auth, 1 for shared key
    var = 0;
    res = Wifi_SendIoctl(WLC_SET_AUTH, 2, (u8*)&var, 4, NULL, 0);
    if (!res) return 0;

    var = security;
    res = Wifi_SendIoctl(WLC_SET_WSEC, 2, (u8*)&var, 4, NULL, 0);
    if (!res) return 0;

    var = auth;
    res = Wifi_SendIoctl(WLC_SET_WPA_AUTH, 2, (u8*)&var, 4, NULL, 0);
    if (!res) return 0;

    var = (auth == WIFI_AUTH_OPEN) ? 0 : 1;
    res = Wifi_SetVar("sup_wpa", (u8*)&var, 4);
    if (!res) return 0;

    WUP_DelayMS(2);

    if (auth != WIFI_AUTH_OPEN)
    {
        // secured network, set passphrase

        u8 pmk[100] = {0};
        int passlen = strlen(pass);
        if (passlen > 96) passlen = 96;
        *(u16*)&pmk[0] = passlen;
        *(u16*)&pmk[2] = 1; // flags, 1 for passphrase
        strncpy((char*)&pmk[4], pass, 96);
        res = Wifi_SendIoctl(WLC_SET_WSEC_PMK, 2, pmk, 100, NULL, 0);
        if (!res) return 0;
    }

    Wifi_ResetRxFlags(Flag_RxEvent);

    // set SSID - this initiates the connection
    u8 ssiddata[42] = {0};
    int ssidlen = strlen(ssid);
    if (ssidlen > 32) ssidlen = 32;
    *(u32*)&ssiddata[0] = ssidlen;
    strncpy((char*)&ssiddata[4], ssid, 32);
    memset(&ssiddata[4+32], 0xFF, 6);
    res = Wifi_SendIoctl(WLC_SET_SSID, 2, ssiddata, 42, NULL, 0);
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

