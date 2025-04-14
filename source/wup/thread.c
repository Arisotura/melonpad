#include <wup/wup.h>

// threading support
// NOTES for possible improvements:
// * could add priority inversion

#define Magic_Thread        0x54485200
#define Magic_Mutex         0x54485201
#define Magic_Semaphore     0x54485202
#define Magic_EventMask     0x54485203
#define Magic_Mailbox       0x54485204

#define Flag_Waiting        (1<<0)
#define Flag_CheckWakeup    (1<<1)
#define Flag_Timeout        (1<<2)
#define Flag_Terminated     (1<<3)
#define Flag_MainThread     (1<<27)
#define Flag_Prio(x)        ((x)<<28)
#define Flag_GetPrio(x)     ((x)>>28)

struct sTimeout;

typedef struct sThread
{
    u32 Magic;

    void* StackPtr;
    void* StackBottom;
    u32 Flags;

    // for objects this thread is waiting on
    void* WaitObject;
    u32 WaitParam;
    struct sTimeout* WaitTimeout;

    // for other threads waiting on this thread to complete
    struct sThread* ExtWaitThreads[16];
    u16 ExtWaitBitmask;

    struct sThread* Prev;
    struct sThread* Next;

    char Name[16];

} sThread;

typedef struct sTimeout
{
    u32 Index;
    sThread* WaitThread;
    u32 StartTime;
    u32 Delay;

} sTimeout;

typedef struct sMutex
{
    u32 Magic;

    u32 Acquired;
    sThread* AcqThread;

    sThread* WaitThreads[16];
    u16 WaitBitmask;

} sMutex;

typedef struct sSemaphore
{
    u32 Magic;

    u32 Count;

    sThread* WaitThreads[16];
    u16 WaitBitmask;

} sSemaphore;

typedef struct sEventMask
{
    u32 Magic;

    u32 Val;

    sThread* WaitThreads[16];
    u32 WaitMasks[16];
    u16 WaitBitmask;

} sEventMask;

typedef struct sMailbox
{
    u32 Magic;

    sSemaphore* CountSema;

    u32 Size;
    u32 ReadPtr;
    u32 WritePtr;
    u32 Count;

    void* Data[1];

} sMailbox;

sThread* CurThread;
sThread* NextThread;

sThread* ThreadQueue[16];
sThread* CurThreadPerQueue[16];

u8 SchedReq;

sTimeout Timeouts[256];
u32 TimeoutBitmask[8];

u8 Critical;
u8 CritIRQ;

void* Thread_InitStack(void* stacktop, void* userdata, fnThreadFunc func);
void Thread_CancelWait(sThread* thread);
void Thread_ReqReschedule();
void Thread_Schedule();
void Thread_Switch();

sTimeout* Timeout_Add(void* object, u32 delay);
void Timeout_Remove(sTimeout* timeout);


static void Queue_Add(sThread* thread)
{
    int prio = Flag_GetPrio(thread->Flags);
    sThread* queue = ThreadQueue[prio];
    if (queue)
    {
        sThread* last = queue;
        while (last->Next) last = last->Next;

        last->Next = thread;
        thread->Prev = last;
        thread->Next = NULL;
    }
    else
    {
        thread->Prev = NULL;
        thread->Next = NULL;
        ThreadQueue[prio] = thread;
        CurThreadPerQueue[prio] = thread;
    }
}

static void Queue_Remove(sThread* thread)
{
    int prio = Flag_GetPrio(thread->Flags);

    if (thread->Prev == NULL && thread->Next == NULL)
    {
        ThreadQueue[prio] = NULL;
        CurThreadPerQueue[prio] = NULL;
    }
    else
    {
        if (thread->Prev)
            thread->Prev->Next = thread->Next;
        else
            ThreadQueue[prio] = thread->Next;

        if (thread->Next)
            thread->Next->Prev = thread->Prev;

        if (thread == CurThreadPerQueue[prio])
        {
            sThread* next = thread->Next;
            if (!next) next = ThreadQueue[prio];
            CurThreadPerQueue[prio] = next;
        }
    }
}


