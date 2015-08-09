/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(vector0,  T_DIVIDE )  # divide error
TRAPHANDLER_NOEC(vector1,  T_DEBUG  )  # debug exceptions
TRAPHANDLER_NOEC(vector2,  T_NMI    )  # NMI interrupt
TRAPHANDLER_NOEC(vector3,  T_BRKPT  )  # breakpoint exceptions
TRAPHANDLER_NOEC(vector4,  T_OFLOW  )  # overflow exceptions
TRAPHANDLER_NOEC(vector5,  T_BOUND  )  # ROUND range exceed exceptions
TRAPHANDLER_NOEC(vector6,  T_ILLOP  )  # invalid opcode exceptions
TRAPHANDLER_NOEC(vector7,  T_DEVICE )  # device not avaolable exceptions
TRAPHANDLER(     vector8,  T_DBLFLT )  # double fault exceptions
TRAPHANDLER_NOEC(vector9,  T_COPROC )  # coprocessor segments overrun
TRAPHANDLER(     vector10, T_TSS    )  # invalid TSS exceptions
TRAPHANDLER(     vector11, T_SEGNP  )  # segments not present
TRAPHANDLER(     vector12, T_STACK  )  # stack fault exceptions
TRAPHANDLER(     vector13, T_GPFLT  )  # general protection exceptions
TRAPHANDLER(     vector14, T_PGFLT  )  # page fault exceptions
TRAPHANDLER(     vector15, T_RES    )  # reserved
TRAPHANDLER_NOEC(vector16, T_FPERR  )  # FPU error
TRAPHANDLER(     vector17, T_ALIGN  )  # alignment check
TRAPHANDLER_NOEC(vector18, T_MCHK   )  # machine check
TRAPHANDLER_NOEC(vector19, T_SIMDERR)  # SIMD FP exceptions
# 
# 20 ~ 31 reserved
# 
TRAPHANDLER_NOEC(vector_irq_timer, IRQ_OFFSET + IRQ_TIMER)
TRAPHANDLER_NOEC(vector_irq_kbd, IRQ_OFFSET + IRQ_KBD)
TRAPHANDLER_NOEC(vector_irq_2, IRQ_OFFSET + 2)
TRAPHANDLER_NOEC(vector_irq_3, IRQ_OFFSET + 3)
TRAPHANDLER_NOEC(vector_irq_serial, IRQ_OFFSET + IRQ_SERIAL)
TRAPHANDLER_NOEC(vector_irq_5, IRQ_OFFSET + 5)
TRAPHANDLER_NOEC(vector_irq_6, IRQ_OFFSET + 6)
TRAPHANDLER_NOEC(vector_irq_spurious, IRQ_OFFSET + IRQ_SPURIOUS)
TRAPHANDLER_NOEC(vector_irq_8, IRQ_OFFSET + 7)
TRAPHANDLER_NOEC(vector_irq_9, IRQ_OFFSET + 8)
TRAPHANDLER_NOEC(vector_irq_10, IRQ_OFFSET + 9)
TRAPHANDLER_NOEC(vector_irq_11, IRQ_OFFSET + 10)
TRAPHANDLER_NOEC(vector_irq_12, IRQ_OFFSET + 11)
TRAPHANDLER_NOEC(vector_irq_13, IRQ_OFFSET + 12)
TRAPHANDLER_NOEC(vector_irq_ide, IRQ_OFFSET + 13)
TRAPHANDLER_NOEC(vector_irq_15, IRQ_OFFSET + IRQ_IDE)
# 48 system call
TRAPHANDLER_NOEC(vector_syscall, T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */
.globl _alltraps
_alltraps:
	# Build trap frame.
	pushl %ds
	pushl %es
	pushal

	# Set up data and per-cpu segments.
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	# Call trap(tf), where tf=%esp
	pushl %esp
	call trap

.data
.globl vector_table
vector_table:
	.long vector0
	.long vector1
	.long vector2
	.long vector3
	.long vector4
	.long vector5
	.long vector6
	.long vector7
	.long vector8
	.long vector9
	.long vector10
	.long vector11
	.long vector12
	.long vector13
	.long vector14
	.long vector15
	.long vector16
	.long vector17
	.long vector18
	.long vector19
	.fill 12, 4, 0            # 20~31 reserved
	.long vector_irq_timer    # 32 IRQ_OFFSET
	.long vector_irq_kbd
	.long vector_irq_2
	.long vector_irq_3
	.long vector_irq_serial
	.long vector_irq_5
	.long vector_irq_6
	.long vector_irq_spurious
	.long vector_irq_8
	.long vector_irq_9
	.long vector_irq_10
	.long vector_irq_11
	.long vector_irq_12
	.long vector_irq_13
	.long vector_irq_ide
	.long vector_irq_15       # 47 IRQ_OFSSET + 15
	.long vector_syscall      # 48 system call
