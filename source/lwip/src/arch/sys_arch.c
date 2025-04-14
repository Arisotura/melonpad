#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include <wup/wup.h>


void* _lwip_tcpip_thread;


void sys_init(void)
{
    _lwip_tcpip_thread = NULL;
}


void sys_mark_tcpip_thread()
{
    _lwip_tcpip_thread = Thread_Current();
}

void sys_assert_core_locked()
{
    if (_lwip_tcpip_thread == NULL) return;
    LWIP_ASSERT("LWIP CALLED FROM WRONG THREAD!!", (Thread_Current() == _lwip_tcpip_thread));
}


u32_t sys_now()
{
    return WUP_GetTicks();
}

void sys_msleep(u32_t ms)
{
    Thread_Sleep(ms);
}


sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
    void* _thread = Thread_Create(thread, arg, stacksize, prio, name);
    LWIP_ASSERT("THREAD IS NULL!!", (_thread != NULL));
    return _thread;
}


sys_prot_t sys_arch_protect(void)
{
    Critical_Enter();
    return 1;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    Critical_Leave();
}


err_t sys_mutex_new(sys_mutex_t *mutex)
{
    void* _mutex = Mutex_Create();
    if (!_mutex)
    {
        *mutex = NULL;
        return ERR_MEM;
    }

    *mutex = _mutex;
    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    Mutex_Acquire(*mutex, NoTimeout);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    Mutex_Release(*mutex);
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    Mutex_Delete(*mutex);
}

int sys_mutex_valid(sys_mutex_t *mutex)
{
    return (*mutex != NULL);
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    *mutex = NULL;
}


err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    void* _sema = Semaphore_Create(count);
    if (!_sema)
    {
        *sem = NULL;
        return ERR_MEM;
    }

    *sem = _sema;
    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem)
{
    Semaphore_Post(*sem, 1);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    u32 tmo = (timeout == 0) ? NoTimeout : timeout;
    int res = Semaphore_Wait(*sem, tmo);
    if (res == -1)
        return SYS_ARCH_TIMEOUT;

    return res;
}

void sys_sem_free(sys_sem_t *sem)
{
    Semaphore_Delete(*sem);
}

int sys_sem_valid(sys_sem_t *sem)
{
    return (*sem != NULL);
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    *sem = NULL;
}


err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    void* _mbox = Mailbox_Create(size);
    if (!_mbox)
    {
        *mbox = NULL;
        return ERR_MEM;
    }

    *mbox = _mbox;
    return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    int res = Mailbox_Send(*mbox, msg);
    LWIP_ASSERT("MAILBOX_SEND SHAT ITSELF!!", (res > 0));
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    int res = Mailbox_Send(*mbox, msg);
    if (res < 1)
        return ERR_MEM;

    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    u32 tmo = (timeout == 0) ? NoTimeout : timeout;
    int res = Mailbox_Recv(*mbox, tmo, msg);
    if (res == -1)
        return SYS_ARCH_TIMEOUT;

    return res;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    int res = Mailbox_Recv(*mbox, 0, msg);
    if (res == -1)
        return SYS_MBOX_EMPTY;

    return res;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    Mailbox_Delete(*mbox);
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return (*mbox != NULL);
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    *mbox = NULL;
}
