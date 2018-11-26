
#include <nuttx/config.h>
#include <arch/irq.h>



#define MINERVA_STACK_BASE    _ebss
#define MINERVA_STACK_TOP     _ebss+CONFIG_IDLETHREAD_STACKSIZE
#define MINERVA_HEAP_BASE     MINERVA_STACK_TOP


#define MIE_MEIE 	0x800
#define CSR_IRQ_MASK 0x330
	
	.section	.text, "ax", @progbits
	.global		g_idle_topstack
	.global		__start

__start:
_reset_handler:
	la sp, MINERVA_STACK_TOP
	la t0, trap_entry
	csrw mtvec, t0

	// initialize .bss
	la t0, _sbss
	la t1, _ebss
1:	beq t0, t1, 2f
	sw zero, 0(t0)
	addi t0, t0, 4
	j 1b
2:
	// enable external interrupts
	//li t0, MIE_MEIE
	//csrs mie, t0

	call os_start
1:	j 1b

trap_entry:
	csrw mscratch, ra
	call save_ctx
	mv a0, sp
	call minerva_doexception
	j restore_ctx_and_mret


save_ctx:
	csrrw ra, mscratch, ra

	addi sp, sp, -37*4
	sw zero, 0*4(sp)
	sw ra,   1*4(sp)
	sw gp,   3*4(sp)
	sw tp,   4*4(sp)
	sw t0,   5*4(sp)
	sw t1,   6*4(sp)
	sw t2,   7*4(sp)
	sw s0,   8*4(sp)
	sw s1,   9*4(sp)
	sw a0,  10*4(sp)
	sw a1,  11*4(sp)
	sw a2,  12*4(sp)
	sw a3,  13*4(sp)
	sw a4,  14*4(sp)
	sw a5,  15*4(sp)
	sw a6,  16*4(sp)
	sw a7,  17*4(sp)
	sw s2,  18*4(sp)
	sw s3,  19*4(sp)
	sw s4,  20*4(sp)
	sw s5,  21*4(sp)
	sw s6,  22*4(sp)
	sw s7,  23*4(sp)
	sw s8,  24*4(sp)
	sw s9,  25*4(sp)
	sw s10, 26*4(sp)
	sw s11, 27*4(sp)
	sw t3,  28*4(sp)
	sw t4,  29*4(sp)
	sw t5,  30*4(sp)
	sw t6,  31*4(sp)

	csrr t0, mstatus
	csrr t1, mepc
	csrr t2, mbadaddr
	csrr t3, mcause
	csrr t4, CSR_IRQ_MASK
	
	sw t0, 32*4(sp)
	sw t1, 33*4(sp)
	sw t2, 34*4(sp)
	sw t3, 35*4(sp)
	sw t4, 36*4(sp)
	
	csrrw ra, mscratch, ra
	ret

restore_ctx_and_mret:
	lw ra,   REG_X1(a0)
	lw sp,   REG_X2(a0)
	lw gp,   REG_X3(a0)
	lw tp,   REG_X4(a0)
	lw t1,   REG_X6(a0)
	lw t2,   REG_X7(a0)
	lw s0,   REG_X8(sp)
	lw s1,   REG_X9(sp)
	lw a1,  REG_X11(sp)
	lw a2,  REG_X12(sp)
	lw a3,  REG_X13(sp)
	lw a4,  REG_X14(sp)
	lw a5,  REG_X15(sp)
	lw a6,  REG_X16(sp)
	lw a7,  REG_X17(sp)
	lw s2,  REG_X18(sp)
	lw s3,  REG_X19(sp)
	lw s4,  REG_X20(sp)
	lw s5,  REG_X21(sp)
	lw s6,  REG_X22(sp)
	lw s7,  REG_X23(sp)
	lw s8,  REG_X24(sp)
	lw s9,  REG_X25(sp)
	lw s10, REG_X26(sp)
	lw s11, REG_X27(sp)
	lw t3,  REG_X28(sp)
	lw t4,  REG_X29(sp)
	lw t5,  REG_X30(sp)
	lw t6,  REG_X31(sp)

	lw t0, REG_INT_CTX(a0)
	csrw CSR_IRQ_MASK, t0
	lw t0, REG_CSR_MEPC(a0)
	csrw mepc, t0
	lw t0, REG_CSR_MSTATUS(a0)
	csrw mstatus, t0

	lw t0,   REG_X5(a0)
	lw a0,  REG_X10(sp)
	
	mret


g_idle_topstack:
	.long	MINERVA_STACK_TOP
	.size	g_idle_topstack, .-g_idle_topstack
	.end
