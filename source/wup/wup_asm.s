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

.global IsInIRQ
IsInIRQ:
    mrs r0, cpsr
    and r0, r0, #0x1F
    cmp r0, #0x12
    moveq r0, #1
    movne r0, #0
    bx lr


.global DC_FlushRange
DC_FlushRange:
    mov r2, #0
    mcr p15, 0, r2, c7, c10, 4
    add r1, r0, r1
    bic r0, r0, #31
_dcflushrange_loop:
    mcr p15, 0, r0, c7, c14, 1
    add r0, r0, #32
    cmp r0, r1
    blt _dcflushrange_loop
    bx lr

.global DC_InvalidateRange
DC_InvalidateRange:
    add r1, r0, r1
    bic r0, r0, #31
_dcinvalidaterange_loop:
    mcr p15, 0, r0, c7, c6, 1
    add r0, r0, #32
    cmp r0, r1
    blt _dcinvalidaterange_loop
    bx lr

.global DC_InvalidateAll
DC_InvalidateAll:
    mov r0, #0
    mcr p15, 0, r0, c7, c6, 0
    bx lr

.global IC_InvalidateRange
IC_InvalidateRange:
    add r1, r0, r1
    bic r0, r0, #31
_icinvalidaterange_loop:
    mcr p15, 0, r0, c7, c5, 1
    add r0, r0, #32
    cmp r0, r1
    blt _icinvalidaterange_loop
    bx lr

.global IC_InvalidateAll
IC_InvalidateAll:
    mov r0, #0
    mcr p15, 0, r0, c7, c5, 0
    bx lr