static void IdleThread(void* userdata)
{
    for (;;)
    {
        //Thread_Yield();
        WaitForIRQ();
    }
}


int Thread_Init()
{
    CurThread = NULL;
    NextThread = NULL;
    SchedReq = 0;

    memset(ThreadQueue, 0, sizeof(ThreadQueue));
    memset(CurThreadPerQueue, 0, sizeof(CurThreadPerQueue));

    memset(Timeouts, 0, sizeof(Timeouts));
    memset(TimeoutBitmask, 0, sizeof(TimeoutBitmask));

    for (int i = 0; i < 256; i++)
        Timeouts[i].Index = i;

    Critical = 0;
    CritIRQ = 0;

    sThread* mainthread = (sThread*)malloc(sizeof(sThread));
    if (!mainthread)
        return 0;

    memset(mainthread, 0, sizeof(sThread));
    mainthread->Magic = Magic_Thread;
    mainthread->Flags = Flag_MainThread | Flag_Prio(8);
    strncpy(mainthread->Name, "main", 15);

    CurThread = mainthread;
    NextThread = CurThread;
    Queue_Add(mainthread);

    Thread_Create(IdleThread, NULL, 256, 15, "idle");

    return 1;
}


void* Thread_Create(fnThreadFunc func, void* userdata, u32 stacksize, u32 prio, const char* name)
{
    int irq = DisableIRQ();

    sThread* thread = (sThread*)malloc(sizeof(sThread));
    if (!thread)
    {
        RestoreIRQ(irq);
        return NULL;
    }

    memset(thread, 0, sizeof(sThread));

    void* stack = malloc(stacksize);
    if (!stack)
    {
        free(thread);
        RestoreIRQ(irq);
        return NULL;
    }

    thread->Magic = Magic_Thread;
    thread->Flags = Flag_Prio(prio);
    strncpy(thread->Name, name, 15);
    thread->StackBottom = stack;

    u32* stacktop = (u32*)(stack + stacksize);

    // prime the stack to start the thread
    stacktop = Thread_InitStack(stacktop, userdata, func);
    thread->StackPtr = stacktop;

    Queue_Add(thread);

    RestoreIRQ(irq);
    return thread;
}

void Thread_Delete(void* _thread)
{
    sThread* thread = (sThread*)_thread;
    if (!thread)
        return;
    if (thread->Magic != Magic_Thread)
        return;
    if (!(thread->Flags & Flag_Terminated))
        return;

    free(thread->StackBottom);
    free(thread);
}

void* Thread_Current()
{
    return CurThread;
}

const char* Thread_CurName()
{
    return CurThread->Name;
}

void Thread_Terminate()
{
    if (CurThread->Flags & Flag_MainThread)
        return;

    int irq = DisableIRQ();

    sThread* thread = CurThread;
    Queue_Remove(thread);

    for (int i = 0; i < 16; i++)
    {
        if (!(thread->ExtWaitBitmask & (1<<i)))
            continue;

        sThread* other = thread->ExtWaitThreads[i];
        other->Flags &= ~Flag_Waiting;
        other->WaitObject = NULL;
        thread->ExtWaitBitmask &= ~(1<<i);
        Timeout_Remove(other->WaitTimeout);
    }

    thread->Flags |= Flag_Terminated;
    Thread_CancelWait(thread);

    Thread_ReqReschedule();
    RestoreIRQ(irq);
}

int Thread_Wait(void* _thread, u32 timeout)
{
    sThread* thread = (sThread*)_thread;
    if (!thread)
        return 0;
    if (thread->Magic != Magic_Thread)
        return 0;
    if (thread->ExtWaitBitmask == 0xFFFF)
        return 0;

    int irq = DisableIRQ();

    if (thread->Flags & Flag_Terminated)
    {
        RestoreIRQ(irq);
        return 1;
    }
    if (timeout == 0)
    {
        RestoreIRQ(irq);
        return -1;
    }

    if (timeout == NoTimeout)
        CurThread->WaitTimeout = NULL;
    else
    {
        CurThread->WaitTimeout = Timeout_Add(CurThread, timeout);
        if (!CurThread->WaitTimeout)
        {
            RestoreIRQ(irq);
            return 0;
        }
    }

    CurThread->WaitObject = thread;
    CurThread->Flags &= ~Flag_Timeout;
    CurThread->Flags |= Flag_Waiting;

    for (int i = 0; i < 16; i++)
    {
        if (thread->ExtWaitBitmask & (1<<i))
            continue;

        thread->ExtWaitThreads[i] = CurThread;

        thread->ExtWaitBitmask |= (1<<i);
        CurThread->WaitParam = i;
        break;
    }

    Thread_ReqReschedule();
    RestoreIRQ(irq);

    if (CurThread->Flags & Flag_Timeout)
        return -1;

    return 1;
}


