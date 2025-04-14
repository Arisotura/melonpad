#ifndef _BOOT_LIST_H_
#define _BOOT_LIST_H_

// Flash map flags
#define Map_Free        0   // free slot
#define Map_FreeUnsafe  1   // free, but might be overwritten
#define Map_Taken       2   // slot taken
#define Map_TakenCont   3   // taken, but in continuity
#define Map_Reserved    255 // reserved slot

typedef struct sBootEntry
{
    u32 Offset;
    u32 Size;
    u32 Slot;
    u32 NumSlots;
    char Title[256];

} sBootEntry;

extern u8 FlashMap[32];
extern sBootEntry* FlashBootMap[32];
extern sBootEntry BootList[32];

void BuildBootList();

#endif //_BOOT_LIST_H_
