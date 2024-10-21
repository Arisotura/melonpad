#include <wup/wup.h>


#define	MFG_ARM		    0x43B
#define	MFG_BROADCOM	0x4BF
#define	MFG_MIPS		0x4A7

typedef struct
{
    u32 CIa, CIb;

    u32 CoreID;
    u32 MemBase, MemSize;
    u32 MemBase2, MemSize2;
    u32 WrapperBase;

} sCoreInfo;

#define MAX_CORES 16
int NumCores;
sCoreInfo CoreInfo[MAX_CORES];
u32 OOBRouter;

sCoreInfo* CurCore;


static u32 GetEROMEntry(u32* addr, u32 mask, u32 wanted)
{
    u32 entry = 0;
    for (;;)
    {
        SDIO_ReadF1Memory(*addr, (u8*)&entry, 4);
        *addr += 4;

        if (!mask)
            break;

        if (!(entry & 0x1)) // not valid
            continue;

        if (entry == 0xF) // end
            break;

        if ((entry & mask) == wanted)
            break;
    }

    return entry;
}

static u32 GetAddrDesc(u32* addr, u32 wanted, u32* addrlo, u32* addrhi, u32* sizelo, u32* sizehi)
{
    u32 desc = GetEROMEntry(addr, 0x1, 0x1);
    if (((desc & 0x6) != 0x4) || ((desc & 0xFC0) != wanted))
    {
        *addr -= 4;
        return 0;
    }

    *addrlo = desc & 0xFFFFF000;
    if (desc & (1<<3))
        *addrhi = GetEROMEntry(addr, 0, 0);
    else
        *addrhi = 0;

    u32 size = desc & 0x30;
    if (size == 0x30)
    {
        u32 sizedesc = GetEROMEntry(addr, 0, 0);
        *sizelo = sizedesc & 0xFFFFF000;
        if (sizedesc & (1<<3))
            *sizehi = GetEROMEntry(addr, 0, 0);
        else
            *sizehi = 0;
    }
    else
    {
        *sizelo = 0x1000 << (size >> 4);
        *sizehi = 0;
    }

    return desc;
}

