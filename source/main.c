#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <wup/wup.h>
#include <lvgl/lvgl.h>


u32 GetCP15Reg(int cn, int cm, int cp);


void rumble()
{
	*(vu32*)0xF0005114 |= 0x0100;
    WUP_DelayMS(250);
	*(vu32*)0xF0005114 &= ~0x0100;
    WUP_DelayMS(250);
}


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

    SPI_Start(SPI_DEVICE_FLASH, 0x8400);//SPI_SPEED_FLASH);
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

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
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

    SPI_Start(SPI_DEVICE_FLASH, 0x8400);//SPI_SPEED_FLASH);
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
    u16* dst = (u16*)0x300000;
    int dststride = 854;

    int i = 0;
    for (int y = area->y1; y <= area->y2; y++)
    {
        for (int x = area->x1; x <= area->x2; x++)
        {
            dst[(y*dststride)+x] = ((u16*)px_map)[i++];
        }
    }

    lv_display_flush_ready(display);
}



static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        LV_LOG_USER("Clicked");
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        LV_LOG_USER("Toggled");
    }
}

void lv_example_button_1(void)
{
    /*lv_obj_t * label;

    lv_obj_t * btn1 = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
    lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);

    label = lv_label_create(btn1);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);

    lv_obj_t * btn2 = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(btn2, LV_SIZE_CONTENT);

    label = lv_label_create(btn2);
    lv_label_set_text(label, "Toggle");
    lv_obj_center(label);*/


    //lv_obj_t* topbar = lv_obj_create(lv_layer_top());
    lv_obj_t* topbar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(topbar, lv_pct(100), 40);//LV_SIZE_CONTENT);
    lv_obj_t* test = lv_label_create(topbar);
    lv_label_set_text(test, "melonpad v0.1");
    lv_obj_align(test, LV_ALIGN_LEFT_MID, 0, 0);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_border_side(&style, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_margin_top(&style, 0);
    lv_style_set_margin_left(&style, 0);
    lv_style_set_margin_right(&style, 0);
    lv_style_set_radius(&style, 0);
    lv_style_set_pad_top(&style, 0);
    lv_style_set_pad_bottom(&style, 0);
    lv_style_set_pad_left(&style, 8);
    lv_style_set_pad_right(&style, 8);
    lv_obj_add_style(topbar, &style, 0);


    lv_obj_t* list1 = lv_list_create(lv_screen_active());
    lv_obj_set_size(list1, lv_pct(70), 480-40);
    lv_obj_align(list1, LV_ALIGN_TOP_LEFT, 0, 40);

    lv_obj_t * btn;
    lv_list_add_text(list1, "Boot menu");
    btn = lv_list_add_button(list1, NULL, "Stock firmware");
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "lucario.fw");
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "Checkerboard");
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "Ass vibrator");
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(list1, NULL, "5 is red");
    //lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);


    lv_obj_t* list2 = lv_list_create(lv_screen_active());
    lv_obj_set_size(list2, lv_pct(30), 480-40);
    lv_obj_align(list2, LV_ALIGN_TOP_RIGHT, 0, 40);
    
    lv_list_add_text(list2, "Options");
    btn = lv_list_add_button(list2, LV_SYMBOL_SETTINGS, "Boot options");
    btn = lv_list_add_button(list2, LV_SYMBOL_HOME, "Hardware info");
    btn = lv_list_add_button(list2, LV_SYMBOL_POWER, "Power to the people");
    lv_list_add_text(list2, "Connectivity");
    btn = lv_list_add_button(list2, LV_SYMBOL_WIFI, "Connect to wifi");
    btn = lv_list_add_button(list2, LV_SYMBOL_BLUETOOTH, "Connect to IR");
}

