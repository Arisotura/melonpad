#ifndef _MAIN_H_
#define _MAIN_H_

#include <wup/types.h>

typedef void (*fnScreenOpen)();
typedef void (*fnScreenClose)();
typedef void (*fnScreenActivate)();
typedef void (*fnScreenUpdate)();

typedef struct sScreen
{
    fnScreenOpen Open;
    fnScreenClose Close;
    fnScreenActivate Activate;
    fnScreenUpdate Update;

} sScreen;

lv_obj_t* ScAddTopbar(lv_obj_t* screen, const char* title);

#endif
