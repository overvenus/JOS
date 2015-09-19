#include "ns.h"

#include <inc/lib.h>

# define debug 0
#if debug
static void hexdump(const char *prefix, const void *data, int len);
#endif

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int r;
	while(1) {

		if (! sys_net_rx_table_available())
			continue;    // FULL!

		// Alloc a new page at nsipcbuf
		r = sys_page_alloc(0, &nsipcbuf, PTE_U| PTE_W| PTE_P);
		if (r < 0) {
			panic("input, %e", r);
		}

		// Cook up a new rx_desc
		struct rx_desc rd = {0, 0, 0, 0, 0, 0};
		rd.addr = (uintptr_t)&nsipcbuf.pkt.jp_data;

		// 0 guarantees read success.
		r = sys_net_try_read_rx_desc(&rd, 0);
		if (r < 0)
			panic("input: %e", r);
		nsipcbuf.pkt.jp_len = rd.length;
#if debug
		cprintf("rd.addr: %08x\n", rd.addr);
		cprintf("&nsipcbuf.pkt.jp_data: %08x\n", &nsipcbuf.pkt.jp_data);
		cprintf("nsipcbuf.pkt.jp_len: %d\n", nsipcbuf.pkt.jp_len);
		hexdump("debug:", (void *)&nsipcbuf.pkt.jp_data, rd.length);
		cprintf("In %s, line %d\n", __FILE__, __LINE__);
#endif
		again:
		r = sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf,
		                     PTE_P| PTE_W| PTE_U);
#if debug
		cprintf("In %s, line %d\n", __FILE__, __LINE__);
#endif
		if (r < 0) {
			if (r == -E_IPC_NOT_RECV) {
				sys_yield();
				goto again;
			}
			panic("input, %e", r);
		}
#if debug
		cprintf("In %s, line %d\n", __FILE__, __LINE__);
#endif
	}
}

#if debug
static void
hexdump(const char *prefix, const void *data, int len)
{
	int i;
	char buf[80];
	char *end = buf + sizeof(buf);
	char *out = NULL;
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			out = buf + snprintf(buf, end - buf,
					     "%s%04x   ", prefix, i);
		out += snprintf(out, end - out, "%02x", ((uint8_t*)data)[i]);
		if (i % 16 == 15 || i == len - 1)
			cprintf("%.*s\n", out - buf, buf);
		if (i % 2 == 1)
			*(out++) = ' ';
		if (i % 16 == 7)
			*(out++) = ' ';
	}
}
#endif