void Thread_Yield()
{
    DisableIRQ();
    Thread_Schedule();
    Thread_Switch();
    EnableIRQ();
}

int Thread_Sleep(u32 time)
{
    if (time < 1)
        return 1;

    int irq = DisableIRQ();

    CurThread->WaitTimeout = Timeout_Add(CurThread, time);
    if (!CurThread->WaitTimeout)
    {
        RestoreIRQ(irq);
        return 0;
    }

    CurThread->WaitObject = NULL;
    CurThread->Flags &= ~Flag_Timeout;
    CurThread->Flags |= Flag_Waiting;

    Thread_ReqReschedule();
    RestoreIRQ(irq);
    return 1;
}


void Thread_ReqReschedule()
{
    // if we are running from within an IRQ handler,
    // the reschedule and switch will take place when returning from IRQ
    // otherwise: should be called with IRQ disabled

    if (IsInIRQ())
    {
        SchedReq |= 1;
    }
    else if (Critical)
    {
        SchedReq |= 2;
    }
    else
    {
        Thread_Schedule();
        Thread_Switch();
    }
}

int Thread_CanWakeup(sThread* thread)
{
    if (!thread->WaitObject)
        return 0;

    u32 magic = *(u32*)thread->WaitObject;
    switch (magic)
    {
    case Magic_Thread:
        {
            sThread* other = (sThread*)thread->WaitObject;
            if (other->Flags & Flag_Terminated)
                return 1;
        }
        break;

    case Magic_Mutex:
        {
            sMutex* mutex = (sMutex*)thread->WaitObject;
            if (mutex->Acquired == 0)
                return 1;
        }
        break;

    case Magic_Semaphore:
        {
            sSemaphore* sema = (sSemaphore*)thread->WaitObject;
            if (sema->Count != 0)
                return 1;
        }
        break;

    case Magic_EventMask:
        {
            sEventMask *event = (sEventMask*)thread->WaitObject;
            u32 waitmask = event->WaitMasks[thread->WaitParam];
            if (event->Val & waitmask)
                return 1;
        }
        break;
    }

    return 0;
}

void Thread_CancelWait(sThread* thread)
{
    thread->Flags &= ~Flag_Waiting;

    if (thread->WaitObject)
    {
        u32 magic = *(u32*)thread->WaitObject;
        switch (magic)
        {
        case Magic_Thread:
            {
                sThread* other = (sThread*)thread->WaitObject;
                u32 id = thread->WaitParam;
                other->ExtWaitBitmask &= ~(1<<id);
                other->ExtWaitThreads[id] = NULL;
            }
            break;

        case Magic_Mutex:
            {
                sMutex* mutex = (sMutex*)thread->WaitObject;
                u32 id = thread->WaitParam;
                mutex->WaitBitmask &= ~(1<<id);
                mutex->WaitThreads[id] = NULL;
            }
            break;

        case Magic_Semaphore:
            {
                sSemaphore* sema = (sSemaphore*)thread->WaitObject;
                u32 id = thread->WaitParam;
                sema->WaitBitmask &= ~(1<<id);
                sema->WaitThreads[id] = NULL;
            }
            break;

        case Magic_EventMask:
            {
                sEventMask *event = (sEventMask*)thread->WaitObject;
                u32 id = thread->WaitParam;
                event->WaitBitmask &= ~(1<<id);
                event->WaitThreads[id] = NULL;
            }
            break;
        }

        thread->WaitObject = NULL;
    }

    Timeout_Remove(thread->WaitTimeout);
    thread->WaitTimeout = NULL;
}

