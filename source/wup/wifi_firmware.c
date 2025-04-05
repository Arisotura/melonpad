#include <wup/wup.h>

static u32 RAMSize;


int Wifi_GetRAMSize()
{
    RAMSize = Wifi_AI_GetRAMSize();
    if (!RAMSize)
        return 0;
}

int Wifi_StartFwUpload()
{
    if (!Wifi_AI_SetCore(WIFI_CORE_ARMCM3))
        return 0;

    Wifi_AI_DisableCore(0);

    if (!Wifi_AI_SetCore(WIFI_CORE_SOCRAM))
        return 0;

    Wifi_AI_ResetCore(0, 0);

    u32 zero = 0;
    SDIO_WriteF1Memory(RAMSize - 4, &zero, 4);

    return 1;
}

int Wifi_FinishFwUpload()
{
    if (!Wifi_AI_SetCore(WIFI_CORE_SDIOD))
        return 0;

    Wifi_AI_WriteCoreMem(0x020, 0xFFFFFFFF);

    if (!Wifi_AI_SetCore(WIFI_CORE_ARMCM3))
        return 0;

    Wifi_AI_ResetCore(0, 0);
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
    if (!Wifi_StartFwUpload()) return 0;

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

    tmpbuf = (u8*)memalign(16, nv_length);
    Flash_Read(nv_offset, tmpbuf, nv_length);

    nv_length = (nv_length + 3) & ~3;
    u32 var_addr = RAMSize - 4 - nv_length;
    if (!SDIO_WriteF1Memory(var_addr, tmpbuf, nv_length))
        return 0;

    u32 len_token = (nv_length >> 2) & 0xFFFF;
    len_token = len_token | ((~len_token) << 16);
    if (!SDIO_WriteF1Memory(RAMSize - 4, &len_token, 4))
        return 0;

    free(tmpbuf);
    printf("NVRAM uploaded\n");

    Wifi_FinishFwUpload();
    SDIO_SetClocks(1, 0);

    return 1;
}
