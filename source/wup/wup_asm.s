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
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
    bx lr

.global DC_FlushAll
DC_FlushAll:
    mov	r1, #0
_dcflushall_outer_loop:
    mov	r0, #0
_dcflushall_inner_loop:
    orr	r2, r1, r0			@ generate segment and line address
    mcr	p15, 0, r2, c7, c14, 2		@ clean and flush the line
    add	r0, r0, #32
    cmp	r0, #0x1000         @ 16K / 4
    bne	_dcflushall_inner_loop
    add	r1, r1, #0x40000000
    cmp	r1, #0
    bne	_dcflushall_outer_loop
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 4
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

.global DisableMMU
DisableMMU:
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0xD
    bic r0, r0, #0x1000
    mcr p15, 0, r0, c1, c0, 0
    bx lr

.global CallLoader
CallLoader:
    mov r12, #0x20
    ldr r12, [r12]
    bx r12
