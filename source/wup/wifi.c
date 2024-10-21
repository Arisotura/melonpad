#include <wup/wup.h>
#include <malloc.h>


static u32 RAMSize;


int Wifi_Init()
{
    printf("wifi init\n");

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


// NOTES on sending stuff
//
// * dhd_bus_txctl() sends command/shit to the wifi chip
//
// HEADER for IOCTL
// 2b: len (message length)
// 2b: ~len
// 4b: header (bit0-7 = seqno, bit8-11 = channel, bit24-31 = data offset)
// 4b: 0
// then data (with optional offset?)
//
// IOCTL STRUCTURE
// see cdc_ioctl_t
// 4b cmd, 4b len, 4b flags, 4b status
// see wlioctl.h for ioctl values
// see dhd_preinit_ioctls() for init stuff
