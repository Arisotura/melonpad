#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "main.h"
#include "sc_boot_menu.h"
#include "sc_wifi_settings.h"
#include "sc_hardware_info.h"

sScreen scBootMenu =
{
    .Open = ScBootMenu_Open,
    .Close = ScBootMenu_Close,
    .Activate = ScBootMenu_Activate,
    .Update = ScBootMenu_Update
};

static lv_obj_t* Screen;

//

#define ObjDelete(obj) do { if (obj) lv_obj_delete(obj); obj = NULL; } while (0)

static void OnOpenScreen(lv_event_t* event)
{
    sScreen* screen = (sScreen*)lv_event_get_user_data(event);
    ScOpen(screen, NULL);
}

void ScBootMenu_Open()
{
    Screen = lv_obj_create(NULL);
    lv_obj_t* body = ScAddTopbar(Screen, "melonpad v0.1");

    // TEST
    lv_obj_t* list1 = lv_list_create(body);
    lv_obj_set_size(list1, lv_pct(70), lv_pct(100));
    lv_obj_align(list1, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_group_t* grp = lv_group_create();

    lv_obj_t * btn;
    lv_list_add_text(list1, "Boot menu");
    btn = lv_list_add_button(list1, NULL, "Stock firmware");
    lv_group_add_obj(grp, btn);
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "lucario.fw");
    lv_group_add_obj(grp, btn);
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "Checkerboard");
    lv_group_add_obj(grp, btn);
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "Ass vibrator");
    lv_group_add_obj(grp, btn);
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "5 is red");
    lv_group_add_obj(grp, btn);
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);


    lv_obj_t* list2 = lv_list_create(body);
    lv_obj_set_size(list2, lv_pct(30), lv_pct(100));
    lv_obj_align(list2, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_list_add_text(list2, "App management");
    btn = lv_list_add_button(list2, LV_SYMBOL_DOWNLOAD, "Install app");
    btn = lv_list_add_button(list2, LV_SYMBOL_WIFI, "Wifi boot");
    lv_list_add_text(list2, "Settings");
    btn = lv_list_add_button(list2, LV_SYMBOL_SETTINGS, "Boot settings");
    btn = lv_list_add_button(list2, LV_SYMBOL_WIFI, "Wifi settings");
    lv_obj_add_event_cb(btn, OnOpenScreen, LV_EVENT_CLICKED, &scWifiSettings);
    btn = lv_list_add_button(list2, LV_SYMBOL_UPLOAD, "Dump FLASH");
    btn = lv_list_add_button(list2, LV_SYMBOL_LIST, "Hardware info");
    lv_obj_add_event_cb(btn, OnOpenScreen, LV_EVENT_CLICKED, &scHardwareInfo);
    btn = lv_list_add_button(list2, LV_SYMBOL_PLUS, "About");
}

void ScBootMenu_Close()
{
    lv_obj_delete(Screen);
}

void ScBootMenu_Activate()
{
    lv_screen_load(Screen);
}

//

void ScBootMenu_Update()
{
    //
}
