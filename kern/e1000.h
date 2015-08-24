#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <kern/e1000_hw.h>

int e1000_82540em_pci_attach(struct pci_func *pcif);
uint32_t e1000_82540em_status();

#endif	// JOS_KERN_E1000_H
