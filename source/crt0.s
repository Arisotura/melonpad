.arch armv5te
.cpu arm926ej-s

.section ".crt0","ax"

.arm
.align 4

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
	.word halt_loop
	.word vec_swi
	.word halt_loop
	.word halt_loop
	.word halt_loop
	.word vec_irq
	.word halt_loop


vec_reset:
	mov r0, #0xD2
	msr cpsr_c, r0
	mov sp, #0x00380000
	mov r0, #0xDF
	msr cpsr_c, r0
	mov sp, #0x00340000
	mrc p15,0,r0,c1,c0,0
	bic r0, r0, #0xD
	mcr p15,0,r0,c1,c0,0
	bl main
	b halt_loop

halt_loop:
	b halt_loop
	
vec_swi:
	b halt_loop

vec_irq:
	stmdb sp!, {r0-r3, r12, lr}
	bl IRQHandler
	ldmia sp!, {r0-r3, r12, lr}
	subs pc, lr, #4
