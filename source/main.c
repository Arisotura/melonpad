#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "main.h"
#include "sc_boot_menu.h"


extern const lv_image_dsc_t wifi_icons;
u8 qualoffset[6] = {0, 0, 1, 2, 3, 4};
u8 animoffset[6] = {1, 2, 3, 4, 3, 2};

u16* Framebuffer;

sScreen* scCurrent;
sScreen* scStack[8];
u8 scStackLevel;
u8 scCloseFlag;
int scCloseRes;
void* scCloseData;

static lv_style_t scScreenStyle;
static lv_style_t scTopbarStyle;
static lv_style_t scBodyStyle;

u8 nwHasSettings;
char nwSSID[32];
u8 nwAuthType;
u8 nwSecurity;
char nwPassphrase[64];
u8 nwDoConnect;
u8 nwState; // 0=no connection 1=connecting 2=connected

u32 nwLastConnAttempt;

u8 nwAnimPhase;
u32 nwLastAnimUpdate;

u8 nwSignalQuality;

u8 nwEnableDHCP;
u8 nwIPAddr[4];
u8 nwSubnetwork[4];
u8 nwGateway[4];

u32 pwLastUpdate;

static void NwUpdateIcon();
static void PwUpdateIcon();




// FPGA debug output

void send_binary(u8* data, int len)
{
#ifdef FPGA_LOG
    len &= 0x3FFF;
    u16 header = len;

    u8 buf[3];
    buf[0] = 0xF2;
    buf[1] = header >> 8;
    buf[2] = header & 0xFF;

    SPI_Start(SPI_DEVICE_FLASH, 0x8400);
    SPI_Write(buf, 3);
    SPI_Write(data, len);
    SPI_Finish();
#endif
}

void send_string(char* str)
{
#ifdef FPGA_LOG
    int len = strlen(str);

    len &= 0x3FFF;
    u16 header = 0x8000 | len;

    u8 buf[3];
    buf[0] = 0xF2;
    buf[1] = header >> 8;
    buf[2] = header & 0xFF;

    SPI_Start(SPI_DEVICE_FLASH, 0x8400);
    SPI_Write(buf, 3);
    SPI_Write((u8*)str, len);
    SPI_Finish();
#endif
}

void dump_data(u8* data, int len)
{
#ifdef FPGA_LOG
    len &= 0x3FFF;
    u16 header = 0x4000 | len;

    u8 buf[3];
    buf[0] = 0xF2;
    buf[1] = header >> 8;
    buf[2] = header & 0xFF;

    SPI_Start(SPI_DEVICE_FLASH, 0x8400);
    SPI_Write(buf, 3);
    SPI_Write(data, len);
    SPI_Finish();
#endif
}



void ExceptionHandler()
{
    send_string("EXCEPTION\n");
}



void flushcb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map)
{
    /*u8* dst = (u8*)0x380000;
    int dststride = 856;*/
    u16* dst = Framebuffer;
    int dststride = 854;

    //u32 t1 = REG_COUNTUP_VALUE;

    int i = 0;
    for (int y = area->y1; y <= area->y2; y++)
    {
        for (int x = area->x1; x <= area->x2; x++)
        {
            dst[(y*dststride)+x] = ((u16*)px_map)[i++];
        }
    }

    /*int linelen = area->x2 - area->x1 + 1;
    int srcstride = linelen * 2;
    int len = srcstride * (area->y2 - area->y1 + 1);
    DC_FlushRange(px_map, len);
    dst += (area->y1 * dststride) + area->x1;
    GPDMA_BlitTransfer(2, px_map, srcstride, dst, dststride*2, linelen*2, len);
    GPDMA_Wait(2);*/

    //u32 t2 = REG_COUNTUP_VALUE;
    //printf("flushcb: %d x %d, %d ns\n", area->x2-area->x1+1, area->y2-area->y1+1, t2-t1);

    lv_display_flush_ready(display);
}






