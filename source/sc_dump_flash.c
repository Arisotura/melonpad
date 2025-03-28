#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>
#include "lwip/apps/tftp_server.h"

#include "main.h"
#include "sc_dump_flash.h"


sScreen scDumpFlash =
{
    .Open = ScDumpFlash_Open,
    .Close = ScDumpFlash_Close,
    .Activate = ScDumpFlash_Activate,
    .Update = ScDumpFlash_Update
};

void* DfOpen(const char* fname, const char* mode, u8_t write);
void DfClose(void* handle);
int DfRead(void* handle, void* buf, int bytes);
int DfWrite(void* handle, struct pbuf* p);
void DfError(void* handle, int err, const char* msg, int size);

static struct tftp_context CtxDumpFlash =
{
    .open = DfOpen,
    .close = DfClose,
    .read = DfRead,
    .write = DfWrite,
    .error = DfError
};

#define FILE_MAGIC 0x4C494653

typedef struct sFile
{
    u32 Magic;
    u32 StartOffset;
    u32 Length;
    u32 Position;

} sFile;

static lv_obj_t* Screen;
static lv_obj_t* NwStateLabel;
static lv_obj_t* TftpStateLabel;
static lv_obj_t* TftpProgressBar;

static u8 LastNwState;
static u8 TftpState;
static sFile* TftpFile;


static void OnBack(lv_event_t* event)
{
    if (TftpState)
    {
        tftp_cleanup();
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
            tftp_cleanup();
            TftpState = 0;
        }
        break;

    case 1: // connecting
        lv_label_set_text(NwStateLabel, "Network status: connecting...");
        lv_label_set_text(TftpStateLabel, "");
        if (TftpState)
        {
            tftp_cleanup();
            TftpState = 0;
        }
        break;

    case 2: // connected
        {
            lv_label_set_text(NwStateLabel, "Network status: connected");

            if (tftp_init_server(&CtxDumpFlash) == ERR_OK)
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
                lv_label_set_text(TftpStateLabel, "TFTP server: failed to start");
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

    if (TftpState)
    {
        tftp_cleanup();
        TftpState = 0;
    }

    if (TftpFile)
    {
        free(TftpFile);
        TftpFile = NULL;
    }
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


void* DfOpen(const char* fname, const char* mode, u8_t write)
{
    if (TftpFile)
        return NULL;
    if (write)
        return NULL;
    if (strncmp(mode, "octet", 5))
        return NULL;

    sFile* file = (sFile*)malloc(sizeof(sFile));
    file->Magic = FILE_MAGIC;
    file->StartOffset = 0;
    file->Length = 32*1024*1024;
    file->Position = 0;

    TftpFile = file;
    lv_obj_remove_flag(TftpProgressBar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(TftpProgressBar, 0, LV_ANIM_OFF);

    return file;
}

void DfClose(void* handle)
{
    sFile* file = (sFile*)handle;
    if (!file) return;
    if (file->Magic != FILE_MAGIC) return;
    free(file);

    if (file == TftpFile)
    {
        TftpFile = NULL;
        lv_obj_add_flag(TftpProgressBar, LV_OBJ_FLAG_HIDDEN);

        //ScMsgBox("Success", "FLASH successfully dumped!", NULL);
    }
}

int DfRead(void* handle, void* buf, int bytes)
{
    sFile* file = (sFile*)handle;
    if (!file) return -1;
    if (file->Magic != FILE_MAGIC) return -1;

    int lentoread = bytes;
    if ((file->Position + lentoread) > file->Length)
        lentoread = file->Length - file->Position;

    if (lentoread > 0)
    {
        Flash_Read(file->Position, (u8*)buf, lentoread);
        file->Position += lentoread;
    }

    if (file == TftpFile)
    {
        int progress = (file->Position * 100) / file->Length;
        lv_bar_set_value(TftpProgressBar, progress, LV_ANIM_OFF);
    }

    return lentoread;
}

int DfWrite(void* handle, struct pbuf* p)
{
    return -1;
}

void DfError(void* handle, int err, const char* msg, int size)
{
    ScMsgBox("Error", msg, NULL);
}