int Wifi_AI_Enumerate()
{
    u32 chipid = 0;
    if (!SDIO_ReadF1Memory(0x18000000, (u8*)&chipid, 4))
        return 0;

    if ((chipid >> 28) != 1)
    {
        printf("Wifi: error: backplane is not AI\n");
        return 0;
    }

    u32 eromptr = 0;
    SDIO_ReadF1Memory(0x180000FC, (u8*)&eromptr, 4);

    NumCores = 0;
    u32 eromend = eromptr + 0xE00;
    while (eromptr < eromend)
    {
        u32 cia = GetEROMEntry(&eromptr, 0xE, 0);
        if (cia == 0xF) // end
        {
            return 1;
        }

        u32 cib = GetEROMEntry(&eromptr, 0, 0);
        if ((cib & 0xE) != 0)
        {
            printf("BAD CIB %08X\n", cib);
            return 0;
        }

        u32 coreID = (cia >> 8) & 0xFFF;
        u32 mfg = (cia >> 20) & 0xFFF;
        u32 corerev = (cib >> 24) & 0xFF;
        u32 numMW = (cib >> 14) & 0x1F;     // master wrappers
        u32 numSW = (cib >> 19) & 0x1F;     // slave wrappers
        u32 numMP = (cib >> 4) & 0x1F;      // master ports
        u32 numSP = (cib >> 9) & 0x1F;      // slave ports

        if (numSP == 0)
            continue;
        if ((mfg == MFG_ARM) && (coreID == 0xFFF)) // default component, maps all
            continue;

        u32 addrdesc, addrlo, addrhi, sizelo, sizehi;

        if ((numMW + numSW) == 0)
        {
            printf("non-core %08X %08X\n", cia, cib);
            if (coreID == 0x367) // OOB router
            {
                addrdesc = GetAddrDesc(&eromptr, 0x000, &addrlo, &addrhi, &sizelo, &sizehi);
                if (addrdesc != 0)
                    OOBRouter = addrlo;
            }
            continue;
        }

        for (int i = 0; i < numMP; i++)
        {
            u32 mpentry = GetEROMEntry(&eromptr, 0x1, 0x1);
            if ((mpentry & 0xE) != 2)
            {
                printf("bad MP entry %08X\n", mpentry);
                return 0;
            }
        }

        int isbridge = 0;
        addrdesc = GetAddrDesc(&eromptr, 0x000, &addrlo, &addrhi, &sizelo, &sizehi);
        if (addrdesc == 0)
        {
            addrdesc = GetAddrDesc(&eromptr, 0x040, &addrlo, &addrhi, &sizelo, &sizehi);
            if (addrdesc != 0)
            {
                printf("it's a bridge: %08X\n", addrdesc);
                isbridge = 1;
            }
            else if ((addrhi != 0) || (sizehi != 0) || (sizelo != 0x1000))
            {
                printf("bad address descriptor\n");
                return 0;
            }
        }

        sCoreInfo* coreinfo = &CoreInfo[NumCores];
        coreinfo->CIa = cia;
        coreinfo->CIb = cib;
        coreinfo->CoreID = coreID;
        coreinfo->MemBase = addrlo;
        coreinfo->MemSize = sizelo;
        coreinfo->MemBase2 = 0;
        coreinfo->MemSize2 = 0;
        printf("--- COMPONENT %d (bridge=%d) ---\n", NumCores, isbridge);
        printf("cia[%d] = %08X\n", NumCores, cia);
        printf("cib[%d] = %08X\n", NumCores, cib);
        printf("coreid[%d] = %08X\n", NumCores, coreID);
        printf("coresba[%d] = %08X\n", NumCores, addrlo);
        printf("coresba_size[%d] = %08X\n", NumCores, sizelo);

        for (int j = 1; ; j++)
        {
            addrdesc = GetAddrDesc(&eromptr, 0x000, &addrlo, &addrhi, &sizelo, &sizehi);
            if (addrdesc == 0) break;
            if ((j == 1) && (sizelo == 0x1000))
            {
                coreinfo->MemBase2 = addrlo;
                coreinfo->MemSize2 = sizelo;
                printf("coresba2[%d] = %08X\n", NumCores, addrlo);
                printf("coresba2_size[%d] = %08X\n", NumCores, sizelo);
            }
        }

        for (int i = 0; i < numSP; i++)
        {
            int j;
            for (j = 0; ; j++)
            {
                addrdesc = GetAddrDesc(&eromptr, 0x000 | (i<<8), &addrlo, &addrhi, &sizelo, &sizehi);
                printf("j=%d desc=%08X\n", j, addrdesc);
                if (addrdesc == 0) break;
            }
            /*if (j == 0)
            {
                printf("error: no SP\n");
                return 0;
            }*/
        }

        for (int i = 0; i < numMW; i++)
        {
            addrdesc = GetAddrDesc(&eromptr, 0x0C0 | (i<<8), &addrlo, &addrhi, &sizelo, &sizehi);
            if ((addrdesc == 0) || (sizehi != 0) || (sizelo != 0x1000))
            {
                printf("error: no MW desc\n");
                return 0;
            }
            if (i == 0)
            {
                coreinfo->WrapperBase = addrlo;
                printf("[mw] wrapba[%d] = %08X\n", NumCores, addrlo);
            }
        }

        for (int i = 0; i < numSW; i++)
        {
            int j = i + ((numSP == 1) ? 0 : 1);
            addrdesc = GetAddrDesc(&eromptr, 0x080 | (j<<8), &addrlo, &addrhi, &sizelo, &sizehi);
            if ((addrdesc == 0) || (sizehi != 0) || (sizelo != 0x1000))
            {
                printf("error: no SW desc\n");
                return 0;
            }
            if ((numMW == 0) && (i == 0))
            {
                coreinfo->WrapperBase = addrlo;
                printf("[sw] wrapba[%d] = %08X\n", NumCores, addrlo);
            }
        }

        if (isbridge) continue;

        NumCores++;
        if (NumCores >= MAX_CORES)
            return 1;
    }

    return 1;
}


