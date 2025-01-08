#ifndef _WIFI_H_
#define _WIFI_H_

int Wifi_Init();

int Wifi_UploadFirmware();

void Wifi_WaitForRx(u8 flags);
void Wifi_CardIRQ();

int Wifi_StartScan(int passive);
int Wifi_StartScan2(int passive);
int Wifi_JoinNetwork();

#endif // _WIFI_H_
