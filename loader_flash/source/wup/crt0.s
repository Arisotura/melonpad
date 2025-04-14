.arch armv5te
.cpu arm926ej-s

.section ".crt0","ax"

.arm
.align 2

.global _start
_start:
ldr pc, [pc, #0x18]
ldr pc, [pc, #0x18]
ldr pc, [pc, #0x18]
ldr pc, [pc, #0x18]
ldr pc, [pc, #0x18]
ldr pc, [pc, #0x18]
ldr pc, [pc, #0x18]
ldr pc, [pc, #0x18]
.word vec_reset
.word vec_undefined
.word vec_swi
.word vec_prefabort
.word vec_dataabort
.word halt_loop
.word vec_irq
.word halt_loop


vec_reset:
    mov r6, r0
	mov r0, #0xD2
	msr cpsr_c, r0
	ldr sp, =__sp_irq
	mov r0, #0xD3
    msr cpsr_c, r0
    ldr sp, =__sp_svc
	mov r0, #0xDF
	msr cpsr_c, r0
	ldr sp, =__sp_usr

	//mov r0, #0xD0
    //msr cpsr_c, r0

	ldr r0, =__bss_start__
	ldr r1, =__bss_end__
	bl clear_mem

	ldr r3, =WUP_Init
	blx r3

    mov r0, r6
	ldr r3, =main
	blx r3
	b halt_loop

halt_loop:
	b halt_loop
	
vec_swi:
	b halt_loop

vec_irq:
	stmdb sp!, {r0-r3, r12, lr}
	ldr r3, =IRQHandler
	blx r3
	ldmia sp!, {r0-r3, r12, lr}
	subs pc, lr, #4

vec_undefined:
vec_prefabort:
vec_dataabort:
    b halt_loop


clear_mem:
    add r0, r0, #3
    bic r0, r0, #3
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r5, #0
_clear_loop:
    stmia r0!, {r2-r5}
    cmp r0, r1
    blt _clear_loop
    bx lr


.section ".init"

.globl _init
_init:
	bx lr
