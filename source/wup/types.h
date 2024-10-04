#ifndef _TYPES_H_
#define _TYPES_H_

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long int u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long int s64;

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;

typedef void (*fnIRQHandler)(int irq, void* user);

#endif
