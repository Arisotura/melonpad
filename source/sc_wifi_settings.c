#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "main.h"
#include "sc_wifi_settings.h"
#include "sc_wifi_scan.h"

// LAYOUT for wifi settings (EEPROM offset 0x600)
// 00: SSID
// 20: wifi auth
// 21: wifi security
// 22: passphrase
// 62: 0=auto IP, 1=manual IP
// 63: IP addr
// 67: subnet mask
// 6B: gateway
// 7E: CRC16

static const u8 wifi_auth_vals[] = {WIFI_AUTH_OPEN, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, 0xFF};
static const char* wifi_auth_labels = "Open\nWPA-PSK\nWPA2-PSK";

static const u8 wifi_sec_vals[] = {WIFI_SEC_TKIP|WIFI_SEC_AES, WIFI_SEC_TKIP, WIFI_SEC_AES, 0xFF};
static const char* wifi_sec_labels = "Auto\nTKIP\nAES";

static const char* keyboard_ip[] = {
    " ", "1", "2", "3", " ", "\n",
    " ", "4", "5", "6", " ", "\n",
    " ", "7", "8", "9", " ", "\n",
    LV_SYMBOL_CLOSE, " ", ".", "0", LV_SYMBOL_BACKSPACE, " ", LV_SYMBOL_OK,
    NULL
};

static const lv_buttonmatrix_ctrl_t keyboard_ip_ctrl[] = {
    LV_BUTTONMATRIX_CTRL_HIDDEN | 8, 4, 4, 4, LV_BUTTONMATRIX_CTRL_HIDDEN | 8,
    LV_BUTTONMATRIX_CTRL_HIDDEN | 8, 4, 4, 4, LV_BUTTONMATRIX_CTRL_HIDDEN | 8,
    LV_BUTTONMATRIX_CTRL_HIDDEN | 8, 4, 4, 4, LV_BUTTONMATRIX_CTRL_HIDDEN | 8,
    4, LV_BUTTONMATRIX_CTRL_HIDDEN | 4, 4, 4, 4, LV_BUTTONMATRIX_CTRL_HIDDEN | 4, 4
};

sScreen scWifiSettings =
{
    .Open = ScWifiSettings_Open,
    .Close = ScWifiSettings_Close,
    .Activate = ScWifiSettings_Activate,
    .Update = ScWifiSettings_Update
};

static lv_obj_t* Screen;
static lv_obj_t* Keyboard;
static lv_obj_t* BtnPane;

static lv_obj_t* TxtSSID;
static lv_obj_t* SelAuth;
static lv_obj_t* SelSec;
static lv_obj_t* TxtPassphrase;

static lv_obj_t* ChkAutoIP;
static lv_obj_t* TxtIPAddr;
static lv_obj_t* TxtSubnet;
static lv_obj_t* TxtGateway;


static int ipset(const u8* ip)
{
    if (ip[0]) return 1;
    if (ip[1]) return 1;
    if (ip[2]) return 1;
    if (ip[3]) return 1;
    return 0;
}

