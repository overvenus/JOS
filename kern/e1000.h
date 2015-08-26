#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <inc/nete1000.h>

#include <kern/pci.h>
#include <kern/e1000_hw.h>

#define E1000_TXD_TCTL_CMD_SHIFT(x) ((x) >> 24)
#define E1000_TXD_TCTL_STAT_SHIFT(x) (x)

int e1000_82540em_pci_attach(struct pci_func *pcif);
uint32_t e1000_82540em_status(void);
int e1000_82540em_put_tx_desc(struct tx_desc *td);

#endif	// JOS_KERN_E1000_H
