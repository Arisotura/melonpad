.arch armv5te
.cpu arm926ej-s

.text

.arm
.align 4

.global GetCP15Reg
GetCP15Reg:
    and r0, r0, #0xF
    and r1, r1, #0xF
    and r2, r2, #0x7
    ldr r3, [pc, #0x30]
    bic r3, r3, #0xF0000
    orr r3, r3, r0, lsl #16
    bic r3, r3, #0xF
    orr r3, r3, r1
    bic r3, r3, #0xE0
    orr r3, r3, r2, lsl #5
    str r3, [pc, #0x14]
    add r3, pc, #0x10
    mcr p15, 0, r3, c7, c10, 1
    mcr p15, 0, r3, c7, c5, 1
    nop
    nop
    nop
    mrc p15, 0, r0, c0, c0, 0
    bx lr
