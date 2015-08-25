#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/string.h>

#include <kern/pmap.h>

#define WORK_MODE_BYTES(addr) (*(uint32_t *) \
                                  ((addr) + (E1000_STATUS / sizeof(uint32_t))))

// e should be e1000
#define E1000_REG_ADDR(e, off) (((uintptr_t) e) + (off))

// Number of total tx_desc
#define NTXDESCS 64

// MMIO address, virtual
volatile uint32_t *e1000;

// MMIO TDT
volatile uint32_t *e1000_tdt;

// DMA tx_desc table
__attribute__((__aligned__(PGSIZE)))
struct tx_desc tx_desc_table[NTXDESCS];


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

static void
e1000_82540em_test()
{
	int c = 0;
	int r = 0;

	cprintf("e1000_82540em_test\n");
	char m[] = "test...";
	cprintf("  m physaddr: 0x%08x\n", PADDR(m));

	struct tx_desc td;
	memset(&td, 0, sizeof(td));
	td.addr = PADDR(m);
	td.cmd |= (E1000_TXD_TCTL_CMD_SHIFT(E1000_TXD_CMD_EOP)
	           | E1000_TXD_TCTL_CMD_SHIFT(E1000_TXD_CMD_RS));
	td.length = sizeof(m);

	uint32_t i;
	for(i = 0; i < 1000; i++) {
		r = e1000_82540em_put_tx_desc(&td);

		if (r < 0) {
			if ((++c) > 10){
				break;
			}

			cprintf("FULL!\n");
		}
	}

	cprintf("e1000_82540em_test end\n\n");
}
#endif

static void
e1000_82540em_init(void)
{
	if (! e1000)
		panic("e1000_82540em_init, MMIO seems wrong!");

	// 1.Allocate a region of memory for the transmit descriptor list.
	memset(&tx_desc_table, 0, sizeof(tx_desc_table));
	physaddr_t descs = PADDR(&tx_desc_table);

#if debug
	cprintf(
		"e1000_82540em_init\n"
		"  descs va: 0x%08x\n"
		"  descs va: 0x%08x\n"
		"  e1000: 0x%08x\n",
		&tx_desc_table, descs, e1000);
#endif

	// 2.Program the Transmit Descriptor Base Address
	//     (TDBAL/TDBAH) register(s) with the address of the region.
	//     JOS is a 32 bits O/S, TDBAL is sufficient.
	uintptr_t tdbal = E1000_REG_ADDR(e1000, E1000_TDBAL);
	*(uint32_t *)tdbal = descs;
	uintptr_t tdbah = E1000_REG_ADDR(e1000, E1000_TDBAH);
	*(uint32_t *)tdbah = 0;

#if debug
	cprintf("  *(uint32_t *)tdbah: 0x%08x\n", *(uint32_t *)tdbal);
#endif

	// 3. Set the Transmit Descriptor Length (TDLEN) register to
	//    the size (in bytes) of the descriptor ring.
	uintptr_t tdlen = E1000_REG_ADDR(e1000, E1000_TDLEN);
	*(uint32_t *)tdlen = sizeof(tx_desc_table);

#if debug
	cprintf(
		"  sizeof tx_desc_table: %d\n"
		"  tx_desc_table entry: %d\n\n",
		sizeof(tx_desc_table), sizeof(tx_desc_table[0]));
#endif

	// 4. The Transmit Descriptor Head and Tail (TDH/TDT) registers
	uintptr_t tdh = E1000_REG_ADDR(e1000, E1000_TDH);
	*(uint32_t *)tdh = 0;
	uintptr_t tdt = E1000_REG_ADDR(e1000, E1000_TDT);
	*(uint32_t *)tdt = 0;
	e1000_tdt = (uint32_t *)tdt;

	// 5. Initialize the Transmit Control Register (TCTL) for desired operation
	uint32_t tflag = 0;
	uintptr_t tctl = E1000_REG_ADDR(e1000, E1000_TCTL);

	//    5.1 Set the Enable (TCTL.EN) bit to 1b for normal operation.
	tflag |= E1000_TCTL_EN;

	//    5.2 Set the Pad Short Packets (TCTL.PSP) bit to 1b.
	tflag |= E1000_TCTL_PSP;

	//    5.3 Configure the Collision Threshold (TCTL.CT) to the desired value.
	//        SKIP!

	//    5.4 Configure the Collision Distance (TCTL.COLD) to its expected value.
	//        assume full-duplex operation.
	tflag |= 0x40 << 12;
	*(uint32_t *)tctl = tflag;

	// 6. Program the Transmit IPG (TIPG) register for IEEE 802.3
	uint32_t tpg = 0;
	uintptr_t tipg = E1000_REG_ADDR(e1000, E1000_TIPG);
	tpg = 10;
	tpg |= 4 << 10;
	tpg |= 6 << 20;
	tpg &= 0x3FFFFFFF;
	*(uint32_t *)tipg = tpg;
}

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

	e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	e1000_82540em_status();

	e1000_82540em_init();

#if debug
	e1000_82540em_test();
#endif

	return 0;
}

uint32_t
e1000_82540em_status(void)
{
#if debug
	cprintf("82540em work mode: 0x%08x\n", WORK_MODE_BYTES(e1000));
#endif

	if (! e1000)
		panic("e1000_82540em_status, MMIO seems wrong!");
	return WORK_MODE_BYTES(e1000);
}

//
// Put a tx_desc.
//
// RETURNS:
//   0 on success
//   -E_TX_DESC_FULL tx_desc_table is full
//
int
e1000_82540em_put_tx_desc(struct tx_desc *td)
{
	// Full?
	struct tx_desc *tt = &tx_desc_table[*e1000_tdt];
	if (tt->status & E1000_TXD_TCTL_STAT_SHIFT(E1000_TXD_STAT_DD)) {
		return -E_TX_DESC_FULL;    // FULL!
	}

	// Put tx_desc and mark RS
	*tt = *td;
	tt->cmd |= E1000_TXD_TCTL_CMD_SHIFT(E1000_TXD_CMD_RS);

	// Update TDT
	*e1000_tdt = ((*e1000_tdt) + 1) & (NTXDESCS - 1);

	return 0;
}

//
// Try put tx_desc till success.
//
// RETURNS:
//   0 on success
//   < 0 on fail
//
int
e1000_82540em_try_put_tx_desc(struct tx_desc *td)
{
	int r = 1;
	while (r) {
		// if (r == -E_TX_DESC_FULL)
		// 	sched_yield();

		r = e1000_82540em_put_tx_desc(td);
	}
	return 0;
}