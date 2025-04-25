#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "main.h"
#include "boot_list.h"
#include "sc_manage_apps.h"


sScreen scManageApps =
{
    .Open = ScManageApps_Open,
    .Close = ScManageApps_Close,
    .Activate = ScManageApps_Activate,
    .Update = ScManageApps_Update
};

static lv_obj_t* Screen;
static lv_obj_t* AppList;


static void AddBootEntry(int i, const char* defaulttitle)
{
    u8 map = FlashMap[i];
    sBootEntry* entry = FlashBootMap[i];
    lv_obj_t* list = AppList;
    lv_obj_t* btn;
    lv_obj_t* btn2;
    lv_obj_t* label;

    if ((map == Map_Free) || (map == Map_FreeUnsafe))
    {
        // empty entry

        if (defaulttitle)
            btn = lv_list_add_button(list, NULL, defaulttitle);
        else
        {
            char title[32];
            sprintf(title, "Slot %d", i);
            btn = lv_list_add_button(list, NULL, title);
        }
        //lv_obj_add_event_cb(btn, OnBootEntry, LV_EVENT_CLICKED, entry);

        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

        btn2 = lv_button_create(btn);
        label = lv_label_create(btn2);
        lv_label_set_text(label, "Install");
        lv_obj_center(label);
    }
    else
    {
        // filled entry

        lv_obj_t* btn = lv_list_add_button(list, NULL, entry->Title);
        //lv_obj_add_event_cb(btn, OnBootEntry, LV_EVENT_CLICKED, entry);

        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

        btn2 = lv_button_create(btn);
        label = lv_label_create(btn2);
        lv_label_set_text(label, "Delete");
        lv_obj_center(label);

        btn2 = lv_button_create(btn);
        label = lv_label_create(btn2);
        lv_label_set_text(label, "Dump");
        lv_obj_center(label);
    }
}

static void PopulateBootList()
{
    lv_obj_t* list = AppList;

    // clear the list
    u32 numchild = lv_obj_get_child_count_by_type(list, &lv_list_button_class);
    for (u32 i = 0; i < numchild; i++)
    {
        lv_obj_t* child = lv_obj_get_child_by_type(list, -1, &lv_list_button_class);
        lv_obj_delete(child);
    }

    // add the stock firmware entry first, even if none is installed
    // because there's special handling for it
    AddBootEntry(StockFwSlot, "Stock firmware slot");

    // fill in the new entries
    for (int i = 0; i < 32; i++)
    {
        if ((i == 1) || (i == 5)) continue;

        u8 map = FlashMap[i];
        if (map == Map_Reserved) continue;
        if (map == Map_TakenCont) continue;

        AddBootEntry(i, NULL);
    }
}


static void OnBack(lv_event_t* event)
{
    ScCloseCurrent(0, NULL);
}

void ScManageApps_Open(void* data)
{
    Screen = lv_obj_create(NULL);
    lv_obj_t* body = ScAddTopbar(Screen, "Manage apps");


    AppList = lv_list_create(body);
    //lv_obj_set_size(AppList, lv_pct(70), lv_pct(100));
    lv_obj_set_size(AppList, lv_pct(100), lv_pct(100));
    lv_obj_align(AppList, LV_ALIGN_TOP_MID, 0, 0);

    PopulateBootList();


    lv_obj_t* btnpane = ScAddButtonPane(Screen);

    lv_obj_t* btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnBack, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "Cancel");
    lv_obj_center(label);
}

void ScManageApps_Close()
{
    lv_obj_delete(Screen);
}

void ScManageApps_Activate()
{
    lv_screen_load(Screen);
}

void ScManageApps_Update()
{
}
