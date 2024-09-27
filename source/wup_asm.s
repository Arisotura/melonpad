.arch armv5te
.cpu arm926ej-s

.text

.arm
.align 4

.global EnableIRQ
EnableIRQ:
	mrs r0, cpsr
	bic r0, r0, #0x80
	msr cpsr_c, r0
	bx lr

.global DisableIRQ
DisableIRQ:
	mrs r1, cpsr
	mov r0, r1, lsr #7
	and r0, r0, #1
	orr r1, r1, #0x80
	msr cpsr_c, r1
	bx lr

.global RestoreIRQ
RestoreIRQ:
    and r0, r0, #1
    mrs r1, cpsr
	bic r1, r1, #0x80
	orr r1, r1, r0, lsl #7
	msr cpsr_c, r1
	bx lr

.global WaitForIRQ
WaitForIRQ:
    mov r0, #0
    mcr p15, 0, r0, c7, c0, 4
    bx lr
