#ifndef _INPUT_H_
#define _INPUT_H_

#define BTN_DOWN        (1<<0)
#define BTN_UP          (1<<1)
#define BTN_RIGHT       (1<<2)
#define BTN_LEFT        (1<<3)
#define BTN_Y           (1<<4)
#define BTN_X           (1<<5)
#define BTN_B           (1<<6)
#define BTN_A           (1<<7)
#define BTN_SYNC        (1<<8)
#define BTN_HOME        (1<<9)
#define BTN_MINUS       (1<<10)
#define BTN_PLUS        (1<<11)
#define BTN_R           (1<<12)
#define BTN_L           (1<<13)
#define BTN_ZR          (1<<14)
#define BTN_ZL          (1<<15)
#define BTN_TV          (1<<21)
#define BTN_R3          (1<<22)
#define BTN_L3          (1<<23)

#define PWR_AC_CONNECTED    (1<<0)
#define PWR_BTN_PRESSED     (1<<1)
#define PWR_CHARGING        (1<<6)

typedef struct
{
    u32 ButtonsDown;
    u32 ButtonsPressed;
    u32 ButtonsReleased;

    u8 PowerStatus;
    u8 BatteryLevel;

    u16 LeftStickX;
    u16 LeftStickY;
    u16 RightStickX;
    u16 RightStickY;

    u8 AudioVolume;

    s16 AccelX;
    s16 AccelY;
    s16 AccelZ;

    s32 GyroRoll;
    s32 GyroYaw;
    s32 GyroPitch;

    u8 MagnetData[6];

    u16 TouchX;
    u16 TouchY;
    u8 TouchPressed;
    u16 TouchPressure;

} sInputData;

void Input_Init();

sInputData* Input_Scan();

#endif // _INPUT_H_
