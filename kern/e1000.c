#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/string.h>

#include <kern/pmap.h>
#include <kern/time.h>

#define WORK_MODE_BYTES(addr) (*(uint32_t *) \
                                  ((addr) + (E1000_STATUS / sizeof(uint32_t))))

// e should be e1000
#define E1000_REG_ADDR(e, off) (((uintptr_t) e) + (off))

// Number of total tx_desc
#define NTXDESCS 64
// Number of total rx_desc, should be 128-byte aligned.
// see 8254x_GBe_SDM.pdf Section 14.4 Receive Initialization: RDLEN
#define NRXDESCS 256

// MMIO address, virtual
volatile uint32_t *e1000;

// MMIO TDT
volatile uint32_t *e1000_tdt;
// MMIO RDT
volatile uint32_t *e1000_rdt;

// DMA tx_desc table
// MUST aligned on a paragraph (16-byte) boundary.
// see 8254x_GBe_SDM.pdf Section 14.5 Transmit Initialization
__attribute__((__aligned__(16)))
struct tx_desc tx_desc_table[NTXDESCS];
// see 8254x_GBe_SDM.pdf Section 14.4 Receive Initialization
__attribute__((__aligned__(16)))
struct rx_desc rx_desc_table[NRXDESCS];



#define debug 0

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
	for(i = 0; i < 300; i++) {
		r = e1000_82540em_put_tx_desc(&td);

		if (r < 0) {
			if ((++c) > 10){
				break;
			}

			cprintf("FULL!\n");
		}
	}

	// int start = time_msec();
	// int now = start;
	// while (now < start + 1000)  // 1 s
	// 	;
	cprintf("rx_desc_table:\n");
	int ii;
	for(ii = 0; ii < NRXDESCS; ii++) {
		cprintf("  [%3d]'s status: %d\n",ii, rx_desc_table[ii].status);
	}
	cprintf("\n");
	cprintf("e1000_82540em_test end\n\n");
}
#endif

