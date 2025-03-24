#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>

#include "main.h"
#include "sc_boot_menu.h"
#include "sc_wifi_scan.h"


u16* Framebuffer;

sScreen* scCurrent;
sScreen* scStack[8];
u8 scStackLevel;
u8 scCloseFlag;

static lv_style_t scScreenStyle;
static lv_style_t scTopbarStyle;
static lv_style_t scBodyStyle;




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




void ScOpen(sScreen* sc)
{
    if (scStackLevel >= 8)
    {
        printf("scOpen(): STACK OVERFLOW!!\n");
        return;
    }

    if (scCurrent)
        scStack[scStackLevel++] = scCurrent;

    sc->Open();
    sc->Activate();
    scCurrent = sc;
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
    prev->Close();
}

void ScCloseCurrent()
{
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

    // TODO: add status icons and shit

    lv_obj_t* body = lv_obj_create(screen);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_add_style(body, &scBodyStyle, 0);

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




void main()
{
	u32 i;

    int fblen = 854 * 480 * sizeof(u16);
    Framebuffer = (u16*)memalign(16, fblen);
    memset(Framebuffer, 0, fblen);
    Video_SetOvlFramebuffer(Framebuffer, 0, 0, 854, 480, 854*2, PIXEL_FMT_RGB565);
    Video_SetOvlEnable(1);

    Video_SetDisplayEnable(1);

	//u8 buf[16];

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

    //lv_example_button_1();
    //lv_example_button_2();
    //lv_example_button_3();
    //ScWifiScan_Init();

    scCurrent = NULL;
    memset(scStack, 0, sizeof(scStack));
    scStackLevel = 0;
    scCloseFlag = 0;

    ScOpen(&scBootMenu);

	for (;;)
	{
		WUP_Update();

        scCurrent->Update();
        if (scCloseFlag)
            ScDoCloseCurrent();

        lv_timer_periodic_handler();
	}
}



