.arch armv5te
.cpu arm926ej-s

.section ".crt0","ax"

.arm
.align 2

.type _init, %function

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
	mov r0, #0xD2
	msr cpsr_c, r0
	ldr sp, =__sp_irq
	mov r0, #0xD3
    msr cpsr_c, r0
    ldr sp, =__sp_svc
	mov r0, #0xDF
	msr cpsr_c, r0
	ldr sp, =__sp_usr

	bl setup_mmu

	//mov r0, #0xD0
    //msr cpsr_c, r0

	ldr r0, =__bss_start__
	ldr r1, =__bss_end__
	bl clear_mem

	ldr r3, =WUP_Init
	blx r3

	//ldr r3, =__libc_init_array
    //blx r3

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
	cmp r0, #0
	ldmia sp!, {r0-r3, r12, lr}
	subeqs pc, lr, #4

    stmdb sp!, {r3, r12}
    stmdb sp!, {sp}^
    ldmia sp!, {r12}
    mrs r3, spsr
    stmdb r12, {r3, lr}     @ store SPSR and LR on user stack
    ldmia sp!, {r3, r12}

    msr cpsr_c, #0xDF
    sub sp, sp, #8
    stmdb sp!, {r0-r3, r12, lr}

    ldr r3, =Thread_Switch
    blx r3

    ldmia sp!, {r0-r3, r12, lr}
    add sp, sp, #8
    msr cpsr_c, #0xD2

    stmdb sp!, {r3, r12}
    stmdb sp!, {sp}^
    ldmia sp!, {r12}
    ldmdb r12, {r3, lr}     @ restore SPSR and LR
    msr spsr, r3
    ldmia sp!, {r3, r12}
    subs pc, lr, #4


vec_undefined:
vec_prefabort:
vec_dataabort:
    ldr r3, =ExceptionHandler
    blx r3
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


// MMU setup
// (TODO: main RAM mirrors?)
// 00000000-003F8000: main RAM
// 003F8000-003FFFFF: TLB
// E0010000-E001FFFF: SDIO
// F0000000-FFFFFFFF: I/O

setup_mmu:
    mov r10, lr
    mov r11, #0x3F8000          // L1 table pointer
    add r12, r11, #0x4000       // L2 table pointer

    // disable caches
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0xD
    bic r0, r0, #0x1000
    mcr p15, 0, r0, c1, c0, 0

    // zerofill the tables
    mov r0, r11
    add r1, r0, #0x3000
    bl clear_mem

    // main RAM
    mov r0, #0
    mov r1, #0x300000
    ldr r2, =0xC1E          // R/W, cacheable, section
    bl mmu_l1_fill_16m

    mov r0, #0x300000
    mov r1, #0x100000
    bl mmu_l1_fill_sub

    // SDIO
    mov r0, #0xE0000000
    mov r1, #0x100000
    bl mmu_l1_fill_sub

    // I/O
    mov r0, #0xF0000000
    mov r1, #0x10000000
    ldr r2, =0xC12          // R/W, non-cacheable, section
    bl mmu_l1_fill_16m

    // fill L2 tables

    // main RAM
    mov r0, #0x300000
    mov r1, #0xF0000
    ldr r2, =0xFFD          // R/W, cacheable, large page
    bl mmu_l2_fill_64k

    mov r0, #0x3F0000
    mov r1, #0x8000
    ldr r2, =0xFFE          // R/W, cacheable, small page
    bl mmu_l2_fill_4k

    // TLB region
    mov r0, #0x3F8000
    mov r1, #0x8000
    ldr r2, =0xAAE          // RO, cacheable, small page
    bl mmu_l2_fill_4k

    // SDIO
    ldr r0, =0xE0010000
    mov r1, #0x10000
    ldr r2, =0xFF1          // R/W, non-cacheable, large page
    bl mmu_l2_fill_64k

    // setup CP15 regs
    mcr p15, 0, r11, c2, c0, 0
    ldr r0, =0x55555555
    mcr p15, 0, r0, c3, c0, 0
    mov r0, #0
    mcr p15, 0, r0, c13, c0, 0
    mcr p15, 0, r0, c1, c0, 0

    // enable caches
    mrc p15, 0, r0, c1, c0, 0
    orr r0, r0, #0xD
    orr r0, r0, #0x1000
    mcr p15, 0, r0, c1, c0, 0

    bx r10


// L1 = 1MB granularity
// r0 = base address
// r1 = length
mmu_l1_fill_sub:
    mov r4, r0, lsr #20
    add r4, r11, r4, lsl #2
_mmu_l1_sub_loop:
    orr r3, r12, #0x11
    str r3, [r4], #4
    add r12, r12, #0x400
    subs r1, r1, #0x100000
    bne _mmu_l1_sub_loop
    bx lr


// L1 = 1MB granularity
// r0 = base address
// r1 = length
// r2 = params
mmu_l1_fill_16m:
    mov r4, r0, lsr #20
    add r4, r11, r4, lsl #2
_mmu_l1_16m_loop:
    orr r3, r0, r2
    str r3, [r4], #4
    add r0, r0, #0x100000
    subs r1, r1, #0x100000
    bne _mmu_l1_16m_loop
    bx lr


// L2 = 4K granularity
// r0 = base address
// r1 = length
// r2 = params
mmu_l2_fill_64k:
    mov r4, r0, lsr #20
    add r4, r11, r4, lsl #2     // r4 = L1 table entry ptr
    ldr r5, [r4], #4
    bic r5, r5, #0xFF
    bic r5, r5, #0x300          // r5 = L2 table addr
    mov r6, r0, lsr #16
    and r6, r6, #0xF             // r6 = L2 table index
    add r5, r5, r6, lsl #6
_mmu_l2_64k_loop:
    orr r3, r0, r2
    mov r7, r3
    mov r8, r3
    mov r9, r3
    stmia r5!, {r3, r7-r9}
    stmia r5!, {r3, r7-r9}
    stmia r5!, {r3, r7-r9}
    stmia r5!, {r3, r7-r9}
    add r0, r0, #0x10000
    subs r1, r1, #0x10000
    beq _mmu_l2_64k_ret
    tst r0, #0xF0000
    bne _mmu_l2_64k_loop
    b mmu_l2_fill_64k           // load new L1 entry if needed
_mmu_l2_64k_ret:
    bx lr


// L2 = 4K granularity
// r0 = base address
// r1 = length
// r2 = params
mmu_l2_fill_4k:
       mov r4, r0, lsr #20
       add r4, r11, r4, lsl #2     // r4 = L1 table entry ptr
       ldr r5, [r4], #4
       bic r5, r5, #0xFF
       bic r5, r5, #0x300          // r5 = L2 table addr
       mov r6, r0, lsr #12
       and r6, r6, #0xFF            // r6 = L2 table index
       add r5, r5, r6, lsl #2
_mmu_l2_4k_loop:
       orr r3, r0, r2
       str r3, [r5], #4
       add r0, r0, #0x1000
       subs r1, r1, #0x1000
       beq _mmu_l2_4k_ret
       tst r0, #0xFF000
       bne _mmu_l2_4k_loop
       b mmu_l2_fill_4k            // load new L1 entry if needed
_mmu_l2_4k_ret:
       bx lr


.section ".init"

.globl _init
_init:
	bx lr
