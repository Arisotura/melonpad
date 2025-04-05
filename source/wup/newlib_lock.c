#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <wup/wup.h>

// retarget locks for newlib
// based on https://gist.github.com/thomask77/3a2d54a482c294beec5d87730e163bdd

int _write(int file, char *ptr, int len);


struct __lock
{
    void* mutex;
};

struct __lock __lock___sinit_recursive_mutex;
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___at_quick_exit_mutex;
struct __lock __lock___malloc_recursive_mutex;
struct __lock __lock___env_recursive_mutex;
struct __lock __lock___tz_mutex;
struct __lock __lock___dd_hash_mutex;
struct __lock __lock___arc4random_mutex;


__attribute__((constructor))
static void init_retarget_locks(void)
{
    __lock___sinit_recursive_mutex.mutex  = Mutex_Create();
    __lock___sfp_recursive_mutex.mutex    = Mutex_Create();
    __lock___atexit_recursive_mutex.mutex = Mutex_Create();
    __lock___at_quick_exit_mutex.mutex    = Mutex_Create();
    //__lock___malloc_recursive_mutex.mutex = Mutex_Create();
    __lock___env_recursive_mutex.mutex    = Mutex_Create();
    __lock___tz_mutex.mutex               = Mutex_Create();
    __lock___dd_hash_mutex.mutex          = Mutex_Create();
    __lock___arc4random_mutex.mutex       = Mutex_Create();
}


// Special case for malloc/free. Without this, the full
// malloc_recursive_mutex would be used, which is much slower.

void __malloc_lock(struct _reent *r)
{
    Critical_Enter();
}


void __malloc_unlock(struct _reent *r)
{
    Critical_Leave();
}


void __retarget_lock_init(_LOCK_T *lock_ptr)
{
    *lock_ptr = (_LOCK_T)malloc(sizeof(struct __lock));
    (*lock_ptr)->mutex = Mutex_Create();
}


void __retarget_lock_init_recursive(_LOCK_T *lock_ptr)
{
    *lock_ptr = (_LOCK_T)malloc(sizeof(struct __lock));
    (*lock_ptr)->mutex = Mutex_Create();
}


void __retarget_lock_close(_LOCK_T lock)
{
    Mutex_Release(lock->mutex);
    free(lock);
}


void __retarget_lock_close_recursive(_LOCK_T lock)
{
    Mutex_Release(lock->mutex);
    free(lock);
}


void __retarget_lock_acquire(_LOCK_T lock)
{
    Mutex_Acquire(lock->mutex, NoTimeout);
}


void __retarget_lock_acquire_recursive(_LOCK_T lock)
{
    Mutex_Acquire(lock->mutex, NoTimeout);
}


int __retarget_lock_try_acquire(_LOCK_T lock)
{
    if (Mutex_Acquire(lock->mutex, 0) == 1)
        return 0;

    return -1;
}


int __retarget_lock_try_acquire_recursive(_LOCK_T lock)
{
    if (Mutex_Acquire(lock->mutex, 0) == 1)
        return 0;

    return -1;
}


void __retarget_lock_release(_LOCK_T lock)
{
    Mutex_Release(lock->mutex);
}


void __retarget_lock_release_recursive(_LOCK_T lock)
{
    Mutex_Release(lock->mutex);
}