void Thread_Schedule()
{
    sThread* choice = NULL;

    for (int prio = 0; prio < 16; prio++)
    {
        // skip queue if it's empty
        if (!ThreadQueue[prio])
            continue;

        // start from this queue's next thread, looping back to current thread
        sThread* cur = CurThreadPerQueue[prio];
        sThread* next = cur->Next;
        for (;;)
        {
            if (!next)
                next = ThreadQueue[prio];

            if (!(next->Flags & Flag_Waiting))
            {
                choice = next;
                break;
            }

            if (next->Flags & Flag_CheckWakeup)
            {
                next->Flags &= ~Flag_CheckWakeup;

                if (Thread_CanWakeup(next))
                {
                    Thread_CancelWait(next);
                    choice = next;
                    break;
                }
            }

            if (next == cur)
                break;

            next = next->Next;
        }

        if (choice)
        {
            CurThreadPerQueue[prio] = choice;
            break;
        }
    }

    NextThread = choice;
}

u32 Thread_PostIRQ()
{
    if (!(SchedReq & 1))
        return 0;

    SchedReq &= ~1;
    Thread_Schedule();
    return 1;
}

void Thread_Tick()
{
    u32 time = REG_COUNTUP_VALUE;
    int resched = 0;

    for (int i = 0; i < 8; i++)
    {
        u32 mask = TimeoutBitmask[i];
        if (mask == 0)
            continue;

        for (int j = 0; j < 32; j++)
        {
            if (!(mask & (1<<j)))
                continue;

            sTimeout* timeout = &Timeouts[(i*32) + j];
            if ((time - timeout->StartTime) < timeout->Delay)
                continue;

            sThread* thread = timeout->WaitThread;
            thread->Flags |= Flag_Timeout;
            Thread_CancelWait(thread);
            resched = 1;
        }
    }

    if (resched)
        Thread_ReqReschedule();
}


void Critical_Enter()
{
    if (Critical)
    {
        Critical++;
        return;
    }

    CritIRQ = DisableIRQ();
    Critical = 1;
}

void Critical_Leave()
{
    if (!Critical) return;
    Critical--;
    if (Critical) return;

    if (SchedReq & 2)
    {
        SchedReq &= ~2;
        Thread_Schedule();
        Thread_Switch();
    }

    RestoreIRQ(CritIRQ);
}


sTimeout* Timeout_Add(void* object, u32 delay)
{
    delay *= 1000;

    for (int i = 0; i < 8; i++)
    {
        u32 mask = TimeoutBitmask[i];
        if (mask == 0xFFFFFFFF)
            continue;

        for (int j = 0; j < 32; j++)
        {
            if (mask & (1<<j))
                continue;

            sTimeout* timeout = &Timeouts[(i<<5) + j];
            timeout->WaitThread = object;
            timeout->StartTime = REG_COUNTUP_VALUE;
            timeout->Delay = delay;

            TimeoutBitmask[i] |= (1<<j);
            return timeout;
        }
    }

    return NULL;
}

void Timeout_Remove(sTimeout* timeout)
{
    if (!timeout) return;
    u32 id = timeout->Index;
    TimeoutBitmask[id>>5] &= ~(1 << (id&0x1F));
    timeout->WaitThread = NULL;
}


void* Mutex_Create()
{
    sMutex* mutex = (sMutex*)malloc(sizeof(sMutex));
    if (!mutex)
        return NULL;

    memset(mutex, 0, sizeof(sMutex));
    mutex->Magic = Magic_Mutex;

    return mutex;
}

void Mutex_Delete(void* _mutex)
{
    sMutex* mutex = (sMutex*)_mutex;
    if (!mutex)
        return;
    if (mutex->Magic != Magic_Mutex)
        return;

    int irq = DisableIRQ();

    // release any threads that are waiting on this mutex
    for (int i = 0; i < 16; i++)
    {
        if (!(mutex->WaitBitmask & (1<<i)))
            continue;

        sThread* thread = mutex->WaitThreads[i];
        thread->Flags &= ~Flag_Waiting;
        thread->Flags |= Flag_Timeout;
        thread->WaitObject = NULL;
        Timeout_Remove(thread->WaitTimeout);
    }

    // delete the mutex itself
    free(mutex);

    Thread_ReqReschedule();
    RestoreIRQ(irq);
}

