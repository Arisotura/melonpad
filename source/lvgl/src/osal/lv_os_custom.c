/**
 * @file lv_os_none.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_os.h"

#if LV_USE_OS == LV_OS_CUSTOM
#include "../misc/lv_types.h"
#include "../misc/lv_assert.h"
#include <wup/wup.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_result_t lv_thread_init(lv_thread_t * thread, lv_thread_prio_t prio, void (*callback)(void *), size_t stack_size,
                           void * user_data)
{
    int _prio;
    switch (prio)
    {
    case LV_THREAD_PRIO_HIGHEST: _prio = 0; break;
    case LV_THREAD_PRIO_HIGH:    _prio = 4; break;
    case LV_THREAD_PRIO_MID:     _prio = 8; break;
    case LV_THREAD_PRIO_LOW:     _prio = 12; break;
    case LV_THREAD_PRIO_LOWEST:  _prio = 15; break;
    default: return LV_RESULT_INVALID;
    }

    *thread = Thread_Create(callback, user_data, stack_size, _prio, "lvgl");
    if (!*thread)
        return LV_RESULT_INVALID;

    return LV_RESULT_OK;
}

lv_result_t lv_thread_delete(lv_thread_t * thread)
{
    Thread_Delete(*thread);
    return LV_RESULT_OK;
}


lv_result_t lv_mutex_init(lv_mutex_t * mutex)
{
    *mutex = Mutex_Create();
    if (!*mutex)
        return LV_RESULT_INVALID;

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock(lv_mutex_t * mutex)
{
    if (!Mutex_Acquire(*mutex, NoTimeout))
        return LV_RESULT_INVALID;

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock_isr(lv_mutex_t * mutex)
{
    return lv_mutex_lock(mutex);
}

lv_result_t lv_mutex_unlock(lv_mutex_t * mutex)
{
    if (!Mutex_Release(*mutex))
        return LV_RESULT_INVALID;

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_delete(lv_mutex_t * mutex)
{
    Mutex_Delete(*mutex);
    return LV_RESULT_OK;
}


lv_result_t lv_thread_sync_init(lv_thread_sync_t * sync)
{
    *sync = EventMask_Create();
    if (!*sync)
        return LV_RESULT_INVALID;

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_wait(lv_thread_sync_t * sync)
{
    if (!EventMask_Wait(*sync, 1, NoTimeout, NULL))
        return LV_RESULT_INVALID;

    EventMask_Clear(*sync, 1);
    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_signal(lv_thread_sync_t * sync)
{
    if (!EventMask_Signal(*sync, 1))
        return LV_RESULT_INVALID;

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_signal_isr(lv_thread_sync_t * sync)
{
    return lv_thread_sync_signal(sync);
}

lv_result_t lv_thread_sync_delete(lv_thread_sync_t * sync)
{
    EventMask_Delete(*sync);
    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_OS == LV_OS_NONE*/
