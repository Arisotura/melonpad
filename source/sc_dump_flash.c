#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>
#include "tftp.h"

#include "main.h"
#include "sc_dump_flash.h"


sScreen scDumpFlash =
{
    .Open = ScDumpFlash_Open,
    .Close = ScDumpFlash_Close,
    .Activate = ScDumpFlash_Activate,
    .Update = ScDumpFlash_Update
};

const int kFlashLen = 0x2000000;

static int OnTftpStart(const char* filename, const char* mode);
static int OnTftpRead(int pos, void* data, int len);
static void OnTftpFinish();
static void OnTftpError(u16 code, const char* msg);

static lv_obj_t* Screen;
static lv_obj_t* NwStateLabel;
static lv_obj_t* TftpStateLabel;
static lv_obj_t* TftpProgressBar;

static u8 LastNwState;
static u8 TftpState;


static void OnBack(lv_event_t* event)
{
    if (TftpState)
    {
        //tftp_cleanup();
        TftpState = 0;
    }

    ScCloseCurrent(0, NULL);
}

static void OnUpdateNwState()
{
    switch (LastNwState)
    {
    case 0: // disconnected
        lv_label_set_text(NwStateLabel, "Network status: not connected");
        lv_label_set_text(TftpStateLabel, "");
        if (TftpState)
        {
            //tftp_cleanup();
            TftpState = 0;
        }
        break;

    case 1: // connecting
        lv_label_set_text(NwStateLabel, "Network status: connecting...");
        lv_label_set_text(TftpStateLabel, "");
        if (TftpState)
        {
            //tftp_cleanup();
            TftpState = 0;
        }
        break;

    case 2: // connected
        {
            lv_label_set_text(NwStateLabel, "Network status: connected");

            if (TftpSendStart(TFTP_PORT, OnTftpStart, OnTftpRead, OnTftpFinish, OnTftpError))
            {
                u8 ip[4] = {0};
                Wifi_GetIPAddr(ip);

                char str[100];
                sprintf(str, "TFTP server: started, IP: %d.%d.%d.%d, port: %d",
                        ip[0], ip[1], ip[2], ip[3], TFTP_PORT);
                lv_label_set_text(TftpStateLabel, str);
                TftpState = 1;
            }
            else
            {
                lv_label_set_text(TftpStateLabel, "FTP server: failed to start");
                TftpState = 0;
            }
        }
        break;
    }
}

void ScDumpFlash_Open()
{
    lv_obj_t* label;
    char str[64];

    Screen = lv_obj_create(NULL);
    lv_obj_t* body = ScAddTopbar(Screen, "Dump FLASH memory");


    lv_obj_t* pane = lv_obj_create(body);
    lv_obj_set_size(pane, lv_pct(70), lv_pct(100));
    lv_obj_align(pane, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(pane, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pane, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);


    label = lv_label_create(pane);
    lv_label_set_text(label, "");
    lv_obj_set_flex_grow(label, 1);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    NwStateLabel = label;

    label = lv_label_create(pane);
    lv_label_set_text(label, "");
    lv_obj_set_flex_grow(label, 1);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    TftpStateLabel = label;

    lv_obj_t* bar = lv_bar_create(pane);
    lv_obj_set_flex_grow(bar, 1);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    TftpProgressBar = bar;


    lv_obj_t* btnpane = ScAddButtonPane(Screen);

    lv_obj_t* btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnBack, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);

    label = lv_label_create(btn);
    lv_label_set_text(label, "Cancel");
    lv_obj_center(label);

    TftpState = 0;
}

void ScDumpFlash_Close()
{
    lv_obj_delete(Screen);

    // TODO clean up our mess here
}

void ScDumpFlash_Activate()
{
    LastNwState = 0xFF;

    lv_screen_load(Screen);
}

void ScDumpFlash_Update()
{
    int nwstate = NwGetState();
    if (nwstate != LastNwState)
    {
        LastNwState = nwstate;
        OnUpdateNwState();
    }
}


static int OnTftpStart(const char* filename, const char* mode)
{
    return 1;
}

static int OnTftpRead(int pos, void* data, int len)
{
    if (pos >= kFlashLen)
        return 0;

    if ((pos + len) > kFlashLen)
        len = kFlashLen - pos;

    Flash_Read(pos, data, len);
    return len;
}

static void OnTftpFinish()
{
    printf("TFTP finish\n");
}

static void OnTftpError(u16 code, const char* msg)
{
    printf("TFTP error %d: %s\n", code, msg);
}