void lv_example_button_2(void)
{
    /*Init the style for the default state*/
    static lv_style_t style;
    lv_style_init(&style);

    lv_style_set_radius(&style, 3);

    lv_style_set_bg_opa(&style, LV_OPA_100);
    lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_dir(&style, LV_GRAD_DIR_VER);

    lv_style_set_border_opa(&style, LV_OPA_40);
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_GREY));

    lv_style_set_shadow_width(&style, 8);
    lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_shadow_offset_y(&style, 8);

    lv_style_set_outline_opa(&style, LV_OPA_COVER);
    lv_style_set_outline_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_pad_all(&style, 10);

    /*Init the pressed style*/
    static lv_style_t style_pr;
    lv_style_init(&style_pr);

    /*Add a large outline when pressed*/
    lv_style_set_outline_width(&style_pr, 30);
    lv_style_set_outline_opa(&style_pr, LV_OPA_TRANSP);

    lv_style_set_translate_y(&style_pr, 5);
    lv_style_set_shadow_offset_y(&style_pr, 3);
    lv_style_set_bg_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_bg_grad_color(&style_pr, lv_palette_darken(LV_PALETTE_BLUE, 4));

    /*Add a transition to the outline*/
    static lv_style_transition_dsc_t trans;
    static lv_style_prop_t props[] = {LV_STYLE_OUTLINE_WIDTH, LV_STYLE_OUTLINE_OPA, 0};
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_linear, 300, 0, NULL);

    lv_style_set_transition(&style_pr, &trans);

    lv_obj_t * btn1 = lv_button_create(lv_screen_active());
    lv_obj_remove_style_all(btn1);                          /*Remove the style coming from the theme*/
    lv_obj_add_style(btn1, &style, 0);
    lv_obj_add_style(btn1, &style_pr, LV_STATE_PRESSED);
    lv_obj_set_size(btn1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(btn1);

    lv_obj_t * label = lv_label_create(btn1);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);
}

void lv_example_button_3(void)
{
    /*Properties to transition*/
    static lv_style_prop_t props[] = {
            LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT, LV_STYLE_TEXT_LETTER_SPACE, 0
    };

    /*Transition descriptor when going back to the default state.
     *Add some delay to be sure the press transition is visible even if the press was very short*/
    static lv_style_transition_dsc_t transition_dsc_def;
    lv_style_transition_dsc_init(&transition_dsc_def, props, lv_anim_path_overshoot, 250, 100, NULL);

    /*Transition descriptor when going to pressed state.
     *No delay, go to presses state immediately*/
    static lv_style_transition_dsc_t transition_dsc_pr;
    lv_style_transition_dsc_init(&transition_dsc_pr, props, lv_anim_path_ease_in_out, 250, 0, NULL);

    /*Add only the new transition to he default state*/
    static lv_style_t style_def;
    lv_style_init(&style_def);
    lv_style_set_transition(&style_def, &transition_dsc_def);

    /*Add the transition and some transformation to the presses state.*/
    static lv_style_t style_pr;
    lv_style_init(&style_pr);
    lv_style_set_transform_width(&style_pr, 10);
    lv_style_set_transform_height(&style_pr, -10);
    lv_style_set_text_letter_space(&style_pr, 10);
    lv_style_set_transition(&style_pr, &transition_dsc_pr);

    lv_obj_t * btn1 = lv_button_create(lv_screen_active());
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -80);
    lv_obj_add_style(btn1, &style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn1, &style_def, 0);

    lv_obj_t * label = lv_label_create(btn1);
    lv_label_set_text(label, "Gum");
}

