#include "ns.h"

#include <inc/lib.h>

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
	struct rx_desc rd;
	int r;
	while(1) {

		if (! sys_net_rx_table_available())
			continue;    // FULL!

		r = sys_ipc_recv(&nsipcbuf);
		if (r < 0)
			continue;

		memset(&rd, 0, sizeof(rd));

		if (thisenv->env_ipc_value == NSREQ_INPUT) {
			rd.addr = (uint32_t)nsipcbuf.pkt.jp_data;
			rd.length = nsipcbuf.pkt.jp_len;

			r = sys_net_try_put_rx_desc(&rd, 0);
		}
	}
}