int Mutex_Acquire(void* _mutex, u32 timeout)
{
    sMutex* mutex = (sMutex*)_mutex;
    if (!mutex)
        return 0;
    if (mutex->Magic != Magic_Mutex)
        return 0;
    if (mutex->WaitBitmask == 0xFFFF)
        return 0;

    int irq = DisableIRQ();

    if (!mutex->Acquired)
    {
        mutex->Acquired = 1;
        mutex->AcqThread = CurThread;
        RestoreIRQ(irq);
        return 1;
    }
    if (CurThread == mutex->AcqThread)
    {
        mutex->Acquired++;
        RestoreIRQ(irq);
        return 1;
    }
    if (timeout == 0)
    {
        RestoreIRQ(irq);
        return -1;
    }

    if (timeout == NoTimeout)
        CurThread->WaitTimeout = NULL;
    else
    {
        CurThread->WaitTimeout = Timeout_Add(CurThread, timeout);
        if (!CurThread->WaitTimeout)
        {
            RestoreIRQ(irq);
            return 0;
        }
    }

    CurThread->WaitObject = mutex;
    CurThread->Flags &= ~Flag_Timeout;
    CurThread->Flags |= Flag_Waiting;

    for (int i = 0; i < 16; i++)
    {
        if (mutex->WaitBitmask & (1<<i))
            continue;

        mutex->WaitThreads[i] = CurThread;

        mutex->WaitBitmask |= (1<<i);
        CurThread->WaitParam = i;
        break;
    }

    Thread_ReqReschedule();

    if (CurThread->Flags & Flag_Timeout)
    {
        RestoreIRQ(irq);
        return -1;
    }

    mutex->Acquired = 1;
    mutex->AcqThread = CurThread;
    RestoreIRQ(irq);
    return 1;
}

int Mutex_Release(void* _mutex)
{
    sMutex* mutex = (sMutex*)_mutex;
    if (!mutex)
        return 0;
    if (mutex->Magic != Magic_Mutex)
        return 0;

    int irq = DisableIRQ();

    if ((!mutex->Acquired) || (mutex->AcqThread != CurThread))
    {
        RestoreIRQ(irq);
        return 0;
    }

    mutex->Acquired--;
    if (mutex->Acquired)
    {
        RestoreIRQ(irq);
        return 1;
    }

    for (int i = 0; i < 16; i++)
    {
        if (!(mutex->WaitBitmask & (1<<i)))
            continue;

        sThread* thread = mutex->WaitThreads[i];
        thread->Flags |= Flag_CheckWakeup;
    }

    Thread_ReqReschedule();
    RestoreIRQ(irq);
    return 1;
}


void* Semaphore_Create(u32 num)
{
    sSemaphore* sema = (sSemaphore*)malloc(sizeof(sSemaphore));
    if (!sema)
        return NULL;

    memset(sema, 0, sizeof(sSemaphore));
    sema->Magic = Magic_Semaphore;
    sema->Count = num;

    return sema;
}

void Semaphore_Delete(void* _sema)
{
    sSemaphore* sema = (sSemaphore*)_sema;
    if (!sema)
        return;
    if (sema->Magic != Magic_Semaphore)
        return;

    int irq = DisableIRQ();

    // release any threads that are waiting on this semaphore
    for (int i = 0; i < 16; i++)
    {
        if (!(sema->WaitBitmask & (1<<i)))
            continue;

        sThread* thread = sema->WaitThreads[i];
        thread->Flags &= ~Flag_Waiting;
        thread->Flags |= Flag_Timeout;
        thread->WaitObject = NULL;
        Timeout_Remove(thread->WaitTimeout);
    }

    // delete the semaphore itself
    free(sema);

    Thread_ReqReschedule();
    RestoreIRQ(irq);
}

