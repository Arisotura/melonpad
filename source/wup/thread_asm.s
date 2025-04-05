.arch armv5te
.cpu arm926ej-s

.text

.arm
.align 2


.global Thread_Switch
.type Thread_Switch, %function
Thread_Switch:
    stmdb sp!, {r0-r12, lr}
    mrs r12, cpsr
    stmdb sp!, {r12}

    ldr r11, =CurThread
    ldr r12, [r11]
    str sp, [r12, #4]           @ save current stack ptr for this thread

    ldr r12, =NextThread
    ldr r12, [r12]
    ldr sp, [r12, #4]           @ load stack ptr from next thread
    str r12, [r11]              @ set current thread

    ldmia sp!, {r12}
    msr cpsr_f, r12
    ldmia sp!, {r0-r12, pc}


thread_init:
    mrs r0, cpsr
    bic r0, r0, #0x80
    msr cpsr_c, r0
    ldr lr, =Thread_Terminate
    ldmia sp!, {r0, pc}


.global Thread_InitStack
.type Thread_InitStack, %function
Thread_InitStack:
    stmdb sp!, {r8-r11, lr}
    stmdb r0!, {r1-r2}
    mov r8, #0
    mov r9, #0
    mov r10, #0
    mov r11, #0
    ldr r12, =thread_init
    stmdb r0!, {r8-r12} @ r9-r12, lr
    stmdb r0!, {r8-r11} @ r5-r8
    stmdb r0!, {r8-r11} @ r1-r4
    stmdb r0!, {r8-r9}  @ cpsr, r0
    ldmia sp!, {r8-r11, pc}
