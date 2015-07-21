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
# 20 ~ 31 reserved
TRAPHANDLER_NOEC(vector_syscall, T_SYSCALL)  # 48 system call
# 32 ~ 255 user defined interrupts


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
	.fill 12, 4, 0  # 20~31 reserved
	.fill 16, 4, 0  # 32~47 empty
	.long vector_syscall  # 48
