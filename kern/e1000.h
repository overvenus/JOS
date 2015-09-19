#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <inc/nete1000.h>

#include <kern/pci.h>
#include <kern/e1000_hw.h>

#define E1000_TXD_TCTL_CMD_SHIFT(x) ((x) >> 24)
#define E1000_TXD_STAT_SHIFT(x) (x)
#define E1000_RXD_STAT_SHIFT(x) (x)

int e1000_82540em_pci_attach(struct pci_func *pcif);
uint32_t e1000_82540em_status(void);
int e1000_82540em_put_tx_desc(struct tx_desc *td);
bool e1000_82540em_tx_table_available(void);
int e1000_82540em_read_rx_desc(struct rx_desc *rd);
bool e1000_82540em_rx_table_available(void);
bool e1000_82540em_is_rx_desc_done(int i);

#endif	// JOS_KERN_E1000_H
