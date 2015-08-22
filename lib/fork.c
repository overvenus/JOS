// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

// PTE premission
#define PTE_T_PERM(p) (((uintptr_t) p) & 0xFFF)

// Like `pgdir_walk()` in pmap.c, but simpler.
static int
pgdir_look(pde_t *upgdir, pte_t *upgtabs, const void *va, pte_t **pte)
{
	// Get page table entry
	if (! (upgdir[PDX(va)] & PTE_P))
		// Not present!
		return -E_INVAL;

	// Get page entry
	if (! (upgtabs[PGNUM(va)] & PTE_P))
		// Not present!
		return -E_INVAL;

	*pte = &upgtabs[PGNUM(va)];
	return 0;
}

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	// if ((err & FEC_WR) != FEC_WR) {
	// 	panic("User pagefault! Unmendable! addr: 0x%08x", addr);
	// }

	pte_t *p;
	r = pgdir_look((pde_t *)uvpd,(pte_t *)uvpt, addr, &p);
	if (r < 0)
		panic("User pagefault! Not present! addr: 0x%08x", addr);

	if (! (PTE_T_PERM(*p) & PTE_COW))
		panic("User pagefault! unmendable! No COW marked! addr: 0x%08x", addr);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.

	// system call #1
	// Allocate a temporary page
	r = sys_page_alloc(0, PFTEMP, PTE_U|PTE_W|PTE_P);
	if (r < 0)
		panic("User pagefault! %e", r);

	// Copy COW page to temporary page.
	addr = ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, addr, PGSIZE);

	// system call #2
	// Map the temporary page to fault address.
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_W|PTE_U|PTE_P);

	if (r < 0)
		panic("User pagefault! %e", r);

	// system call #3
	// Unmap PFTEMP
	r = sys_page_unmap(0, PFTEMP);
	if (r < 0)
		panic("User pagefault! %e", r);

	return;
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *addr = (void *)(pn * PGSIZE);

	pte_t *p;
	r = pgdir_look((pde_t *)uvpd,(pte_t *)uvpt, addr, &p);
	if (r < 0)
		panic("duppage error %e", r);

	uint32_t perm = PTE_T_PERM(*p);
	if (perm & PTE_SHARE) {
		perm |= PTE_P | PTE_SHARE;
	} else if (perm & (PTE_COW | PTE_W)) {
		// Mark writable and copy-on-write page as COW
		perm &= ~PTE_W;
		perm |= PTE_COW | PTE_P;
	}

	// Map to child
	r = sys_page_map(0, addr, envid, addr, perm);
	if (r < 0)
		panic("duppage error %e", r);

	// Remap to itself
	r = sys_page_map(0, addr, 0, addr, perm);
	if (r < 0)
		panic("duppage error %e", r);

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
//   uvpd and uvpt are defined in lib/entry.S and memlayout.h
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;

	// Allocate a child Env
	envid_t eid = sys_exofork();
	if (eid < 0) {
		r = eid;
		goto fail;
	}

	// exofork return 0 to child
	if (eid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		goto good;
	}

	// Allocate a new page for the child's user exception stack. 
	r = sys_page_alloc(eid,
			(void*)(UXSTACKTOP-PGSIZE),
			PTE_U|PTE_P|PTE_W);
	if (r < 0)
		goto bad;

	// Install pgfault_handler
	// MUST install handler before duppage,
	// because we make every writable page as read-only.
	set_pgfault_handler(pgfault);

	// Address space
	// UTOP is UXSTACKTOP, we have maped a page for child UXSTACK
	// so, duppage 0 ~ USTACKTOP is enough. 
	uintptr_t va; 
	for (va = 0; va < USTACKTOP; va += PGSIZE) {
		if ( ! uvpd[PDX(va)] & PTE_P)
			continue;

		if (! (uvpt[PGNUM(va)] & PTE_P))
			continue;

		r = duppage(eid, PGNUM(va));
		if (r < 0)
			goto bad;
	}

	// Install assembly language pgfault entrypoint 
	// MUST call after duppage, inside the system call,
	// there is a user_mem_assert() which will walk
	// envid's page table.
	extern void _pgfault_upcall(void);
	r = sys_env_set_pgfault_upcall(eid, (void*)_pgfault_upcall);
	if (r < 0)
		goto bad;

	// Run, Forrest, run!
	r = sys_env_set_status(eid, ENV_RUNNABLE);
	if (r < 0)
		goto bad;

	good:
	return eid;

	bad:
	sys_env_destroy(eid);

	fail:
	panic("fork failed! %e", r);
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
