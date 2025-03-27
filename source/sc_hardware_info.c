#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "main.h"
#include "sc_hardware_info.h"

const char* boardtypes[15] = {
    "DK1", "EP_DK2", "DP1", "DP2",
    "DK3", "DK4", "DP3", "DK5",
    "DP4", "DKMP", "DP5", "MASS",
    "DKMP2", "DRC_I", "???"
};

const char* regions[8] = {
    "Japan", "North America",
    "Europe", "China",
    "South Korea", "Taiwan",
    "Australia", "???"
};

sScreen scHardwareInfo =
{
    .Open = ScHardwareInfo_Open,
    .Close = ScHardwareInfo_Close,
    .Activate = ScHardwareInfo_Activate,
    .Update = ScHardwareInfo_Update
};

static lv_obj_t* Screen;


static void OnBack(lv_event_t* event)
{
    ScCloseCurrent(0, NULL);
}

void ScHardwareInfo_Open()
{
    char str[64];
    u8 tmp[16];

    Screen = lv_obj_create(NULL);
    lv_obj_t* body = ScAddTopbar(Screen, "Hardware info");


    lv_obj_t* table = lv_table_create(body);
    lv_obj_align(table, LV_ALIGN_TOP_MID, 0, 0);
    lv_table_set_row_count(table, 6);
    lv_table_set_column_count(table, 2);
    lv_table_set_column_width(table, 0, 200);
    lv_table_set_column_width(table, 1, 400);
    lv_obj_set_height(table, lv_pct(100));
    lv_obj_remove_style(table, NULL, LV_PART_ITEMS | LV_STATE_PRESSED);


    u32 hard_id = REG_HARDWARE_ID;
    //char* soc_type = ((hard_id & 0xFF) == 0x41) ? "Samsung" : "Renesas";

    u8 board_info;
    UIC_ReadEEPROM(0x100, tmp, 3);
    if (CheckCRC16(tmp, 1))
        board_info = tmp[0];
    else
    {
        UIC_ReadEEPROM(0x180, tmp, 3);
        if (CheckCRC16(tmp, 1))
            board_info = tmp[0];
        else
            board_info = 0xFF;
    }

    u8 board_type = board_info & 0xF;
    if (board_type > 13)
        board_type = 14;

    char* board_rev = AudioAmp_GetType() ? "20" : "01";

    u8 region;
    UIC_ReadEEPROM(0x103, tmp, 3);
    if (CheckCRC16(tmp, 1))
        region = tmp[0];
    else
    {
        UIC_ReadEEPROM(0x183, tmp, 3);
        if (CheckCRC16(tmp, 1))
            region = tmp[0];
        else
            region = 0xFF;
    }

    if (region > 6)
        region = 7;

    u8 mac[6];
    Wifi_GetMACAddr(mac);

    u32 uic_ver = UIC_GetFirmwareVersion();


    int r = 0;
    //sprintf(str, "0x%08X (%s)", hard_id, soc_type);
    sprintf(str, "0x%08X", hard_id);
    lv_table_set_cell_value(table, r, 0, "Hardware ID:");
    lv_table_set_cell_value(table, r, 1, str);

    r++;
    sprintf(str, "%s (0x%02X)", boardtypes[board_type], board_info);
    lv_table_set_cell_value(table, r, 0, "Board version:");
    lv_table_set_cell_value(table, r, 1, str);

    r++;
    lv_table_set_cell_value(table, r, 0, "Board revision:");
    lv_table_set_cell_value(table, r, 1, board_rev);

    r++;
    sprintf(str, "%s (%d)", regions[region], region);
    lv_table_set_cell_value(table, r, 0, "Region:");
    lv_table_set_cell_value(table, r, 1, str);

    r++;
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    lv_table_set_cell_value(table, r, 0, "MAC address:");
    lv_table_set_cell_value(table, r, 1, str);

    r++;
    sprintf(str, "%d.%d.%d.%d",
            uic_ver>>24, (uic_ver>>16)&0xFF, (uic_ver>>8)&0xFF, uic_ver&0xFF);
    lv_table_set_cell_value(table, r, 0, "UIC firmware:");
    lv_table_set_cell_value(table, r, 1, str);


    lv_obj_t* btnpane = ScAddButtonPane(Screen);

    lv_obj_t* btn = lv_button_create(btnpane);
    lv_obj_add_event_cb(btn, OnBack, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);
}

void ScHardwareInfo_Close()
{
    lv_obj_delete(Screen);
}

void ScHardwareInfo_Activate()
{
    lv_screen_load(Screen);
}

void ScHardwareInfo_Update()
{
}
