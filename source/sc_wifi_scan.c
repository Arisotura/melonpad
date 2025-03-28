#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "main.h"
#include "sc_wifi_scan.h"


extern const lv_image_dsc_t wifi_icons;
extern u8 qualoffset[6];

sScreen scWifiScan =
{
    .Open = ScWifiScan_Open,
    .Close = ScWifiScan_Close,
    .Activate = ScWifiScan_Activate,
    .Update = ScWifiScan_Update
};

static lv_obj_t* Screen;
static lv_obj_t* ScanList;
static lv_obj_t* PasswordPopup;
static lv_obj_t* PasswordField;
static lv_obj_t* PasswordOkBtn;
static lv_obj_t* ConnectPopup;
static lv_obj_t* Keyboard;
static lv_obj_t* ScanBtn;

static int JoinRes;
static u32 JoinTime;
static lv_obj_t* JoinMsgPopup;

static sWifiScanResult ScanResult;

static u8 DoScan;
static u8 Scanning;
static u32 LastScan;

static void StartScan();
static void JoinNetwork(sScanInfo* info, const char* pass);


static void OnBack(lv_event_t* event)
{
    ScCloseCurrent(0, NULL);
}

static void OnSearch(lv_event_t* event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t* target = lv_event_get_target_obj(event);

    if (lv_obj_get_state(target) & LV_STATE_DISABLED)
        return;

    int ischecked = (lv_obj_get_state(target) & LV_STATE_CHECKED);

    if ((code == LV_EVENT_LONG_PRESSED) && (!ischecked))
    {
        lv_obj_add_flag(target, LV_OBJ_FLAG_CHECKABLE);
        DoScan = 1;
    }
    else if (lv_obj_has_flag(target, LV_OBJ_FLAG_CHECKABLE) && (!ischecked))
    {
        lv_obj_remove_flag(target, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_add_flag(target, LV_OBJ_FLAG_USER_1);
        DoScan = 0;
    }
    else if (code == LV_EVENT_CLICKED)
    {
        if (lv_obj_has_flag(target, LV_OBJ_FLAG_USER_1))
            lv_obj_remove_flag(target, LV_OBJ_FLAG_USER_1);
        else
        {
            StartScan();
        }
    }
}

void ScWifiScan_Open()
{
    Screen = lv_obj_create(NULL);
    lv_obj_t* body = ScAddTopbar(Screen, "Search for wifi networks");


    ScanList = lv_list_create(body);
    lv_obj_set_size(ScanList, lv_pct(70), lv_pct(100));
    lv_obj_align(ScanList, LV_ALIGN_TOP_MID, 0, 0);


    lv_obj_t* btnpane = ScAddButtonPane(Screen);

    lv_obj_t* btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnBack, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "Cancel");
    lv_obj_center(label);

    btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnSearch, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(btn, OnSearch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn, OnSearch, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    ScanBtn = btn;

    label = lv_label_create(btn);
    lv_label_set_text(label, "Search");
    lv_obj_center(label);

    Keyboard = NULL;
}

void ScWifiScan_Close()
{
    if (Keyboard) lv_obj_delete(Keyboard);
    lv_obj_delete(Screen);
}

void ScWifiScan_Activate()
{
    memset(&ScanResult, 0, sizeof(ScanResult));

    //DoScan = 1;
    Scanning = 0;
    LastScan = 0;

    PasswordPopup = NULL;
    Keyboard = NULL;

    JoinRes = 1;
    JoinTime = 0;
    JoinMsgPopup = NULL;

    lv_screen_load(Screen);
    StartScan();
}

static void OnPwCancel(lv_event_t* event)
{
    lv_msgbox_close(PasswordPopup);
    lv_obj_delete(Keyboard); Keyboard = NULL;

    //DoScan = 1;
}

static void OnPwConnect(lv_event_t* event)
{
    if (Scanning) return;

    sScanInfo* info = (sScanInfo*)lv_event_get_user_data(event);
    const char* pass = lv_textarea_get_text(PasswordField);
    JoinNetwork(info, pass);

    lv_msgbox_close(PasswordPopup);
    lv_obj_delete(Keyboard); Keyboard = NULL;
}

/*static void OnPwType(lv_event_t* event)
{
    const char* pass = lv_textarea_get_text(PasswordField);
    if (strlen(pass) < 8)
        lv_obj_add_state(PasswordOkBtn, LV_STATE_DISABLED);
    else
        lv_obj_remove_state(PasswordOkBtn, LV_STATE_DISABLED);
}*/

static void OnConnCancel(lv_event_t* event)
{
    Wifi_Disconnect();

    lv_msgbox_close(ConnectPopup);
}

static void OpenPasswordPopup(sScanInfo* info)
{
    DoScan = 0;
    lv_obj_remove_state(ScanBtn, LV_STATE_CHECKED);
    lv_obj_remove_flag(ScanBtn, LV_OBJ_FLAG_CHECKABLE);

    //lv_obj_t* msgbox = lv_msgbox_create(lv_screen_active());
    lv_obj_t* msgbox = lv_msgbox_create(NULL);
    lv_obj_set_width(msgbox, 500);
    lv_obj_set_y(msgbox, lv_pct(-25));

    char text[100];
    snprintf(text, 100, "Connecting to %s...", info->SSID);
    lv_msgbox_add_title(msgbox, text);

    lv_obj_t* content = lv_msgbox_get_content(msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, "Enter passphrase:");

    lv_obj_t* textbox = lv_textarea_create(content);
    lv_textarea_set_one_line(textbox, 1);
    lv_textarea_set_password_mode(textbox, 1);
    lv_textarea_set_max_length(textbox, 64);
    lv_obj_set_flex_grow(textbox, 1);
    lv_obj_add_event_cb(textbox, OnPwCancel, LV_EVENT_CANCEL, info);
    lv_obj_add_event_cb(textbox, OnPwConnect, LV_EVENT_READY, info);
    //lv_obj_add_event_cb(textbox, OnPwType, LV_EVENT_VALUE_CHANGED, info);

    lv_obj_t* btn;
    btn = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_add_event_cb(btn, OnPwCancel, LV_EVENT_CLICKED, info);
    //btn = lv_msgbox_add_footer_button(msgbox, "Connect");
    btn = lv_msgbox_add_footer_button(msgbox, "OK");
    lv_obj_add_event_cb(btn, OnPwConnect, LV_EVENT_CLICKED, info);
    PasswordOkBtn = btn;

    lv_obj_t* footer = lv_msgbox_get_footer(msgbox);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    PasswordPopup = msgbox;
    PasswordField = textbox;

    // keyboard
    // we have to create it here so that it shows up over the msgbox's modal bg layer
    Keyboard = lv_keyboard_create(lv_layer_top());
    lv_keyboard_set_textarea(Keyboard, textbox);
}

static void OpenConnectingPopup(sScanInfo* info)
{
    DoScan = 0;
    lv_obj_remove_state(ScanBtn, LV_STATE_CHECKED);
    lv_obj_remove_flag(ScanBtn, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_t* msgbox = lv_msgbox_create(NULL);
    lv_obj_set_width(msgbox, 500);

    char text[100];
    snprintf(text, 100, "Connecting to %s...", info->SSID);
    lv_msgbox_add_title(msgbox, text);

    lv_obj_t* content = lv_msgbox_get_content(msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, "Connecting...");

    lv_obj_t* btn;
    btn = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_add_event_cb(btn, OnConnCancel, LV_EVENT_CLICKED, info);

    lv_obj_t* footer = lv_msgbox_get_footer(msgbox);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ConnectPopup = msgbox;
}

static void OnSelectAP(lv_event_t* event)
{
    if (Scanning) return;

    sScanInfo* info = (sScanInfo*)lv_event_get_user_data(event);
    if (info->AuthType != WIFI_AUTH_OPEN)
        OpenPasswordPopup(info);
    else
        JoinNetwork(info, "");
}

static void ScanCallback(sScanInfo* list, int num)
{
    // add in the new entries
    sScanInfo* info = list;
    while (info)
    {
        lv_obj_t* btn = lv_list_add_button(ScanList, NULL, info->SSID);
        lv_obj_add_event_cb(btn, OnSelectAP, LV_EVENT_CLICKED, info);

        char* security;
        if (info->AuthType == WIFI_AUTH_WPA2_PSK)
            security = "WPA2-PSK";
        else if (info->AuthType == WIFI_AUTH_WPA2_UNSPEC)
            security = "WPA2-???";
        else if (info->AuthType == WIFI_AUTH_WPA_PSK)
            security = "WPA-PSK";
        else if (info->AuthType == WIFI_AUTH_WPA_UNSPEC)
            security = "WPA-???";
        else if (info->AuthType == WIFI_AUTH_OPEN)
            security = "Open";
        else
            security = "???";

        lv_obj_t* seclabel = lv_label_create(btn);
        lv_label_set_text(seclabel, security);

        lv_obj_t* lvlicon = lv_image_create(btn);
        lv_obj_set_size(lvlicon, 16, 16);
        lv_image_set_src(lvlicon, &wifi_icons);
        lv_image_set_inner_align(lvlicon, LV_IMAGE_ALIGN_TOP_LEFT);
        lv_image_set_offset_x(lvlicon, -16 * qualoffset[info->SignalQuality]);
        lv_image_set_offset_y(lvlicon, -16);

        info = info->Next;
    }

    Scanning = 0;

    lv_obj_remove_state(ScanBtn, LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(ScanBtn, 0), "Search");
}

static void StartScan()
{
    LastScan = WUP_GetTicks();
    Scanning = 1;

    lv_obj_add_state(ScanBtn, LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(ScanBtn, 0), "Searching...");

    // clear the list
    u32 numchild = lv_obj_get_child_count_by_type(ScanList, &lv_list_button_class);
    for (u32 i = 0; i < numchild; i++)
    {
        lv_obj_t* child = lv_obj_get_child_by_type(ScanList, -1, &lv_list_button_class);
        lv_obj_delete(child);
    }

    if (!Wifi_StartScan(ScanCallback))
    {
        Scanning = 0;
        ScMsgBox("Error", "Failed to start network search.", NULL);

        lv_obj_remove_state(ScanBtn, LV_STATE_DISABLED);
        lv_label_set_text(lv_obj_get_child(ScanBtn, 0), "Search");
    }
}

static void OnJoinMsgOK()
{
    JoinMsgPopup = NULL;

    ScCloseCurrent(1, &ScanResult);
}

static void JoinCallback(int status)
{
    JoinRes = status;
    JoinTime = WUP_GetTicks();

    lv_msgbox_close(ConnectPopup);
    if (status == WIFI_JOIN_SUCCESS)
        JoinMsgPopup = ScMsgBox("Success", "Connection successful!", OnJoinMsgOK);
    else
    {
        char* msg;
        switch (status)
        {
        case WIFI_JOIN_BADPASS: msg = "Incorrect passphrase."; break;
        case WIFI_JOIN_BADSEC: msg = "This network isn't compatible with the Wii U Gamepad."; break;
        default: msg = "Failed to connect."; break;
        }

        JoinMsgPopup = ScMsgBox("Error", msg, NULL);
    }
}

static void JoinNetwork(sScanInfo* info, const char* pass)
{
    strncpy(ScanResult.SSID, info->SSID, 32);
    ScanResult.AuthType = info->AuthType;
    ScanResult.Security = info->Security;
    strncpy(ScanResult.Passphrase, pass, 64);

    OpenConnectingPopup(info);

    Wifi_SetDHCPEnable(0);
    Wifi_SetIPAddr(NULL, NULL, NULL);

    if (!Wifi_JoinNetwork(info->SSID, info->AuthType, info->Security, pass, JoinCallback))
    {
        lv_msgbox_close(ConnectPopup);
        ScMsgBox("Error", "Failed to connect.", NULL);
    }
}

void ScWifiScan_Update()
{
    u32 time = WUP_GetTicks();

    if (DoScan)
    {
        if ((time - LastScan) > 10000)
        {
            // start a wifi scan
            StartScan();
        }
    }

    if (JoinMsgPopup &&
        (JoinRes == WIFI_JOIN_SUCCESS) &&
        ((time - JoinTime) > 1000))
    {
        lv_msgbox_close(JoinMsgPopup);
        OnJoinMsgOK();
    }
}
