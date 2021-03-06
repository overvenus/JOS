// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ 
		"backtrace",
		"Backtrace from current function to the end of init()",
		mon_backtrace
	},
	{
		"showmappings",
		"Display the physical page mappings and corresponding"
		"permission bits that\napply to the pages",
		mon_show_mappings
	},
	{
		"smp",
		"Display the physical page mappings and corresponding"
		"permission bits that\napply to the pages",
		mon_show_mappings
	},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// http://unixwiz.net/techtips/win32-callconv-asm.html
	// Calling a __cdecl function

	cprintf("Stack backtrace:\n");
	// current ebp
	uint32_t ebp = read_ebp();
	// old ebp address
	uint32_t old_ebp = *(uint32_t *) ebp;

	// cuurent return address
	uint32_t ret = *(((uint32_t *) ebp) + 1);
	// old return address
	uint32_t old_ret = *(((uint32_t *)old_ebp) + 1);

	uint32_t args[5];

	struct Eipdebuginfo info;
	int i;
	while (ebp != 0x0) {
		for (i = 0; i < 5; i++) {
			args[i] = *((uint32_t *) ebp + i + 2);
		}
		debuginfo_eip(ret, &info);
		// ebp f010ff78  eip f01008ae  args 00000001 f010ff8c 00000000 f0110580 00000000
		//      kern/monitor.c:143: monitor+106
		cprintf("  ebp %x eip %x args %08x %08x %08x %08x %08x\n"  // no ,
				"         %s:%d: %.*s+%d\n",
				ebp, ret, args[0], args[1], args[2], args[3], args[4],
				info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, (ret-info.eip_fn_addr)
		);
		ebp = old_ebp;
		old_ebp = *(uint32_t *) ebp;
		ret = old_ret;
		old_ret = *(((uint32_t *) old_ebp) + 1);

	}
	return 0;
}

int
mon_show_mappings(int argc, char **argv, struct Trapframe *tf)
{
	// TODO: complete this function, `strtol` might be useful.
	extern pde_t *kern_pgdir;
	cprintf("kern_pgdir: 0x%08x\n", kern_pgdir);
	int i;
	uintptr_t a;
	pde_t pd, pt;
	for (i = 1; i < argc; i++) {
		// ignore the first arg.
		a = (uintptr_t) strtol(argv[i], NULL, 16);
		pd = kern_pgdir[a >> 22];
		cprintf("pd: 0x%08x\n", pd);
		pt = ((pde_t *)pd)[(a >> 12) & 0x3FF];
		cprintf("pt: 0x%08x\n", pt);

		cprintf("address %s maps to 0x%08x\n",
		  argv[i],
		  (pt & 0xFFFFF000) | (a & 0x00000FFF));
	}
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