static void
e1000_82540em_init(void)
{
	if (! e1000)
		panic("e1000_82540em_init, MMIO seems wrong!");

	/* TRANSMITTING */
	// 0. Initialize tx_desc_table 
	struct tx_desc td = {
			.addr = (uint64_t)0,
			.length = 0,
			.cso = 0,
			.cmd = 0,
			.status = E1000_TXD_STAT_SHIFT(E1000_TXD_STAT_DD),
			.css = 0,
			.special = 0
		};
	int i;
	for(i = 0; i < NTXDESCS; i++) {
		tx_desc_table[i] = td;
	}

	// 1.A region of memory for the transmit descriptor list.
	physaddr_t tx_table = PADDR(&tx_desc_table);

#if debug
	cprintf(
		"e1000_82540em_init\n"
		"  descs va: 0x%08x\n"
		"  descs va: 0x%08x\n"
		"  e1000: 0x%08x\n",
		&tx_desc_table, tx_table, e1000);
#endif

	// 2.Program the Transmit Descriptor Base Address
	//     (TDBAL/TDBAH) register(s) with the address of the region.
	//     JOS is a 32 bits O/S, TDBAL is sufficient.
	uintptr_t tdbal = E1000_REG_ADDR(e1000, E1000_TDBAL);
	*(uint32_t *)tdbal = tx_table;
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
	tflag |= (0x10) << 4;

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

	/* RECEIVING */
	// 0. Program the Receive Address Register(s) (RAL/RAH) with the
	//    desired Ethernet addresses.
	//
	//    RAL0 and RAH0 always should be used to store the individual
	//    Ethernet MAC address of the Ethernet controller.
	//
	//    QEMU default MAC: 52-54-00-12-34-56
	//
	//    MAC In memory layout: https://tools.ietf.org/html/rfc2469#page-2
	uintptr_t ral0 = E1000_REG_ADDR(e1000, E1000_RA);
	uintptr_t rah0 = ral0 + sizeof(uint32_t);
	*(uint32_t *)ral0 = 0x12005452;
	*(uint32_t *)rah0 = 0x5634 | E1000_RAH_AV;

	// 1. Initialize the MTA (Multicast Table Array) to 0b.
	//    SKIP!

	// 2. Program the Interrupt Mask Set/Read (IMS) register to enable any
	//    interrupt the software driver wants to be notified of
	//    when the event occurs.
	//    SKIP!

	// 3. Allocate a region of memory for the receive descriptor list.
	//    Program the Receive Descriptor Base Address (RDBAL/RDBAH) register(s)
	//    with the address of the region. RDBAL is used for 32-bit addresses
	//    and both RDBAL and RDBAH are used for 64-bit addresses.
	physaddr_t rx_table = PADDR(&rx_desc_table);
	uintptr_t rdbal = E1000_REG_ADDR(e1000, E1000_RDBAL);
	*(uint32_t *)rdbal = rx_table;
	uintptr_t rdbah = E1000_REG_ADDR(e1000, E1000_RDBAH);
	*(uint32_t *)rdbah = 0;
	//     3.1 Initialize tx_desc_table
	memset(rx_desc_table, 0, sizeof(rx_desc_table));
	for (i = 0; i < NRXDESCS; i++) {
		// Allocate memory, phsical address for DMA.
		rx_desc_table[i].addr = page2pa(page_alloc(0));
	}

	// 4. Set the Receive Descriptor Length (RDLEN) register to
	//    the size (in bytes) of the descriptor ring.
	uintptr_t rdlen = E1000_REG_ADDR(e1000, E1000_RDLEN);
	*(uint32_t *)rdlen = sizeof(rx_desc_table);

	// 5. The Receive Descriptor Head and Tail registers
	//    are initialized (by hardware) to 0b
	//
	//    BE CAREFUL!!! rdt = rdh = 0 means the ring is empty, hardware stops
	//                  storing packets in system memory.
	//    see 3.2.6 Receive Descriptor Queue Structure
	uintptr_t rdt = E1000_REG_ADDR(e1000, E1000_RDT);
	*(uint32_t *)rdt = NRXDESCS - 1;
	uintptr_t rdh = E1000_REG_ADDR(e1000, E1000_RDH);
	*(uint32_t *)rdh = 0;
	e1000_rdt = (uint32_t *)rdt;

	// 6. Program the Receive Control (RCTL) register with appropriate values
	uint32_t rflag = 0;
	uintptr_t rctl = E1000_REG_ADDR(e1000, E1000_RCTL);

	//    6.1 Set the receiver Enable (RCTL.EN) bit to 1b for normal operation.
	rflag |= E1000_RCTL_EN;

	//    6.2 Set the Long Packet Enable (RCTL.LPE) bit.
	//        SKIP! Disable.

	//    6.3 Loopback Mode (RCTL.LBM) should be set to 00b for
	//        normal operation.
	rflag &= (~E1000_RCTL_DTYP_MASK);

	//    6.4 Configure the Multicast Offset (RCTL.MO) bits to
	//       the desired value.
	//       SKIP!. Multicast disabled.

	//    6.5 Set the Broadcast Accept Mode (RCTL.BAM) bit to 1b.
	rflag |= E1000_RCTL_BAM;

	//    6.6 Configure the Receive Buffer Size (RCTL.BSIZE) bits.
	//        Mark buffer size 2048 bytes
	rflag |= E1000_RCTL_SZ_2048;

	//    6.7 Set the Strip Ethernet CRC (RCTL.SECRC) bit.
	//        Strip CRC field.
	rflag |= E1000_RCTL_SECRC;

	*(uint32_t *) rctl = rflag;

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
//   index of the tx_desc, >= 0
//   -E_NET_TX_DESC_FULL tx_desc_table is full
//
int
e1000_82540em_put_tx_desc(struct tx_desc *td)
{
	// Full?
	struct tx_desc *tt = &tx_desc_table[*e1000_tdt];
	if (! (tt->status & E1000_TXD_STAT_SHIFT(E1000_TXD_STAT_DD))) {
		return -E_NET_TX_DESC_FULL;    // FULL!
	}

	// Put tx_desc and mark RS
	*tt = *td;
	tt->cmd |= E1000_TXD_TCTL_CMD_SHIFT(E1000_TXD_CMD_RS);

	// Update TDT
	*e1000_tdt = ((*e1000_tdt) + 1) & (NTXDESCS - 1);

	return *e1000_tdt;
}

//
// Got a free entry of tx_desc_table?
//
bool
e1000_82540em_tx_table_available(void)
{
	struct tx_desc *tt = &tx_desc_table[*e1000_tdt];
	if (! (tt->status & E1000_TXD_STAT_SHIFT(E1000_TXD_STAT_DD)))
		return false;    // FULL!
	return true;
}

//
// Read a rx_desc.
// The rd->addr will be updates to the new physical address which is the page
// that already contents some date from network.
//
//   1. Exchange physical address of rd and current RDT
//   2. Unset RDT's DD
//   3. RDT += 1
//
// RETURNS:
//   index of the rx_desc, >= 0
//   -E_NET_RX_DESC_EMPTY if there is no data in receive queue.
//
int
e1000_82540em_read_rx_desc(struct rx_desc *rd)
{
#if debug
	cprintf("In %s, line %d\n", __FILE__, __LINE__);
	cprintf("rx_desc_table:\n");
	int ii;
	for(ii = 0; ii < 15; ii++) {
		cprintf("  [%3d]'s status: %d\n",ii, rx_desc_table[ii].status);
	}
	cprintf("\n");
#endif

	struct rx_desc *rr;

	// i is the index of the frist rx_desc with DD bit under RDT. 
	int i = (*e1000_rdt + 1) & (NRXDESCS - 1);
	if (rx_desc_table[i].status & E1000_RXD_STAT_SHIFT(E1000_RXD_STAT_DD)) {
		if (rx_desc_table[i].status & E1000_RXD_STAT_SHIFT(E1000_RXD_STAT_EOP)) {

			goto receive;
		}
		panic("DO NOT support jumbo frames!");

	} else {

#if debug
		cprintf("No data in receive queue.\n");
#endif
		return -E_NET_RX_DESC_EMPTY;
	}

	receive:

	rr = &rx_desc_table[i];

#if debug
	cprintf("Tail will point to %d\n", i);
#endif

	// Exchange rx_desc and unset DD
	uint64_t pa = rd->addr;
	*rd = *rr;
	rr->addr = pa;
	rr->status = 0;

	// Update RDT
	*e1000_rdt = i;

	return i;
}

//
// Got a free entry of rx_desc_table?
//
bool
e1000_82540em_rx_table_available(void)
{
	struct rx_desc *rr = &rx_desc_table[*e1000_rdt];
	if (! (rr->status & E1000_RXD_STAT_SHIFT(E1000_RXD_STAT_DD)))
		return false;    // FULL!
	return true;
}

bool
e1000_82540em_is_rx_desc_done(int i)
{
	struct rx_desc *rr = &rx_desc_table[i];
	if (! (rr->status & E1000_RXD_STAT_SHIFT(E1000_RXD_STAT_DD)))
		return false;    // FULL!
	return true;
}