int Semaphore_Wait(void* _sema, u32 timeout)
{
    sSemaphore* sema = (sSemaphore*)_sema;
    if (!sema)
        return 0;
    if (sema->Magic != Magic_Semaphore)
        return 0;
    if (sema->WaitBitmask == 0xFFFF)
        return 0;

    int irq = DisableIRQ();

    if (sema->Count)
    {
        sema->Count--;
        RestoreIRQ(irq);
        return 1;
    }
    if (timeout == 0)
    {
        RestoreIRQ(irq);
        return -1;
    }

    if (timeout == NoTimeout)
        CurThread->WaitTimeout = NULL;
    else
    {
        CurThread->WaitTimeout = Timeout_Add(CurThread, timeout);
        if (!CurThread->WaitTimeout)
        {
            RestoreIRQ(irq);
            return 0;
        }
    }

    CurThread->WaitObject = sema;
    CurThread->Flags &= ~Flag_Timeout;
    CurThread->Flags |= Flag_Waiting;

    for (int i = 0; i < 16; i++)
    {
        if (sema->WaitBitmask & (1<<i))
            continue;

        sema->WaitThreads[i] = CurThread;

        sema->WaitBitmask |= (1<<i);
        CurThread->WaitParam = i;
        break;
    }

    Thread_ReqReschedule();
    if (CurThread->Flags & Flag_Timeout)
    {
        RestoreIRQ(irq);
        return -1;
    }

    sema->Count--;
    RestoreIRQ(irq);
    return 1;
}

int Semaphore_Post(void* _sema, u32 num)
{
    sSemaphore* sema = (sSemaphore*)_sema;
    if (!sema)
        return 0;
    if (sema->Magic != Magic_Semaphore)
        return 0;

    int irq = DisableIRQ();

    sema->Count += num;

    for (int i = 0; i < 16; i++)
    {
        if (!(sema->WaitBitmask & (1<<i)))
            continue;

        sThread* thread = sema->WaitThreads[i];
        thread->Flags |= Flag_CheckWakeup;
    }

    Thread_ReqReschedule();
    RestoreIRQ(irq);
    return 1;
}


void* EventMask_Create()
{
    sEventMask* event = (sEventMask*)malloc(sizeof(sEventMask));
    if (!event)
        return NULL;

    memset(event, 0, sizeof(sEventMask));
    event->Magic = Magic_EventMask;

    return event;
}

void EventMask_Delete(void* _event)
{
    sEventMask* event = (sEventMask*)_event;
    if (!event)
        return;
    if (event->Magic != Magic_EventMask)
        return;

    int irq = DisableIRQ();

    // release any threads that are waiting on this event
    for (int i = 0; i < 16; i++)
    {
        if (!(event->WaitBitmask & (1<<i)))
            continue;

        sThread* thread = event->WaitThreads[i];
        thread->Flags &= ~Flag_Waiting;
        thread->Flags |= Flag_Timeout;
        thread->WaitObject = NULL;
        Timeout_Remove(thread->WaitTimeout);
    }

    // delete the event itself
    free(event);

    Thread_ReqReschedule();
    RestoreIRQ(irq);
}

int EventMask_Wait(void* _event, u32 mask, u32 timeout, u32* res)
{
    sEventMask* event = (sEventMask*)_event;
    if (!event)
        return 0;
    if (event->Magic != Magic_EventMask)
        return 0;
    if (event->WaitBitmask == 0xFFFF)
        return 0;

    int irq = DisableIRQ();

    if (event->Val & mask)
    {
        if (res) *res = event->Val & mask;
        RestoreIRQ(irq);
        return 1;
    }
    if (timeout == 0)
    {
        if (res) *res = event->Val & mask;
        RestoreIRQ(irq);
        return -1;
    }

    if (timeout == NoTimeout)
        CurThread->WaitTimeout = NULL;
    else
    {
        CurThread->WaitTimeout = Timeout_Add(CurThread, timeout);
        if (!CurThread->WaitTimeout)
        {
            RestoreIRQ(irq);
            return 0;
        }
    }

    CurThread->WaitObject = event;
    CurThread->Flags &= ~Flag_Timeout;
    CurThread->Flags |= Flag_Waiting;

    for (int i = 0; i < 16; i++)
    {
        if (event->WaitBitmask & (1<<i))
            continue;

        event->WaitThreads[i] = CurThread;
        event->WaitMasks[i] = mask;

        event->WaitBitmask |= (1<<i);
        CurThread->WaitParam = i;
        break;
    }

    Thread_ReqReschedule();
    if (res) *res = event->Val & mask;
    RestoreIRQ(irq);

    if (CurThread->Flags & Flag_Timeout)
        return -1;

    return 1;
}

