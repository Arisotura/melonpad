#ifndef _WIFI_H_
#define _WIFI_H_

#define WIFI_SEC_OPEN       0
#define WIFI_SEC_WEP        1
#define WIFI_SEC_WPA        2
#define WIFI_SEC_WPA2       3

typedef struct sScanInfo
{
    u8 SSIDLength;
    char SSID[32];

    u8 BSSID[6];
    u8 Channel;
    u8 Security;
    u16 Capabilities;

    s16 RSSI;
    u8 SignalQuality; // 0..5

    u32 Timestamp;
    struct sScanInfo* Prev;
    struct sScanInfo* Next;

} sScanInfo;

typedef void (*fnScanCb)(sScanInfo*);

int Wifi_Init();
void Wifi_DeInit();

int Wifi_UploadFirmware();

void Wifi_WaitForRx(u8 flags);
void Wifi_CardIRQ();

void Wifi_Update();

int Wifi_StartScan(fnScanCb callback);

/*int Wifi_StartScan(int passive);
int Wifi_StartScan2(int passive);
int Wifi_JoinNetwork(u32 wsec, u32 wpaauth);

void Wifi_Update();
void Wifi_Test();*/

#endif // _WIFI_H_