sInputData* inputdata;
void readtouch(lv_indev_t* indev, lv_indev_data_t* data)
{
    sInputData* input = Input_GetData();
    if (input->TouchPressed)
    {
        data->point.x = input->TouchX;
        data->point.y = input->TouchY;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
        data->state = LV_INDEV_STATE_RELEASED;
}

void readkeypad(lv_indev_t* indev, lv_indev_data_t* data)
{
    //data->key = LV_KEY_DOWN;
    data->key = LV_KEY_NEXT;
    if (inputdata->ButtonsDown & BTN_DOWN)
        data->state = LV_INDEV_STATE_PRESSED;
    else
        data->state = LV_INDEV_STATE_RELEASED;
}


// FLASH
// 1MB slots
// 00: boot (taken)
// 01-04: see how far stock firmware goes (check both banks)
// 05-08: same
// 09-10: see how far language bank goes (check both banks)
// 11-18: same
// 19-1A: IR shit (taken)
// 1B: free
// 1C-1F: service firmware (taken but freeable)

// 0=free 1=taken 2=overwrite warning 3=service fw warning
// er
// 0=free 1=
u8 FlashTable[32];

void InitFlashTable()
{
    FlashTable[0x00] = 1;
    for (int i = 0x01; i < 0x19; i++)
        FlashTable[i] = 2;
    FlashTable[0x19] = 1;
    FlashTable[0x1A] = 1;
    FlashTable[0x1B] = 0;
    for (int i = 0x1C; i < 0x20; i++)
        FlashTable[i] = 3;

    u32 indxinfo[4];
    Flash_Read(0x100000, (u8*)indxinfo, 16);
    if (indxinfo[2] == 0x58444E49 && indxinfo[1] < 0x100)
    {
        //
    }
}




void LoadBinaryFromFlash(u32 addr)
{
    AudioAmp_DeInit();
    //UIC_SetBacklight(0);
    LCD_DeInit();
    //Wifi_DeInit();
    WUP_DelayUS(60);

    /*REG_DMA_CNT = 0;

    REG_SPI_CNT = 0;
    REG_SPI_SPEED = 0;*/

    //DisableIRQ();

    DC_FlushAll();
    IC_InvalidateAll();
    DisableMMU();

    u32 loaderaddr, loaderlen;
    Flash_GetEntryInfo("LDRf", &loaderaddr, &loaderlen, NULL);

    u8 excepvectors[64];
    Flash_Read(loaderaddr, excepvectors, 64);
    Flash_Read(loaderaddr + 64, (u8*)0x3F0000, loaderlen - 64);

    *(vu32*)0xF0001200 = 0xFFFF;
    *(vu32*)0xF0001204 = 0xFFFF;

    for (int i = 0; i < 64; i++)
        *(u8*)i = excepvectors[i];

    void* loadermain = (void*)(*(vu32*)0x20);
    ((void(*)(u32))loadermain)(addr);
}




void ScOpen(sScreen* sc, fnCloseCB callback)
{
    if (scStackLevel >= 8)
    {
        printf("scOpen(): STACK OVERFLOW!!\n");
        return;
    }

    if (scCurrent)
        scStack[scStackLevel++] = scCurrent;

    scCurrent = sc;
    sc->CloseCB = callback;
    sc->HasTopBar = 0;
    sc->Open();
    sc->Activate();
}

void ScDoCloseCurrent()
{
    scCloseFlag = 0;

    if (scStackLevel == 0)
    {
        printf("scCloseCurrent(): cannot close toplevel screen\n");
        return;
    }

    sScreen* prev = scCurrent;
    scCurrent = scStack[--scStackLevel];
    scCurrent->Activate();
    if (scCurrent->HasTopBar)
    {
        NwUpdateIcon();
        PwUpdateIcon();
    }
    prev->Close();

    if (prev->CloseCB)
        prev->CloseCB(scCloseRes, scCloseData);
    prev->CloseCB = NULL;
}

void ScCloseCurrent(int res, void* data)
{
    scCloseRes = res;
    scCloseData = data;
    scCloseFlag = 1;
}

lv_obj_t* ScAddTopbar(lv_obj_t* screen, const char* title)
{
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_style(screen, &scScreenStyle, 0);

    lv_obj_t* topbar = lv_obj_create(screen);
    lv_obj_set_size(topbar, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(topbar, &scTopbarStyle, 0);

    lv_obj_t* label = lv_label_create(topbar);
    lv_label_set_text(label, title);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* nwicon = lv_image_create(topbar);
    lv_obj_set_size(nwicon, 16, 16);
    lv_obj_align(nwicon, LV_ALIGN_RIGHT_MID, -28, 0);
    lv_image_set_src(nwicon, &wifi_icons);
    lv_image_set_inner_align(nwicon, LV_IMAGE_ALIGN_TOP_LEFT);
    scCurrent->TopBar[0] = nwicon;

    lv_obj_t* pwicon = lv_image_create(topbar);
    lv_obj_align(pwicon, LV_ALIGN_RIGHT_MID, 0, 0);
    scCurrent->TopBar[1] = pwicon;

    lv_obj_t* body = lv_obj_create(screen);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_add_style(body, &scBodyStyle, 0);

    scCurrent->HasTopBar = 1;
    NwUpdateIcon();
    PwUpdateIcon();

    return body;
}

lv_obj_t* ScAddButtonPane(lv_obj_t* screen)
{
    lv_obj_t* body = lv_obj_create(screen);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_height(body, LV_SIZE_CONTENT);
    lv_obj_add_style(body, &scBodyStyle, 0);

    return body;
}

static void OnMsgBoxClose(lv_event_t* event)
{
    lv_obj_t* msgbox = (lv_obj_t*)lv_event_get_user_data(event);
    fnMsgBoxCB callback = (fnMsgBoxCB)lv_obj_get_user_data(msgbox);
    lv_msgbox_close(msgbox);
    if (callback)
        callback();
}

lv_obj_t* ScMsgBox(const char* title, const char* msg, fnMsgBoxCB callback)
{
    lv_obj_t* msgbox = lv_msgbox_create(NULL);
    lv_obj_set_width(msgbox, 500);
    lv_obj_set_user_data(msgbox, callback);

    lv_msgbox_add_title(msgbox, title);

    lv_obj_t* content = lv_msgbox_get_content(msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, msg);

    lv_obj_t* btn;
    btn = lv_msgbox_add_footer_button(msgbox, "OK");
    lv_obj_add_event_cb(btn, OnMsgBoxClose, LV_EVENT_CLICKED, msgbox);

    lv_obj_t* footer = lv_msgbox_get_footer(msgbox);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    return msgbox;
}


void NwLoadSettings()
{
    u8 settings[128];
    UIC_ReadEEPROM(0x600, settings, 128);
    if (!CheckCRC16(settings, 126))
    {
        nwHasSettings = 0;
        return;
    }

    memcpy(nwSSID, &settings[0x00], 32);
    nwAuthType = settings[0x20];
    nwSecurity = settings[0x21];
    memcpy(nwPassphrase, &settings[0x22], 64);

    nwEnableDHCP = settings[0x62] == 0;
    memcpy(nwIPAddr, &settings[0x63], 4);
    memcpy(nwSubnetwork, &settings[0x67], 4);
    memcpy(nwGateway, &settings[0x6B], 4);

    nwHasSettings = 1;
}

static void NwUpdateIcon()
{
    if (!scCurrent->HasTopBar)
        return;

    switch (nwState)
    {
    case 0:
        lv_image_set_offset_x(scCurrent->TopBar[0], 0);
        lv_image_set_offset_y(scCurrent->TopBar[0], 0);
        break;

    case 1:
        lv_image_set_offset_x(scCurrent->TopBar[0], -16 * animoffset[nwAnimPhase]);
        lv_image_set_offset_y(scCurrent->TopBar[0], 0);
        break;

    case 2:
        lv_image_set_offset_x(scCurrent->TopBar[0], -16 * qualoffset[nwSignalQuality]);
        lv_image_set_offset_y(scCurrent->TopBar[0], -16);
        break;
    }
}

static void NwUpdateQuality()
{
    if (!Wifi_GetRSSI(NULL, &nwSignalQuality))
        nwSignalQuality = 0;

    NwUpdateIcon();
}

static void NwSetState(int state)
{
    nwState = state;

    nwLastAnimUpdate = WUP_GetTicks();
    nwAnimPhase = 0;
    NwUpdateIcon();
}

int NwGetState()
{
    return nwState;
}

static void NwJoinCallback(int status)
{
    if (status == WIFI_JOIN_SUCCESS)
    {
        NwSetState(2);
        NwUpdateQuality();
    }
    else
    {
        NwSetState(0);
        nwLastConnAttempt = WUP_GetTicks();
    }
}

void NwConnect()
{
    if (nwState != 0)
    {
        Wifi_Disconnect();
        NwSetState(0);
        WUP_DelayMS(1);
    }

    if (!nwHasSettings)
        return;

    if (nwEnableDHCP)
    {
        Wifi_SetIPAddr(NULL, NULL, NULL);
        Wifi_SetDHCPEnable(1);
    }
    else
    {
        Wifi_SetDHCPEnable(0);
        Wifi_SetIPAddr(nwIPAddr, nwSubnetwork, nwGateway);
    }

    if (!Wifi_JoinNetwork(nwSSID, nwAuthType, nwSecurity, nwPassphrase, NwJoinCallback))
        return;

    NwSetState(1);
}

void NwDisconnect()
{
    Wifi_Disconnect();
    NwSetState(0);
}

void NwSetAutoConnect(u8 conn)
{
    nwDoConnect = conn;
    if (conn && (nwState == 0))
        nwLastConnAttempt = WUP_GetTicks();
}

void NwUpdate()
{
    u32 now = WUP_GetTicks();

    if (nwDoConnect &&
        nwHasSettings &&
        (nwState == 0) &&
        ((now - nwLastConnAttempt) > 5000))
    {
        // if we failed to connect, try again
        NwConnect();
    }

    if ((nwState == 1) &&
        ((now - nwLastAnimUpdate) > 300))
    {
        // if connecting, update status icon animation
        nwLastAnimUpdate = now;
        nwAnimPhase++;
        if (nwAnimPhase >= 6)
            nwAnimPhase = 0;

        NwUpdateIcon();
    }

    if ((nwState == 2) &&
        ((now - nwLastAnimUpdate) > 5000))
    {
        // if connected, periodically update signal quality
        nwLastAnimUpdate = now;
        NwUpdateQuality();
    }
}


static void PwUpdateIcon()
{
    sInputData* input = Input_GetData();
    if (input->PowerStatus & 0xC1)
    {
        // connected to external power
        lv_image_set_src(scCurrent->TopBar[1], LV_SYMBOL_CHARGE);
    }
    else
    {
        u8 batt = UIC_GetBatteryLevel();
        switch (batt)
        {
        case 6: lv_image_set_src(scCurrent->TopBar[1], LV_SYMBOL_BATTERY_FULL); break;
        case 5:
        case 4: lv_image_set_src(scCurrent->TopBar[1], LV_SYMBOL_BATTERY_3); break;
        case 3:
        case 2: lv_image_set_src(scCurrent->TopBar[1], LV_SYMBOL_BATTERY_2); break;
        case 1: lv_image_set_src(scCurrent->TopBar[1], LV_SYMBOL_BATTERY_1); break;
        case 0: lv_image_set_src(scCurrent->TopBar[1], LV_SYMBOL_BATTERY_EMPTY); break;
        }
    }

    pwLastUpdate = WUP_GetTicks();
}

void PwUpdate()
{
    u32 now = WUP_GetTicks();
    if ((now - pwLastUpdate) > 1000)
    {
        PwUpdateIcon();
    }
}




void main()
{
    // set VMatch positions to fire every ~5.5ms
    int vmatchpos[3] = {8, 178, 348};
    Video_SetVMatchPositions(vmatchpos, 3);

    int fblen = 854 * 480 * sizeof(u16);
    Framebuffer = (u16*)memalign(16, fblen);
    memset(Framebuffer, 0, fblen);
    Video_SetOvlFramebuffer(Framebuffer, 0, 0, 854, 480, 854*2, PIXEL_FMT_RGB565);
    Video_SetOvlEnable(1);

    Video_SetDisplayEnable(1);

    nwState = 0;
    NwLoadSettings();

    lv_init();
    lv_tick_set_cb((lv_tick_get_cb_t)WUP_GetTicks);

    lv_display_t* disp = lv_display_create(854, 480);

    int lv_fblen = fblen / 10;
    u8* dispbuf1 = (u8*)memalign(16, lv_fblen);
    u8* dispbuf2 = NULL;//(u8*)memalign(16, lv_fblen);
    lv_display_set_buffers(disp, dispbuf1, dispbuf2, lv_fblen, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flushcb);

    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, readtouch);
    // lv_indev_set_user_data()

    /*lv_indev_t* keypad = lv_indev_create();
    lv_indev_set_type(keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keypad, readkeypad);*/

    lv_style_init(&scScreenStyle);
    lv_style_set_pad_row(&scScreenStyle, 0);

    lv_style_init(&scTopbarStyle);
    lv_style_set_border_side(&scTopbarStyle, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_margin_top(&scTopbarStyle, 0);
    lv_style_set_margin_left(&scTopbarStyle, 0);
    lv_style_set_margin_right(&scTopbarStyle, 0);
    lv_style_set_margin_bottom(&scTopbarStyle, 0);
    lv_style_set_radius(&scTopbarStyle, 0);
    lv_style_set_pad_all(&scTopbarStyle, 10);

    lv_style_init(&scBodyStyle);
    lv_style_set_border_side(&scBodyStyle, LV_BORDER_SIDE_NONE);
    lv_style_set_margin_top(&scBodyStyle, 0);
    lv_style_set_margin_left(&scBodyStyle, 0);
    lv_style_set_margin_right(&scBodyStyle, 0);
    lv_style_set_margin_bottom(&scBodyStyle, 0);
    lv_style_set_radius(&scBodyStyle, 0);
    lv_style_set_pad_all(&scBodyStyle, 10);

    scCurrent = NULL;
    memset(scStack, 0, sizeof(scStack));
    scStackLevel = 0;
    scCloseFlag = 0;
    scCloseRes = 0;
    scCloseData = NULL;

    ScOpen(&scBootMenu, NULL);
    nwDoConnect = 1;
    NwConnect();

	for (;;)
	{
		WUP_Update();

        scCurrent->Update();
        if (scCloseFlag)
            ScDoCloseCurrent();

        NwUpdate();
        PwUpdate();

        lv_timer_periodic_handler();
        Video_WaitForVMatch();
	}
}