int EventMask_Clear(void* _event, u32 mask)
{
    sEventMask* event = (sEventMask*)_event;
    if (!event)
        return 0;
    if (event->Magic != Magic_EventMask)
        return 0;

    int irq = DisableIRQ();

    event->Val &= ~mask;

    RestoreIRQ(irq);
    return 1;
}

int EventMask_Signal(void* _event, u32 mask)
{
    sEventMask* event = (sEventMask*)_event;
    if (!event)
        return 0;
    if (event->Magic != Magic_EventMask)
        return 0;

    int irq = DisableIRQ();

    event->Val |= mask;

    for (int i = 0; i < 16; i++)
    {
        if (!(event->WaitBitmask & (1<<i)))
            continue;
        if (!(event->WaitMasks[i] & event->Val))
            continue;

        sThread* thread = event->WaitThreads[i];
        thread->Flags |= Flag_CheckWakeup;
    }

    Thread_ReqReschedule();
    RestoreIRQ(irq);
    return 1;
}


void* Mailbox_Create(u32 size)
{
    u32 totalsize = sizeof(sMailbox) + ((size-1) * sizeof(void*));
    sMailbox* mbox = (sMailbox*)malloc(totalsize);
    if (!mbox)
        return NULL;

    memset(mbox, 0, totalsize);
    mbox->Magic = Magic_Mailbox;
    mbox->Size = size;

    mbox->CountSema = Semaphore_Create(0);
    if (!mbox->CountSema)
    {
        free(mbox);
        return NULL;
    }

    return mbox;
}

void Mailbox_Delete(void* _mbox)
{
    sMailbox* mbox = (sMailbox*)_mbox;
    if (!mbox)
        return;
    if (mbox->Magic != Magic_Mailbox)
        return;

    Semaphore_Delete(mbox->CountSema);
    free(mbox);
}

int Mailbox_Recv(void* _mbox, u32 timeout, void** data)
{
    sMailbox* mbox = (sMailbox*)_mbox;
    if (!mbox)
        return 0;
    if (mbox->Magic != Magic_Mailbox)
        return 0;

    int irq = DisableIRQ();

    int res = Semaphore_Wait(mbox->CountSema, timeout);
    if (res < 1)
    {
        RestoreIRQ(irq);
        return res;
    }

    if (mbox->Count < 1)
    {
        // ??? this should not happen
        printf("??? MAILBOX BROKEN: SEMA SIGNALED BUT COUNT=0\n");
        RestoreIRQ(irq);
        return 0;
    }

    if (data) *data = mbox->Data[mbox->ReadPtr];
    mbox->ReadPtr++;
    if (mbox->ReadPtr >= mbox->Size)
        mbox->ReadPtr = 0;
    mbox->Count--;

    RestoreIRQ(irq);
    return 1;
}

int Mailbox_Send(void* _mbox, void* data)
{
    sMailbox* mbox = (sMailbox*)_mbox;
    if (!mbox)
        return 0;
    if (mbox->Magic != Magic_Mailbox)
        return 0;

    int irq = DisableIRQ();

    if (mbox->Count >= mbox->Size)
    {
        RestoreIRQ(irq);
        return 0;
    }

    mbox->Data[mbox->WritePtr] = data;
    mbox->WritePtr++;
    if (mbox->WritePtr >= mbox->Size)
        mbox->WritePtr = 0;
    mbox->Count++;

    Semaphore_Post(mbox->CountSema, 1);
    RestoreIRQ(irq);
    return 1;
}
