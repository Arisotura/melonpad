#ifndef _THREAD_H_
#define _THREAD_H_

#define NoTimeout   0xFFFFFFFF

typedef void (*fnThreadFunc)(void* userdata);

int Thread_Init();

void* Thread_Create(fnThreadFunc func, void* userdata, u32 stacksize, u32 prio, const char* name);
void Thread_Delete(void* thread);
void* Thread_Current();
const char* Thread_CurName();
void Thread_Terminate();
int Thread_Wait(void* thread, u32 timeout);
void Thread_Yield();
int Thread_Sleep(u32 time);

void Critical_Enter();
void Critical_Leave();

void* Mutex_Create();
void Mutex_Delete(void* mutex);
int Mutex_Acquire(void* mutex, u32 timeout);
int Mutex_Release(void* mutex);

void* Semaphore_Create(u32 num);
void Semaphore_Delete(void* sema);
int Semaphore_Wait(void* sema, u32 timeout);
int Semaphore_Post(void* sema, u32 num);

void* EventMask_Create();
void EventMask_Delete(void* event);
int EventMask_Wait(void* event, u32 mask, u32 timeout, u32* res);
int EventMask_Clear(void* event, u32 mask);
int EventMask_Signal(void* event, u32 mask);

void* Mailbox_Create(u32 size);
void Mailbox_Delete(void* mbox);
int Mailbox_Recv(void* mbox, u32 timeout, void** data);
int Mailbox_Send(void* mbox, void* data);

#endif // _THREAD_H_
