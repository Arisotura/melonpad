#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>
#include "tftp.h"

#include "main.h"
#include "sc_tftp_send.h"


sScreen scTftpSend =
{
    .Open = ScTftpSend_Open,
    .Close = ScTftpSend_Close,
    .Activate = ScTftpSend_Activate,
    .Update = ScTftpSend_Update
};

//const int kFlashLen = 0x2000000;

static int OnTftpStart(const char* filename, const char* mode);
static int OnTftpRead(int pos, void* data, int len);
static void OnTftpFinish();
static void OnTftpError(u16 code, const char* msg);

static sTftpSendArgs* TftpArgs;

static lv_obj_t* Screen;
static lv_obj_t* NwStateLabel;
static lv_obj_t* TftpStateLabel[2];

static u8 Result;
static lv_obj_t* ResMsgBox;
static u32 ResMsgTime;

static u8 LastNwState;
static u8 TftpState;
static void* TftpCtx;
static int TftpPos;


static void OnBack(lv_event_t* event)
{
    ScCloseCurrent(0, NULL);
}

static void OnUpdateNwState()
{
    switch (LastNwState)
    {
    case 0: // disconnected
        lv_label_set_text(NwStateLabel, "Network status: not connected");
        lv_label_set_text(TftpStateLabel[0], "");
        lv_label_set_text(TftpStateLabel[1], "");
        if (TftpState)
        {
            TftpAbort(TftpCtx);
            TftpState = 0;
        }
        break;

    case 1: // connecting
        lv_label_set_text(NwStateLabel, "Network status: connecting...");
            lv_label_set_text(TftpStateLabel[0], "");
            lv_label_set_text(TftpStateLabel[1], "");
        if (TftpState)
        {
            TftpAbort(TftpCtx);
            TftpState = 0;
        }
        break;

    case 2: // connected
        {
            lv_label_set_text(NwStateLabel, "Network status: connected");

            TftpCtx = TftpSendStart(TFTP_PORT, OnTftpStart, OnTftpRead, OnTftpFinish, OnTftpError);
            if (TftpCtx)
            {
                u8 ip[4] = {0};
                Wifi_GetIPAddr(ip);

                char str[100];
                sprintf(str, "TFTP server: started, IP: %d.%d.%d.%d, port: %d",
                        ip[0], ip[1], ip[2], ip[3], TFTP_PORT);
                lv_label_set_text(TftpStateLabel[0], str);
                lv_label_set_text(TftpStateLabel[1], "Ready for download");
                TftpState = 1;
            }
            else
            {
                lv_label_set_text(TftpStateLabel[0], "TFTP server: failed to start");
                lv_label_set_text(TftpStateLabel[1], "");
                TftpState = 0;
            }
        }
        break;
    }
}

void ScTftpSend_Open(void* data)
{
    lv_obj_t* label;
    char str[64];

    Result = 0;
    ResMsgBox = NULL;
    ResMsgTime = 0;
    TftpArgs = (sTftpSendArgs*)data;

    TftpState = 0;
    TftpCtx = NULL;


    Screen = lv_obj_create(NULL);
    lv_obj_t* body = ScAddTopbar(Screen, TftpArgs->Title);


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
    TftpStateLabel[0] = label;

    label = lv_label_create(pane);
    lv_label_set_text(label, "");
    lv_obj_set_flex_grow(label, 1);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    TftpStateLabel[1] = label;


    lv_obj_t* btnpane = ScAddButtonPane(Screen);

    lv_obj_t* btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnBack, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);

    label = lv_label_create(btn);
    lv_label_set_text(label, "Cancel");
    lv_obj_center(label);
}

void ScTftpSend_Close()
{
    if (TftpState)
    {
        TftpAbort(TftpCtx);
        TftpState = 0;
    }

    lv_obj_delete(Screen);
}

void ScTftpSend_Activate()
{
    LastNwState = 0xFF;

    lv_screen_load(Screen);
}

static void OnMsgBoxOK(int btn)
{
    ResMsgBox = NULL;
    ScCloseCurrent(Result, NULL);
}

void ScTftpSend_Update()
{
    int nwstate = NwGetState();
    if (nwstate != LastNwState)
    {
        LastNwState = nwstate;
        OnUpdateNwState();
    }

    u32 time = WUP_GetTicks();

    if (ResMsgBox &&
        (Result == 1) &&
        ((time - ResMsgTime) > 1000))
    {
        lv_msgbox_close(ResMsgBox);
        OnMsgBoxOK(1);
    }
}


static int OnTftpStart(const char* filename, const char* mode)
{
    lv_lock();
    lv_label_set_text(TftpStateLabel[1], "Downloading... 0 KB");
    TftpPos = 0;
    lv_unlock();

    return TftpArgs->StartCB(TftpArgs, filename, mode);
}

static int OnTftpRead(int pos, void* data, int len)
{
    lv_lock();
    char progress[100];
    sprintf(progress, "Downloading... %d KB", TftpPos >> 10);
    lv_label_set_text(TftpStateLabel[1], progress);
    TftpPos += len;
    lv_unlock();

    return TftpArgs->ReadCB(TftpArgs, pos, data, len);
}

static void OnTftpFinish()
{
    TftpArgs->FinishCB(TftpArgs);

    lv_lock();
    Result = 1;
    ResMsgBox = ScMsgBox("Success", "Download finished!", NULL, "OK", OnMsgBoxOK);
    ResMsgTime = WUP_GetTicks();
    lv_unlock();
}

static void OnTftpError(u16 code, const char* msg)
{
    TftpArgs->ErrorCB(TftpArgs, code, msg);

    if (code == 10) // user abort code
        return;

    char title[32];
    sprintf(title, "Error %d", code);

    lv_lock();
    Result = 0;
    ResMsgBox = ScMsgBox(title, msg, NULL, "OK", OnMsgBoxOK);
    ResMsgTime = WUP_GetTicks();
    lv_unlock();
}
