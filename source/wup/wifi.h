#ifndef _WIFI_H_
#define _WIFI_H_

#define WIFI_AUTH_OPEN          0x00
#define WIFI_AUTH_WPA_UNSPEC    0x02
#define WIFI_AUTH_WPA_PSK       0x04
#define WIFI_AUTH_WPA2_UNSPEC   0x40
#define WIFI_AUTH_WPA2_PSK      0x80

#define WIFI_SEC_TKIP           0x02
#define WIFI_SEC_AES            0x04

// status code for join callback
// TODO they may not reflect the entire complexity of reality
#define WIFI_JOIN_SUCCESS       0
#define WIFI_JOIN_FAIL          -1 // generic failure code
#define WIFI_JOIN_NOTFOUND      -2 // network not found
#define WIFI_JOIN_BADSEC        -3 // incorrect auth/security parameters
#define WIFI_JOIN_BADPASS       -4 // incorrect passphrase
#define WIFI_JOIN_TIMEOUT       -5 // timeout

typedef struct sScanInfo
{
    u8 SSIDLength;
    char SSID[32];

    u8 BSSID[6];
    u8 Channel;
    u8 AuthType;
    u8 Security;
    u16 Capabilities;

    s16 RSSI;
    u8 SignalQuality; // 0..5

    u32 Timestamp;
    struct sScanInfo* Prev;
    struct sScanInfo* Next;

} sScanInfo;

typedef void (*fnScanCb)(sScanInfo* list, int num);
typedef void (*fnJoinCb)(int status);

int Wifi_Init();
void Wifi_DeInit();

void Wifi_GetMACAddr(u8* addr);

void Wifi_SetClkEnable(int enable);

int Wifi_StartScan(fnScanCb callback);
void Wifi_CleanupScanList();

int Wifi_JoinNetwork(const char* ssid, u8 auth, u8 security, const char* pass, fnJoinCb callback);
int Wifi_Disconnect();

int Wifi_GetRSSI(s16* rssi, u8* quality);

void Wifi_SetDHCPEnable(int enable);
void Wifi_SetIPAddr(const u8* ip, const u8* subnet, const u8* gateway);
int Wifi_GetIPAddr(u8* ip);

#endif // _WIFI_H_
