#include <wup/wup.h>
#include <malloc.h>


static u32 RAMSize;


int Wifi_Init()
{
    printf("wifi init\n");

    u32 regval;

    // dhdsdio_probe_attach

    regval = 0x28;
    SDIO_WriteCardRegs(1, 0x1000E, 1, (u8*)&regval);
    regval = 0;
    SDIO_ReadCardRegs(1, 0x1000E, 1, (u8*)&regval);
    if ((regval & ~0xC0) != 0x28)
        return 0;

    /*regval = 0;
    SDIO_ReadCardRegs(1, 0x10009, 1, (u8*)&regval);
    printf("ISO SHITO = %02X\n", regval);*/

    // si_attach

    regval = 0x28;
    if (SDIO_WriteCardRegs(1, 0x1000E, 1, (u8*)&regval))
    {
        u8 readval;
        SDIO_ReadCardRegs(1, 0x1000E, 1, &readval);
        //printf("readval=%02X\n", readval);
        // reads 68
        if ((readval & ~0xC0) == regval)
        {
            while (!(readval & 0xC0))
            {
                SDIO_ReadCardRegs(1, 0x1000E, 1, &readval);
            }

            regval = 0x21;
            SDIO_WriteCardRegs(1, 0x1000E, 1, (u8*)&regval);
            WUP_DelayUS(65);
        }
    }

    regval = 0;
    SDIO_WriteCardRegs(1, 0x1000F, 1, (u8*)&regval);

    printf("--- FART ---\n");

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

    // F1:1000E = 0 then 8 ???
    // then they turn off ARM and reset SOCRAM
    // then they write 0 at 47FFC

    // dhdsdio_probe_attach done

    //Wifi_AI_SetCore(0x800);
    //Wifi_AI_ResetCore(0, 0);

    {
        // stupid shit
        u8 val = 0x02; // enable F1
        if (!SDIO_WriteCardRegs(0, 0x2, 1, &val))
            return 0;

        val = 0;
        if (!SDIO_WriteCardRegs(1, 0x1000E, 1, &val))
            return 0;

        WUP_DelayMS(5);

        val = 0x08;
        if (!SDIO_WriteCardRegs(1, 0x1000E, 1, &val))
            return 0;
    }

    int ret = Wifi_UploadFirmware();
    printf("ret=%d\n", ret);
    WUP_DelayMS(20);
    printf("pres=%08X\n", REG_SD_PRESENTSTATE);

    return 1;
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
        if (!Wifi_AI_SetCore(WIFI_CORE_SOCRAM))
            return 0;

        // TODO write vars and shit
    }

    return 1;
}
void send_binary(u8* data, int len);
int Wifi_UploadFirmware()
{
    u32 offset, length;
    if (!Flash_GetEntryInfo("WIFI", &offset, &length, NULL))
        return 0;

    int tmplen = 0x2000;
    u8* tmpbuf = (u8*)memalign(16, tmplen);
    printf("firmware offset=%08X len=%d\n", offset, length);

    //if (!SDIO_SetClocks(1, 1)) return 0;
    if (!Wifi_SetUploadState(1)) return 0;
    printf("we're going\n");
    //length = 256;//0x200;

    for (int i = 0; i < length; )
    {
        int chunk = tmplen;
        if ((i + chunk) > length)
            chunk = length - i;

        printf("writing %08X, len=%d\n", i, chunk);
        Flash_Read(offset + i, tmpbuf, chunk);

        //printf("upload FLASH:\n");
        //send_binary(tmpbuf, chunk);

        if (!SDIO_WriteF1Memory(i, tmpbuf, chunk))
            return 0;

        i += chunk;
    }

    printf("firmware upload done, verifying\n");

    u8* verbuf = (u8*)memalign(16, tmplen);
    for (int i = 0; i < length; )
    {
        int chunk = tmplen;
        if ((i + chunk) > length)
            chunk = length - i;

        Flash_Read(offset + i, tmpbuf, chunk);

        if (!SDIO_ReadF1Memory(i, verbuf, chunk))
        {
            printf("read failed\n");
            return 0;
        }

        /*printf("FLASH:\n");
        send_binary(tmpbuf, chunk);
        printf("SDIO:\n");
        send_binary(verbuf, chunk);*/

        if (memcmp(tmpbuf, verbuf, chunk))
        {
            printf("%08X: error\n", i);
            /*printf("%08X %08X %08X %08X\n", *(u32*)&tmpbuf[0], *(u32*)&tmpbuf[4], *(u32*)&tmpbuf[8], *(u32*)&tmpbuf[12]);
            printf("%08X %08X %08X %08X\n", *(u32*)&verbuf[0], *(u32*)&verbuf[4], *(u32*)&verbuf[8], *(u32*)&verbuf[12]);
            printf("%08X %08X %08X %08X\n", *(u32*)&tmpbuf[16+0], *(u32*)&tmpbuf[16+4], *(u32*)&tmpbuf[16+8], *(u32*)&tmpbuf[16+12]);
            printf("%08X %08X %08X %08X\n", *(u32*)&verbuf[16+0], *(u32*)&verbuf[16+4], *(u32*)&verbuf[16+8], *(u32*)&verbuf[16+12]);*/
            return 0;
        }

        i += chunk;
    }
    printf("OK\n");
    free(verbuf);

    Wifi_SetUploadState(0);
    SDIO_SetClocks(1, 0);

    free(tmpbuf);
    return 1;
}