sInputData* inputdata;
void readtouch(lv_indev_t * indev, lv_indev_data_t * data)
{
    if (inputdata->TouchPressed)
    {
        data->point.x = inputdata->TouchX;
        data->point.y = inputdata->TouchY;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
        data->state = LV_INDEV_STATE_RELEASED;
}


volatile u8 vblendflag;
void vbl_end(int irq, void* userdata)
{
    vblendflag = 1;
}


void clk_testbench()
{
    *(vu32*)0xF0004424 = 0;

    // bit9=CS  bit8=device  bit1=direction
    *(vu32*)0xF0004404 = (*(vu32*)0xF0004404 & ~0x702) | 0x100;

    // 15=0 1=0: clk out=in
    // 15=0 1=1: clk out=in
    // 15=1 1=0: clk out=~in
    // 15=1 1=1: clk out=in
    //*(vu32*)0xF0004414 &= ~(1<<4);
    //*(vu32*)0xF0004414 &= ~(1<<1);

    // 857? -> 43MHz
    *(vu32*)0xF0004400 = 0x8000 | 4 | (19<<3);
    //*(vu32*)0xF0004400 = 0x8000 | 3;
    //*(vu32*)0xF0004400 = 0x8000;

    for (;;)
        *(vu32*)0xF0004410 = 0x00;
}

void shit_testbench()
{
    for (int i = 0; i < 20; i++)
    {
        printf("round %d:\n", i);

        u32 zerp[5];
        Flash_Read(0x66C38C, (u8*)&zerp[0], 4);
        Flash_Read(0x66C38D, (u8*)&zerp[1], 4);
        Flash_Read(0x66C38E, (u8*)&zerp[2], 4);
        Flash_Read(0x66C38F, (u8*)&zerp[3], 4);
        printf("zerp: %08X %08X %08X %08X\n", zerp[0], zerp[1], zerp[2], zerp[3]);

        /*Flash_Read(0x66C384, (u8*)&zerp[0], 4);
        Flash_Read(0x66C385, (u8*)&zerp[1], 4);
        Flash_Read(0x66C386, (u8*)&zerp[2], 4);
        Flash_Read(0x66C387, (u8*)&zerp[3], 4);
        printf("zerp: %08X %08X %08X %08X\n", zerp[0], zerp[1], zerp[2], zerp[3]);

        Flash_Read(0x66C37C, (u8*)&zerp[0], 4);
        Flash_Read(0x66C37D, (u8*)&zerp[1], 4);
        Flash_Read(0x66C37E, (u8*)&zerp[2], 4);
        Flash_Read(0x66C37F, (u8*)&zerp[3], 4);
        printf("zerp: %08X %08X %08X %08X\n", zerp[0], zerp[1], zerp[2], zerp[3]);*/

        Flash_Read(0x66C37F, (u8*)&zerp[0], 20);
        printf("%08X\n", zerp[4]);

        /*u32 imglen;
        Flash_Read(0x66C38F, (u8 * ) & imglen, 4);
        printf("IMG_ len: %08X\n", imglen);*/

        /*u8 *img = (u8 *) malloc(imglen * 0x18);
        Flash_Read(0x66C38F + 4, img, imglen * 0x18);
        printf("offset for entry 31: %08X\n", *(u32 * ) & img[0x18 * 31 + 4]);

        free(img);*/

        WUP_DelayMS(500);
    }
}

void flash_testbench()
{
    u8* derp = (u8*)malloc(200);
    /*char str[200];
    sprintf(str, "sprintf test: %08X\n", *(vu32*)0xF0000000);
    send_string(str);*/

    printf("printf test: %08X\n", *(vu32*)0xF0000000);

    u8* test = (u8*)malloc(0x1008);
    Flash_Read(0, test, 0x1008);
    printf("TEST BEFORE: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd[5] = {0x20, 0x00, 0x00, 0x02, 0x34};
    SPI_Write(cmd, 5);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("subsec erased\n");

    Flash_Read(0, test, 0x1008);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    for (int i = 0; i < 0x100; i++)
        test[i] = 0x40+i;
    for (int i = 0x100; i < 0x200; i++)
        test[i] = 0x77;

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd2[5] = {0x02, 0x00, 0x00, 0x00, 0x01};
    SPI_Write(cmd2, 5);
    SPI_Write(test, 0x101);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("page programmed\n");

    Flash_Read(0, test, 0x1008);
    //printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xF8], *(u32*)&test[0xFC], *(u32*)&test[0x100], *(u32*)&test[0x104]);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0x48], *(u32*)&test[0x4C], *(u32*)&test[0x78], *(u32*)&test[0x7C], *(u32*)&test[0x100], *(u32*)&test[0x104]);

    printf("now testing elsewhere\n");
    Flash_Read(0x100000, test, 0x1008);
    printf("TEST BEFORE: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd1[5] = {0x20, 0x00, 0x10, 0x05, 0x67};
    SPI_Write(cmd1, 5);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("subsec erased\n");

    Flash_Read(0x100000, test, 0x1008);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    for (int i = 0; i < 0x100; i++)
        test[i] = 0x40+i;
    for (int i = 0x100; i < 0x200; i++)
        test[i] = 0x77;

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    //u8 cmd4[5] = {0x02, 0x00, 0x00, 0x00, 0x00};
    u8 cmd4[5] = {0x02, 0x00, 0x10, 0x00, 0x00};
    SPI_Write(cmd4, 5);
    SPI_Write(test, 0x100);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("page programmed\n");

    //Flash_Read(0, test, 0x1008);
    Flash_Read(0x100000, test, 0x1008);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xF8], *(u32*)&test[0xFC], *(u32*)&test[0x100], *(u32*)&test[0x104]);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0x48], *(u32*)&test[0x4C], *(u32*)&test[0x78], *(u32*)&test[0x7C], *(u32*)&test[0x100], *(u32*)&test[0x104]);


    for (int i = 0; i < 20; i++)
    {
        *(u32*)&test[0] = (i & 0xF) * 0x11111111;

        Flash_WaitForStatus(0x03, 0x00);
        Flash_WriteEnable();

        SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
        u8 cmd4[5] = {0x02, 0x00, 0x10, 0x05, 0x00};
        SPI_Write(cmd4, 5);
        SPI_Write(test, 0x4);
        SPI_Finish();

        Flash_WaitForStatus(0x03, 0x00);

        Flash_Read(0x100500, test, 4);
        printf("%d. res=%08X\n", i, *(u32*)&test[0]);
    }
}