u32 Wifi_AI_GetRAMSize()
{
    if (!Wifi_AI_SetCore(WIFI_CORE_SOCRAM))
        return 0;

    int wasup = Wifi_AI_IsCoreUp();
    if (!wasup) Wifi_AI_ResetCore(0, 0);

    u32 rev = (CurCore->CIb >> 24) & 0xFF;
    u32 info = 0;
    SDIO_ReadF1Memory(CurCore->MemBase+0x000, (u8*)&info, 4);

    u32 ret;
    if (rev == 0)
    {
        ret = 1 << (16 + (info & 0xF));
    }
    else if (rev < 3)
    {
        ret = 1 << (14 + (info & 0xF));
        ret *= ((info >> 4) & 0xF);
    }
    else
    {
        u32 nb = ((info >> 4) & 0xF);
        u32 bsz = (info & 0xF);
        u32 lss = ((info >> 20) & 0xF);
        if (lss != 0) nb--;
        ret = nb * (1 << (14 + bsz));
        if (lss != 0) ret += (1 << (14 + (lss-1)));
    }

    if (!wasup) Wifi_AI_DisableCore(0);

    return ret;
}


u32 Wifi_AI_GetCore()
{
    return CurCore->CoreID;
}

int Wifi_AI_SetCore(u32 coreid)
{
    for (int i = 0; i < NumCores; i++)
    {
        sCoreInfo* core = &CoreInfo[i];
        if (core->CoreID == coreid)
        {
            CurCore = core;
            printf("core: %03X, rev=%d\n", CurCore->CoreID, CurCore->CIb >> 24);
            return 1;
        }
    }

    return 0;
}

u32 Wifi_AI_GetCoreMemBase()
{
    return CurCore->MemBase;
}

int Wifi_AI_IsCoreUp()
{
    u32 regval = 0;

    SDIO_ReadF1Memory(CurCore->WrapperBase+0x408, (u8*)&regval, 4);
    if ((regval & 0x03) != 0x01)
        return 0;

    SDIO_ReadF1Memory(CurCore->WrapperBase+0x800, (u8*)&regval, 4);
    if (regval & (1<<0)) // already in reset
        return 0;

    return 1;
}

void Wifi_AI_DisableCore(u32 ctrl)
{
    u32 regval = 0;
    SDIO_ReadF1Memory(CurCore->WrapperBase+0x800, (u8*)&regval, 4);
    if (regval & (1<<0)) // already in reset
        return;

    regval = ctrl;
    SDIO_WriteF1Memory(CurCore->WrapperBase+0x408, (u8*)&regval, 4);
    SDIO_ReadF1Memory(CurCore->WrapperBase+0x408, (u8*)&regval, 4);
    WUP_DelayUS(1);

    regval = (1<<0); // reset
    SDIO_WriteF1Memory(CurCore->WrapperBase+0x800, (u8*)&regval, 4);
    WUP_DelayUS(1);
}

void Wifi_AI_ResetCore(u32 ctrl, u32 resetctrl)
{
    u32 regval = 0;

    Wifi_AI_DisableCore(ctrl | resetctrl);

    regval = ctrl | (1<<0) | (1<<1);
    SDIO_WriteF1Memory(CurCore->WrapperBase+0x408, (u8*)&regval, 4);
    SDIO_ReadF1Memory(CurCore->WrapperBase+0x408, (u8*)&regval, 4);

    regval = 0;
    SDIO_WriteF1Memory(CurCore->WrapperBase+0x800, (u8*)&regval, 4);
    WUP_DelayUS(1);

    regval = ctrl | (1<<0);
    SDIO_WriteF1Memory(CurCore->WrapperBase+0x408, (u8*)&regval, 4);
    SDIO_ReadF1Memory(CurCore->WrapperBase+0x408, (u8*)&regval, 4);
    WUP_DelayUS(1);
}
