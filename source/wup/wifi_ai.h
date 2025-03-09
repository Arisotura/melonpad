#ifndef _WIFI_AI_H_
#define _WIFI_AI_H_

#define WIFI_CORE_BACKPLANE     0x800
#define WIFI_CORE_SOCRAM        0x80E
#define WIFI_CORE_SDIOD         0x829
#define WIFI_CORE_ARMCM3        0x82A

int Wifi_AI_Enumerate();

u32 Wifi_AI_GetRAMSize();

u32 Wifi_AI_GetCore();
int Wifi_AI_SetCore(u32 coreid);
u8 Wifi_AI_GetCoreRevision();

u32 Wifi_AI_GetCoreMemBase();
u32 Wifi_AI_ReadCoreMem(u32 addr);
void Wifi_AI_WriteCoreMem(u32 addr, u32 val);

int Wifi_AI_IsCoreUp();
void Wifi_AI_DisableCore(u32 ctrl);
void Wifi_AI_ResetCore(u32 ctrl, u32 resetctrl);

#endif // _WIFI_AI_H_
