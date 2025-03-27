#ifndef _SC_WIFI_SCAN_H_
#define _SC_WIFI_SCAN_H_

typedef struct sWifiScanResult
{
    char SSID[32];
    u8 AuthType, Security;
    char Passphrase[64];

} sWifiScanResult;

extern sScreen scWifiScan;

void ScWifiScan_Open();
void ScWifiScan_Close();
void ScWifiScan_Activate();
void ScWifiScan_Update();

#endif // _SC_WIFI_SCAN_H_
