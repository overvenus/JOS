#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <inc/stdio.h>

#define debug 1

#if debug
static void
pci_print_func_full(struct pci_func *pcif)
{
	if (! debug)
		return;

	cprintf(
		"struct pci_func {\n"
		"    struct pci_bus *bus;  0x%08x\n"
		"      struct pci_bus *parent_bridge;  0x%08x\n"
		"      uint32_t busno;  0x%08d\n",
		pcif->bus, pcif->bus->parent_bridge, pcif->bus->busno);

	cprintf(
		"    uint32_t dev;  0x%08d\n"
		"    uint32_t func;  0x%08x\n"
		"    uint32_t dev_id;  0x%08x\n"
		"    uint32_t dev_class;  0x%08x\n",
		pcif->dev, pcif->func, pcif->dev_id, pcif->dev_class);

	cprintf(
		"    uint32_t reg_base[6];  0x%08x\n"
		"      uint32_t reg_base[0];  0x%08x\n"
		"      uint32_t reg_base[1];  0x%08x\n"
		"      uint32_t reg_base[2];  0x%08x\n"
		"      uint32_t reg_base[3];  0x%08x\n"
		"      uint32_t reg_base[4];  0x%08x\n"
		"      uint32_t reg_base[5];  0x%08x\n",
		&pcif->reg_base,
		 pcif->reg_base[0],
		 pcif->reg_base[1],
		 pcif->reg_base[2],
		 pcif->reg_base[3],
		 pcif->reg_base[4],
		 pcif->reg_base[5]);

	cprintf(
		"    uint32_t reg_size[6];  0x%08x\n"
		"      uint32_t reg_size[0];  0x%08x\n"
		"      uint32_t reg_size[1];  0x%08x\n"
		"      uint32_t reg_size[2];  0x%08x\n"
		"      uint32_t reg_size[3];  0x%08x\n"
		"      uint32_t reg_size[4];  0x%08x\n"
		"      uint32_t reg_size[5];  0x%08x\n"
		"    uint8_t irq_line;  0x%08x\n"
		"};\n",
		&pcif->reg_size,
		 pcif->reg_size[0],
		 pcif->reg_size[1],
		 pcif->reg_size[2],
		 pcif->reg_size[3],
		 pcif->reg_size[4],
		 pcif->reg_size[5],
		pcif->irq_line);
}
#endif

int
e1000_82540em_pci_attach (struct pci_func *pcif)
{
	cprintf("Hello, this is e1000 driver!\n");

#if debug
	cprintf("Before pci_func_enable(pcif);\n");
	pci_print_func_full(pcif);
#endif

	// enable 82540em.
	pci_func_enable(pcif);

#if debug
	cprintf("After pci_func_enable(pcif);\n");
	pci_print_func_full(pcif);
#endif

	return 0;
}
