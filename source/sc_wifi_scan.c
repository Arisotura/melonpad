#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "sc_wifi_scan.h"

extern const lv_image_dsc_t wifi_0, wifi_1, wifi_2, wifi_3, wifi_4;
const lv_image_dsc_t* wifi_levels[6] = {&wifi_0, &wifi_0, &wifi_1, &wifi_2, &wifi_3, &wifi_4};

static lv_obj_t* Screen;
static lv_obj_t* ScanList;
static lv_obj_t* PasswordPopup;
static lv_obj_t* PasswordField;
static lv_obj_t* Keyboard;

static u8 DoScan;
static u8 Scanning;
static u32 LastScan;

static void StartScan();
static void JoinNetwork(sScanInfo* info, const char* pass);

#define ObjDelete(obj) do { if (obj) lv_obj_delete(obj); obj = NULL; } while (0)

void ScWifiScan_Init()
{
    Screen = lv_obj_create(NULL);

    ScanList = lv_list_create(Screen);
    lv_obj_set_size(ScanList, 800, 400);
    lv_obj_align(ScanList, LV_ALIGN_TOP_MID, 0, 5);

    //lv_obj_t * btn;
    // TODO make this a header and not part of the list?
    lv_list_add_text(ScanList, "Wifi networks");
}

void ScWifiScan_Enter()
{
    DoScan = 1;
    Scanning = 0;
    LastScan = 0;

    PasswordPopup = NULL;
    Keyboard = NULL;

    lv_screen_load(Screen);
    StartScan();
}

static void OnPwCancel(lv_event_t* event)
{
    ObjDelete(PasswordPopup);
    ObjDelete(Keyboard);

    DoScan = 1;
}

static void OnPwConnect(lv_event_t* event)
{
    if (Scanning) return;

    sScanInfo* info = (sScanInfo*)lv_event_get_user_data(event);
    const char* pass = lv_textarea_get_text(PasswordField);
    JoinNetwork(info, pass);

    ObjDelete(PasswordPopup);
    ObjDelete(Keyboard);
}

static void OpenPasswordPopup(sScanInfo* info)
{
    DoScan = 0;

    lv_obj_t* msgbox = lv_msgbox_create(lv_screen_active());
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
    lv_textarea_set_max_length(textbox, 96);
    lv_obj_set_flex_grow(textbox, 1);
    lv_obj_add_event_cb(textbox, OnPwCancel, LV_EVENT_CANCEL, info);
    lv_obj_add_event_cb(textbox, OnPwConnect, LV_EVENT_READY, info);

    lv_obj_t* btn;
    btn = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_add_event_cb(btn, OnPwCancel, LV_EVENT_CLICKED, info);
    btn = lv_msgbox_add_footer_button(msgbox, "Connect");
    lv_obj_add_event_cb(btn, OnPwConnect, LV_EVENT_CLICKED, info);

    lv_obj_t* footer = lv_msgbox_get_footer(msgbox);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    PasswordPopup = msgbox;
    PasswordField = textbox;

    // keyboard
    lv_obj_t* keyboard = lv_keyboard_create(lv_screen_active());
    lv_keyboard_set_textarea(keyboard, textbox);
    Keyboard = keyboard;
}

static void OpenConnectingPopup(sScanInfo* info)
{
    DoScan = 0;

    lv_obj_t* msgbox = lv_msgbox_create(lv_screen_active());
    lv_obj_set_width(msgbox, 500);
    lv_obj_set_y(msgbox, lv_pct(-25));

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
    lv_obj_add_event_cb(btn, OnPwCancel, LV_EVENT_CLICKED, info);

    lv_obj_t* footer = lv_msgbox_get_footer(msgbox);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // TODO assign it to something
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
        lv_image_set_src(lvlicon, wifi_levels[info->SignalQuality]);

        info = info->Next;
    }

    Scanning = 0;
}

static void StartScan()
{
    LastScan = WUP_GetTicks();
    Scanning = 1;

    // clear the list
    u32 numchild = lv_obj_get_child_count_by_type(ScanList, &lv_list_button_class);
    for (u32 i = 0; i < numchild; i++)
    {
        lv_obj_t* child = lv_obj_get_child_by_type(ScanList, -1, &lv_list_button_class);
        lv_obj_delete(child);
    }

    Wifi_StartScan(ScanCallback);
}

static void JoinCallback(int status)
{
    printf("JOIN: STATUS=%d\n", status);
}

static void JoinNetwork(sScanInfo* info, const char* pass)
{
    OpenConnectingPopup(info);

    Wifi_JoinNetwork(info->SSID, info->AuthType, info->Security, pass, JoinCallback);
}

void ScWifiScan_Update()
{
    if (DoScan)
    {
        u32 time = WUP_GetTicks();
        if ((time - LastScan) > 10000)
        {
            // start a wifi scan
            StartScan();
        }
    }
}