u32 flash_addr = 0x100000;
void flash_testbench2()
{
    printf("TESTING AT %08X\n", flash_addr);

    u8* test = (u8*)malloc(0x1008);
    Flash_Read(flash_addr, test, 0x1008);
    printf("TEST BEFORE: %08X %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[8], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd[5] = {0x20, 0x00, 0x00, 0x00, 0x00};
    cmd[1] = flash_addr >> 24;
    cmd[2] = flash_addr >> 16;
    cmd[3] = flash_addr >> 8;
    cmd[4] = flash_addr;
    SPI_Write(cmd, 5);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("subsec erased\n");

    Flash_Read(flash_addr, test, 0x1008);
    printf("TEST AFTER: %08X %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[8], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);

    for (int i = 0; i < 0x100; i++)
        test[i] = i;//0x40+i;
    for (int i = 0x100; i < 0x200; i++)
        test[i] = 0x77;

    Flash_WaitForStatus(0x03, 0x00);
    Flash_WriteEnable();

    SPI_Start(SPI_DEVICE_FLASH, SPI_SPEED_FLASH);
    u8 cmd2[5] = {0x02, 0x00, 0x00, 0x00, 0x00};
    u32 flash_addr_2 = flash_addr + 8;
    cmd2[1] = flash_addr_2 >> 24;
    cmd2[2] = flash_addr_2 >> 16;
    cmd2[3] = flash_addr_2 >> 8;
    cmd2[4] = flash_addr_2;
    SPI_Write(cmd2, 5);
    //SPI_Write(test, 0x9);
    SPI_Write(test, 0x102);
    SPI_Finish();

    Flash_WaitForStatus(0x03, 0x00);
    printf("page programmed\n");

    Flash_Read(flash_addr, test, 0x1008);
    //printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[0xFF8], *(u32*)&test[0xFFC], *(u32*)&test[0x1000], *(u32*)&test[0x1004]);
    printf("TEST AFTER: %08X %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0], *(u32*)&test[4], *(u32*)&test[8], *(u32*)&test[0xF8], *(u32*)&test[0xFC], *(u32*)&test[0x100], *(u32*)&test[0x104]);
    printf("TEST AFTER: %08X %08X, %08X %08X, %08X %08X\n", *(u32*)&test[0x48], *(u32*)&test[0x4C], *(u32*)&test[0x78], *(u32*)&test[0x7C], *(u32*)&test[0x100], *(u32*)&test[0x104]);

    free(test);
    flash_addr += 0x1000;
}



void LoadBinaryFromFlash(u32 addr)
{
    AudioAmp_DeInit();
    //UIC_SetBacklight(0);
    LCD_SetBrightness(-1);
    *(vu32*)0xF0005100 = 0xC200;
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


void scantest(sScanInfo* list)
{
    printf("RECEIVED SCAN INFO\n");

    sScanInfo* cur = list;
    while (cur)
    {
        printf("RESULT: %s, CHAN=%d, SEC=%d, RSSI=%d QUAL=%d\n", cur->SSID, cur->Channel, cur->Security, cur->RSSI, cur->SignalQuality);

        cur = cur->Next;
    }
}


void uictest();
extern u32 irqlog[16];
void main()
{
	u32 i;
	
	// setup GPIO
	*(vu32*)0xF0005114 = 0xC200;

    // clear framebuffer (TODO more nicely)
    {
        u16* fb = (u16*)0x300000;
        for (i = 0; i < 854*480; i++)
            fb[i] = 0;
    }

    vblendflag = 0;
    WUP_SetIRQHandler(0x15, vbl_end, NULL, 0);


	u8 buf[16];

    lv_init();
    lv_tick_set_cb((lv_tick_get_cb_t)WUP_GetTicks);

    lv_display_t* disp = lv_display_create(854, 480);

    u8* dispbuf1 = (u8*)memalign(16, 854*480/10);
    u8* dispbuf2 = NULL;//(u8*)memalign(16, 854*480/10);
    lv_display_set_buffers(disp, dispbuf1, dispbuf2, 854*480/10, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flushcb);

    u32 palette[256];
    for (int i = 0; i < 256; i++)
        palette[i] = 0xFF000000 | (i * 0x010101);
    GFX_SetPalette(0, palette, 256);

    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, readtouch);

    lv_example_button_1();
    //lv_example_button_2();
    //lv_example_button_3();

    //Console_OpenDefault();


    printf("hardware: %08X\n", *(vu32*)0xF0000000);
    printf("SD caps1: %08X\n", *(vu32*)0xE0010040);
    printf("SD caps2: %08X\n", *(vu32*)0xE0010044);
    printf("SD version: %04X\n", *(vu16*)0xE00100FE);
    printf("UIC: %02X, %d\n", *(vu8*)0x3FFFFC, UIC_GetState());



    //printf("derp=%02X %02X\n", derp[0], derp[1]);




    for (;;)
    {
        u8 fark;
        UIC_SendCommand(0x13, NULL, 0, &fark, 1);
        WUP_DelayMS(5);
        if (fark != 1) continue;
        printf("UIC13=%02X\n", fark);
        break;
    }
    printf("UIC came up\n");

#if 1
    // 69EF30B0
    // 01101001 11101111 00110000 10110000
    // TODO move this to WUP_Init()
    int a = SDIO_Init();
    //uictest();
    int b = Wifi_Init();
    printf("wifi init res=%d/%d\n", a, b);
    //uictest();

    /*WUP_DelayMS(2000);
    //for (;;)
    {
        //Wifi_StartScan(1);
        //Wifi_StartScan2(1);
        //WUP_DelayMS(10);
        WUP_DelayMS(1000);
    }
    //Wifi_StartScan(1);
    Wifi_JoinNetwork();
    //Wifi_JoinNetwork2();
    printf("joined network, maybe\n");*/

    printf("press A to try joining\n");
    printf("press B to scan for networks\n");
    printf("press X to send test packet\n");
    printf("press UP to change WSEC, DOWN to change WPAAUTH\n");
#endif

    //clk_testbench();
    //shit_testbench();
    //flash_testbench();
    //flash_testbench2();


    int frame = 0;
    int d = 0;
    u32 regaddr = 0xF0005000;
    u16 oldval = 0;
    u8 poop = 0;

    u32 wsecvals[] = {2, 4, 6, -1};
    u32 wpaauthvals[] = {4, 0x80, -1};
    int wsecid = 0;
    int wpaauthid = 0;

    u32 lastvbl = 0;
    int derp = 4;
	for (;;)
	{
		frame++;
		if (frame >= 15) frame = 0;

        sInputData* test = Input_Scan();
        inputdata = test;

        // TODO: integrate this elsewhere
        //Audio_SetVolume(test->AudioVolume);
        //Audio_SetOutput((test->PowerStatus & (1<<5)) ? 1:0);

        /*if (test->ButtonsPressed & BTN_X)
        {
            printf("Pressed X\n");
            UIC_SetBacklight(0);
            WUP_DelayMS(10);
            UIC_SendCommand(0x14, NULL, 0, NULL, 0);
        }*/

        /*if (test->ButtonsDown & BTN_A)
            printf("Pressed A! frame=%d\n", frame);
        if (test->ButtonsPressed & BTN_B)
            printf("Pressed B! frame=%d\n", frame);
        if (test->ButtonsDown & BTN_UP)
            printf("Pressed UP! frame=%d,\nthis is a multi-line printf\nI like it\n", frame);
        if (test->ButtonsDown & BTN_DOWN)
            printf("Pressed DOWN! this is going to be a really long line, the point is to test how far the console thing can go and whether it handles this correctly\n");
*/

        /*if (test->ButtonsPressed & BTN_A)
        {
            printf("attempting to join network\n");
            Wifi_JoinNetwork(wsecvals[wsecid], wpaauthvals[wpaauthid]);
        }

        if (test->ButtonsPressed & BTN_B)
        {
            printf("scanning for networks\n");
            Wifi_StartScan2(1);
        }

        if (test->ButtonsPressed & BTN_X)
        {
            printf("sending test packet\n");
            Wifi_Test();
        }*/

        if (test->ButtonsPressed & BTN_B)
        {
            //printf("lucario?\n");
            //LoadBinaryFromFlash(0x500000);
            void Wifi_JoinNetwork(u32, u32);
            Wifi_JoinNetwork(6, 0x84);
        }

        if (test->ButtonsPressed & BTN_A)
        {
            Wifi_StartScan(scantest);
            //printf("rebooting?\n");
            //LoadBinaryFromFlash(0x100000);
            //flash_testbench2();
#if 0
            AudioAmp_DeInit();
            //UIC_SetBacklight(0);
            LCD_SetBrightness(-1);
            *(vu32*)0xF0005100 = 0xC200;
            //Wifi_DeInit();
            WUP_DelayUS(60);

            /*REG_DMA_CNT = 0;

            REG_SPI_CNT = 0;
            REG_SPI_SPEED = 0;*/

            // trigger soft reset
            //DisableIRQ();
            *(vu32*)0xF0001200 = 0xFFFF;
            *(vu32*)0xF0001204 = 0xFFFF;

            *(vu32*)0xF0004400 = 0;
            *(vu32*)0xF0004424 = 0;

            *(vu32*)0xF00050EC = 0x8000;    // clock
            *(vu32*)0xF00050F0 = 0x0000;    // MISO
            *(vu32*)0xF00050F4 = 0x8000;    // MOSI
            *(vu32*)0xF00050F8 = 0x8000;    // FLASH CS
            *(vu32*)0xF00050FC = 0x8000;    // UIC CS

            //u32 zzn = *(vu32*)0xF0000030;
            //*(vu32*)0xF0000030 &= ~0x300;
            //printf("zzn=%08X/%08X\n", zzn, *(vu32*)0xF0000030);
            //for(;;);

            *(vu32*)0xF0000058 = 0xFFFFFFFB;
            //*(vu32*)0xF0000058 = 0x3;
            *(vu32*)0xF0000058 = 0;
            *(vu32*)0xF0000004 = 0;
            *(vu32*)0xF0000004 = 1;
            for (;;);
#endif
        }

        if (test->ButtonsPressed & BTN_UP)
        {
            /*wsecid++;
            if (wsecvals[wsecid] == -1)
                wsecid = 0;
            printf("WSEC=%02X WPAAUTH=%02X\n", wsecvals[wsecid], wpaauthvals[wpaauthid]);*/
            void LCD_test(int);
            printf("der=%d\n", derp);
            LCD_test(derp);
            derp++;
        }

        if (test->ButtonsPressed & BTN_DOWN)
        {
            wpaauthid++;
            if (wpaauthvals[wpaauthid] == -1)
                wpaauthid = 0;
            printf("WSEC=%02X WPAAUTH=%02X\n", wsecvals[wsecid], wpaauthvals[wpaauthid]);
        }

        /**(vu32*)regaddr ^= 0x0100;
        if (test->ButtonsPressed & BTN_LEFT)
        {
            regaddr += 4;
            *(vu32*)regaddr = 0xC200;
            printf("toggling register %08X\n", regaddr);
        }*/
        /*if (test->ButtonsPressed & BTN_LEFT)
        {
            d++;
            d &= 31;
            printf("reset bit: %d\n", d);
        }*/
        // X offset: min=2 max=C2 mask=7FF
        // Y offset: min=7 max=1E7?? mask=7FF
        // F0009404
        // low / vbl
        // 0x1FE = 940
        // 0x208 = 1266
        // 0x212 = 1593
        // max = 7FF
        // step ~= 32.6ns
        // high: max = 1FF
        // value less than 0x100 makes vblank a bit longer? (36ns)
        // 533970 pixels in ~16638ns
        // 1 pixel in X ns
#define reg 0xF000950C
#define step 10
        if (test->ButtonsPressed & BTN_LEFT)
        {
            (*(vu32*)reg) -= step;
            printf("reg=%08X\n", *(vu32*)reg);
        }
        if (test->ButtonsPressed & BTN_RIGHT)
        {
            (*(vu32*)reg) += step;
            printf("reg=%08X\n", *(vu32*)reg);
        }
        // aaagdsrdfdddd
        if (test->ButtonsPressed & (BTN_Y | BTN_SYNC))
        {
            //resettest(d);
            AudioAmp_DeInit();
            //UIC_SetBacklight(0);
            LCD_SetBrightness(-1);
            *(vu32*)0xF0005100 = 0xC200;
            Wifi_DeInit();
            WUP_DelayUS(60);

            u8 zarmf = 0;
            UIC_SendCommand(0x17, &zarmf, 1, NULL, 0);
            WUP_DelayUS(60);

            u8 zirmf[4] = {1, 0, 0, 0};
            UIC_SendCommand(0x1B, zirmf, 4, NULL, 0);
            WUP_DelayUS(60);

            //UIC_SetState(9);
            UIC_SetState(5);
            //UIC_SendCommand(0x16, NULL, 0, NULL, 0);
            /*WUP_DelayUS(60);
            u8 fark = 44;
            UIC_SendCommand(0x13, NULL, 0, &fark, 1);
            printf("fark=%d\n", fark);*/
            //UIC_SetBacklight(1);
            *(vu32*)0xF0000410 = 0;
            *(vu32*)0xF0000420 = 0;
            DisableIRQ();
            for (;;);
        }

        Wifi_Update();

        GFX_WaitForVBlank();
        /*u32 vbl = REG_COUNTUP_VALUE;

        vblendflag = 0;
        while (!vblendflag) WaitForIRQ();
        u32 vbl2 = REG_COUNTUP_VALUE;

        u32 vbltime = vbl2 - vbl;
        u32 frametime = vbl - lastvbl;
        lastvbl = vbl2;
        printf("frame: reg=%08X disp=%010d blk=%010d tot=%010d\n", *(vu32*)reg, frametime, vbltime, frametime+vbltime);*/

        // 5E8000 = E0000 * 108 / 16

		/*GFX_WaitForVBlank();
        u32 derp = lv_task_handler();
        printf("%d\n", derp);*/
        lv_timer_periodic_handler();
        //u32 next = lv_timer_handler();
        //if (next) WUP_DelayMS(next);
	}
}



