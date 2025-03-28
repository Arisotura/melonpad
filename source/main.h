#ifndef _MAIN_H_
#define _MAIN_H_

#include <wup/types.h>

typedef void (*fnScreenOpen)();
typedef void (*fnScreenClose)();
typedef void (*fnScreenActivate)();
typedef void (*fnScreenUpdate)();

typedef void (*fnCloseCB)(int res, void* data);

typedef void (*fnMsgBoxCB)();

typedef struct sScreen
{
    fnScreenOpen Open;
    fnScreenClose Close;
    fnScreenActivate Activate;
    fnScreenUpdate Update;

    fnCloseCB CloseCB;

    u8 HasTopBar;
    lv_obj_t* TopBar[2];

} sScreen;

void ScOpen(sScreen* sc, fnCloseCB callback);
void ScDoCloseCurrent();
void ScCloseCurrent(int res, void* data);
lv_obj_t* ScAddTopbar(lv_obj_t* screen, const char* title);
lv_obj_t* ScAddButtonPane(lv_obj_t* screen);
lv_obj_t* ScMsgBox(const char* title, const char* msg, fnMsgBoxCB callback);

void NwLoadSettings();
void NwConnect();
void NwDisconnect();
void NwSetAutoConnect(u8 conn);
int NwGetState();

#endif