static void ip2str(char* str, const u8* ip)
{
    // ip = 0D00A8C0
    sprintf(str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

static int str2ip(u8* ip, const char* str)
{
    u32 _ip[4];
    int res = sscanf(str, "%d.%d.%d.%d", &_ip[0], &_ip[1], &_ip[2], &_ip[3]);
    if (res < 4) return 0;
    for (int i = 0; i < 4; i++)
    {
        if (_ip[i] > 255) return 0;
        ip[i] = _ip[i];
    }
    return 1;
}


static void OnBack(lv_event_t* event)
{
    ScCloseCurrent(0, NULL);
}

static void OnApply(lv_event_t* event)
{
    u8 ipflag;
    u8 ip[4];
    u8 subnet[4];
    u8 gateway[4];
    char* str;
    int val;

    if (lv_obj_get_state(ChkAutoIP) & LV_STATE_CHECKED)
    {
        // collect valid IPs anyway, to save them
        // just ignore invalid ones

        ipflag = 0;

        str = lv_textarea_get_text(TxtIPAddr);
        if (!str2ip(ip, str))
            memset(ip, 0, 4);

        str = lv_textarea_get_text(TxtSubnet);
        if (!str2ip(subnet, str))
            memset(subnet, 0, 4);

        str = lv_textarea_get_text(TxtGateway);
        if (!str2ip(gateway, str))
            memset(gateway, 0, 4);
    }
    else
    {
        ipflag = 1;

        str = lv_textarea_get_text(TxtIPAddr);
        if (!str2ip(ip, str))
        {
            ScMsgBox("Error", "Invalid IP address.", NULL, "OK", NULL);
            return;
        }

        str = lv_textarea_get_text(TxtSubnet);
        if (!str2ip(subnet, str))
        {
            ScMsgBox("Error", "Invalid subnetwork mask.", NULL, "OK", NULL);
            return;
        }

        str = lv_textarea_get_text(TxtGateway);
        if (!str2ip(gateway, str))
        {
            ScMsgBox("Error", "Invalid default gateway address.", NULL, "OK", NULL);
            return;
        }
    }

    // save settings
    u8 settings[128] = {0};

    str = lv_textarea_get_text(TxtSSID);
    strncpy(&settings[0x00], str, 32);

    val = lv_dropdown_get_selected(SelAuth);
    settings[0x20] = wifi_auth_vals[val];

    val = lv_dropdown_get_selected(SelSec);
    settings[0x21] = wifi_sec_vals[val];

    str = lv_textarea_get_text(TxtPassphrase);
    strncpy(&settings[0x22], str, 64);

    settings[0x62] = ipflag;
    memcpy(&settings[0x63], ip, 4);
    memcpy(&settings[0x67], subnet, 4);
    memcpy(&settings[0x6B], gateway, 4);

    SetCRC16(settings, 126);

    UIC_WriteEnable();
    UIC_WriteEEPROM(0x600, settings, 128);
    UIC_WriteDisable();

    ScCloseCurrent(0, NULL);
}

static void OnScanRet(int res, void* data)
{
    if (res == 1)
    {
        sWifiScanResult* res = (sWifiScanResult*)data;
        char tmp[65];

        strncpy(tmp, res->SSID, 32); tmp[32] = '\0';
        lv_textarea_set_text(TxtSSID, tmp);

        lv_dropdown_set_selected(SelAuth, 0);
        for (int i = 0; wifi_auth_vals[i] != 0xFF; i++)
        {
            if (wifi_auth_vals[i] == res->AuthType)
            {
                lv_dropdown_set_selected(SelAuth, i);
                break;
            }
        }

        lv_dropdown_set_selected(SelSec, 0);
        for (int i = 0; wifi_sec_vals[i] != 0xFF; i++)
        {
            if (wifi_sec_vals[i] == res->Security)
            {
                lv_dropdown_set_selected(SelSec, i);
                break;
            }
        }

        strncpy(tmp, res->Passphrase, 64); tmp[64] = '\0';
        lv_textarea_set_text(TxtPassphrase, tmp);
    }
}

static void OnNetworkSearch(lv_event_t* event)
{
    ScOpen(&scWifiScan, OnScanRet);
}

static void OnTextareaEvent(lv_event_t* event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t* target = lv_event_get_target(event);

    if (lv_obj_get_state(target) & LV_STATE_DISABLED)
        return;

    if (code == LV_EVENT_FOCUSED ||
        code == LV_EVENT_CLICKED)
    {
        lv_keyboard_set_textarea(Keyboard, target);
        lv_obj_remove_flag(Keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(BtnPane, LV_OBJ_FLAG_HIDDEN);

        if (target == TxtIPAddr ||
            target == TxtSubnet ||
            target == TxtGateway)
            lv_keyboard_set_mode(Keyboard, LV_KEYBOARD_MODE_USER_1);
        else
            lv_keyboard_set_mode(Keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);

        lv_obj_scroll_to_view_recursive(target, LV_ANIM_OFF);
    }
    else if (code == LV_EVENT_DEFOCUSED ||
             code == LV_EVENT_CANCEL ||
             code == LV_EVENT_READY)
    {
        lv_keyboard_set_textarea(Keyboard, NULL);
        lv_obj_add_flag(Keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(BtnPane, LV_OBJ_FLAG_HIDDEN);
    }
}

static void OnChangeAuth(lv_event_t* event)
{
    lv_obj_t* target = lv_event_get_target_obj(event);
    int sel = lv_dropdown_get_selected(target);
    if (sel == 0)
    {
        lv_obj_add_state(SelSec, LV_STATE_DISABLED);
        lv_dropdown_set_text(SelSec, "");
    }
    else
    {
        lv_obj_remove_state(SelSec, LV_STATE_DISABLED);
        lv_dropdown_set_text(SelSec, NULL);
    }
}

static void OnChangeAutoIP(lv_event_t* event)
{
    lv_obj_t* target = lv_event_get_target_obj(event);
    if (lv_obj_get_state(target) & LV_STATE_CHECKED)
    {
        lv_obj_add_state(TxtIPAddr, LV_STATE_DISABLED);
        lv_obj_add_state(TxtSubnet, LV_STATE_DISABLED);
        lv_obj_add_state(TxtGateway, LV_STATE_DISABLED);
        lv_obj_remove_flag(TxtIPAddr, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(TxtSubnet, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(TxtGateway, LV_OBJ_FLAG_CLICKABLE);
    }
    else
    {
        lv_obj_remove_state(TxtIPAddr, LV_STATE_DISABLED);
        lv_obj_remove_state(TxtSubnet, LV_STATE_DISABLED);
        lv_obj_remove_state(TxtGateway, LV_STATE_DISABLED);
        lv_obj_add_flag(TxtIPAddr, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(TxtSubnet, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(TxtGateway, LV_OBJ_FLAG_CLICKABLE);
    }
}

void ScWifiSettings_Open()
{
    lv_obj_t* label;
    lv_obj_t* field;

    char str[65];
    u8 settings[128];
    u8* ip;

    NwSetAutoConnect(0);
    NwDisconnect();

    UIC_ReadEEPROM(0x600, settings, 128);
    if (!CheckCRC16(settings, 126))
        memset(settings, 0, 128);

    Screen = lv_obj_create(NULL);
    lv_obj_t* body = ScAddTopbar(Screen, "Wifi settings");


    lv_obj_t* pane = lv_obj_create(body);
    lv_obj_set_size(pane, lv_pct(70), lv_pct(100));
    lv_obj_align(pane, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(pane, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pane, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);


    label = lv_label_create(pane);
    lv_label_set_text(label, "Network name:");
    lv_obj_set_flex_grow(label, 1);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    field = lv_textarea_create(pane);
    TxtSSID = field;
    lv_textarea_set_one_line(field, 1);
    lv_textarea_set_max_length(field, 32);
    lv_obj_set_flex_grow(field, 2);
    lv_obj_add_event_cb(field, OnTextareaEvent, LV_EVENT_ALL, NULL);

    if (settings[0x00])
    {
        strncpy(str, &settings[0x00], 32); str[32] = '\0';
        lv_textarea_set_text(field, str);
    }

    field = lv_button_create(pane);
    lv_obj_add_event_cb(field, OnNetworkSearch, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(field);
    lv_label_set_text(label, "Search");
    lv_obj_center(label);


    label = lv_label_create(pane);
    lv_label_set_text(label, "Security:");
    lv_obj_set_flex_grow(label, 1);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    field = lv_dropdown_create(pane);
    SelAuth = field;
    lv_dropdown_set_options(field, wifi_auth_labels);
    lv_obj_set_flex_grow(field, 1);
    lv_obj_add_event_cb(field, OnChangeAuth, LV_EVENT_VALUE_CHANGED, NULL);

    for (int i = 0; wifi_auth_vals[i] != 0xFF; i++)
    {
        if (wifi_auth_vals[i] == settings[0x20])
        {
            lv_dropdown_set_selected(field, i);
            break;
        }
    }

    field = lv_dropdown_create(pane);
    SelSec = field;
    lv_dropdown_set_options(field, wifi_sec_labels);
    lv_obj_set_flex_grow(field, 1);

    for (int i = 0; wifi_sec_vals[i] != 0xFF; i++)
    {
        if (wifi_sec_vals[i] == settings[0x21])
        {
            lv_dropdown_set_selected(field, i);
            break;
        }
    }


    label = lv_label_create(pane);
    lv_label_set_text(label, "Passphrase:");
    lv_obj_set_flex_grow(label, 1);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    field = lv_textarea_create(pane);
    TxtPassphrase = field;
    lv_textarea_set_one_line(field, 1);
    lv_textarea_set_password_mode(field, 1);
    lv_textarea_set_max_length(field, 64);
    lv_obj_set_flex_grow(field, 2);
    lv_obj_add_event_cb(field, OnTextareaEvent, LV_EVENT_ALL, NULL);

    if (settings[0x22])
    {
        strncpy(str, &settings[0x22], 64); str[64] = '\0';
        lv_textarea_set_text(field, str);
    }


    field = lv_checkbox_create(pane);
    ChkAutoIP = field;
    lv_checkbox_set_text(field, "Automatically obtain IP address");
    lv_obj_add_flag(field, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_add_event_cb(field, OnChangeAutoIP, LV_EVENT_VALUE_CHANGED, NULL);
    if (!settings[0x62])
        lv_obj_add_state(field, LV_STATE_CHECKED);


    label = lv_label_create(pane);
    lv_label_set_text(label, "IP address:");
    lv_obj_set_flex_grow(label, 2);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    field = lv_textarea_create(pane);
    TxtIPAddr = field;
    lv_textarea_set_one_line(field, 1);
    lv_textarea_set_max_length(field, 15);
    lv_obj_set_flex_grow(field, 1);
    lv_obj_add_event_cb(field, OnTextareaEvent, LV_EVENT_ALL, NULL);

    ip = &settings[0x63];
    if (ipset(ip))
    {
        ip2str(str, ip);
        lv_textarea_set_text(field, str);
    }


    label = lv_label_create(pane);
    lv_label_set_text(label, "Subnetwork mask:");
    lv_obj_set_flex_grow(label, 2);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    field = lv_textarea_create(pane);
    TxtSubnet = field;
    lv_textarea_set_one_line(field, 1);
    lv_textarea_set_max_length(field, 15);
    lv_obj_set_flex_grow(field, 1);
    lv_obj_add_event_cb(field, OnTextareaEvent, LV_EVENT_ALL, NULL);

    ip = &settings[0x67];
    if (ipset(ip))
    {
        ip2str(str, ip);
        lv_textarea_set_text(field, str);
    }


    label = lv_label_create(pane);
    lv_label_set_text(label, "Default gateway:");
    lv_obj_set_flex_grow(label, 2);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    field = lv_textarea_create(pane);
    TxtGateway = field;
    lv_textarea_set_one_line(field, 1);
    lv_textarea_set_max_length(field, 15);
    lv_obj_set_flex_grow(field, 1);
    lv_obj_add_event_cb(field, OnTextareaEvent, LV_EVENT_ALL, NULL);

    ip = &settings[0x6B];
    if (ipset(ip))
    {
        ip2str(str, ip);
        lv_textarea_set_text(field, str);
    }


    lv_obj_t* btnpane = ScAddButtonPane(Screen);
    BtnPane = btnpane;

    lv_obj_t* btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnBack, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);

    label = lv_label_create(btn);
    //lv_label_set_text(label, "Back");
    lv_label_set_text(label, "Cancel");
    lv_obj_center(label);

    btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnApply, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);

    label = lv_label_create(btn);
    //lv_label_set_text(label, "Apply");
    lv_label_set_text(label, "OK");
    lv_obj_center(label);

    Keyboard = lv_keyboard_create(Screen);
    lv_obj_add_flag(Keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_map(Keyboard, LV_KEYBOARD_MODE_USER_1, keyboard_ip, keyboard_ip_ctrl);


    lv_obj_send_event(SelAuth, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_send_event(ChkAutoIP, LV_EVENT_VALUE_CHANGED, NULL);
}

void ScWifiSettings_Close()
{
    lv_obj_delete(Screen);

    NwLoadSettings();
    NwSetAutoConnect(1);
    NwConnect();
}

void ScWifiSettings_Activate()
{
    lv_screen_load(Screen);
}

void ScWifiSettings_Update()
{
}
