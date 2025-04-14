#include <wup/wup.h>
#include "boot_list.h"


u8 FlashMap[32];
sBootEntry* FlashBootMap[32];
sBootEntry BootList[32];


sBootEntry* FillBootEntry(sBootEntry* entry, u32 offset)
{
    u32 indx[4];
    Flash_Read(offset, indx, 16);
    if (indx[0] != 0) return entry;
    if (indx[2] != 0x58444E49) return entry; // INDX

    u32 tbllen = indx[1];
    if (tbllen < 0x20) return entry;
    if (tbllen > 0x10000) return entry;
    if (tbllen & 0xF) return entry;

    int hasLVC = 0;
    int hasTITL = 0;
    char title[256] = {0};
    u32 maxlen = tbllen;

    for (u32 tbloff = 0x10; tbloff < tbllen; tbloff+=0x10)
    {
        u32 tblent[4];
        Flash_Read(offset + tbloff, tblent, 16);

        u32 endoff = tblent[0] + tblent[1];
        if (endoff > maxlen)
            maxlen = endoff;

        if (tblent[2] == 0x5F43564C) // LVC_
        {
            hasLVC = 1;
        }
        else if (tblent[2] == 0x4C544954) // TITL
        {
            hasTITL = 1;
            u32 tlen = tblent[1];
            if (tlen > 255) tlen = 255;
            Flash_Read(offset + tblent[0], title, tlen);
        }
    }

    if (!hasLVC) return entry;

    u32 slot = (offset >> 20) & 0x1F;
    u32 nslots = (maxlen + 0xFFFFF) >> 20;

    entry->Offset = offset;
    entry->Size = maxlen;
    entry->Slot = slot;
    entry->NumSlots = nslots;

    if (hasTITL)
        strncpy(entry->Title, title, 255);
    else if ((offset == 0x100000) || (offset == 0x500000))
        strncpy(entry->Title, "Stock firmware", 255);
    else if (offset == 0x1C00000)
        strncpy(entry->Title, "Service firmware", 255);
    else
        snprintf(entry->Title, 255, "Slot %d", slot);

    FlashMap[slot] = Map_Taken;
    FlashBootMap[slot] = entry;
    if (nslots > 1)
    {
        for (u32 i = 1; i < nslots; i++)
        {
            FlashMap[i] = Map_TakenCont;
            FlashBootMap[i] = entry;
        }
    }

    return ++entry;
}

void BuildBootList()
{
    memset(FlashMap, 0, sizeof(FlashMap));
    memset(FlashBootMap, 0, sizeof(FlashBootMap));
    memset(BootList, 0, sizeof(BootList));

    for (int i = 0; i < 32; i++)
        BootList[i].Offset = 0xFFFFFFFF;

    // mark reserved entries
    FlashMap[0x00] = Map_Reserved;

    // these are the language banks and IR data
    // for now we'll hardcode it, we can always make it nicer later
    for (int i = 0x09; i < 0x10; i++)
        FlashMap[i] = Map_Reserved;
    for (int i = 0x11; i < 0x18; i++)
        FlashMap[i] = Map_Reserved;
    for (int i = 0x19; i < 0x1B; i++)
        FlashMap[i] = Map_Reserved;

    // mark 'free but unsafe' slots
    for (int i = 0x01; i < 0x09; i++)
        FlashMap[i] = Map_FreeUnsafe;
    FlashMap[0x10] = Map_FreeUnsafe;
    FlashMap[0x18] = Map_FreeUnsafe;
    FlashMap[0x1B] = Map_FreeUnsafe;
    FlashMap[0x1F] = Map_FreeUnsafe;

    sBootEntry* entry = &BootList[0];

    // read 'stock firmware' entry
    u32 stockpart;
    u8 stocksel;
    Flash_Read(0xF000, &stocksel, 1);
    if (stocksel == 1)
        stockpart = 0x500000;
    else
        stockpart = 0x100000;

    entry = FillBootEntry(entry, stockpart);

    // read other entries
    for (int i = 0x01; i < 0x20; i++)
    {
        u8 map = FlashMap[i];
        if (map == Map_Reserved) continue;
        if (map == Map_Taken) continue;
        if (map == Map_TakenCont) continue;

        entry = FillBootEntry(entry, i << 20);
    }
}
