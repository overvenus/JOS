#include "ns.h"

#include <inc/lib.h>

# define debug 0
#if debug
static void hexdump(const char *prefix, const void *data, int len);
#endif

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	struct tx_desc td;
	int r;
	while(1) {

		if (! sys_net_tx_table_available())
			continue;    // FULL!

		r = sys_ipc_recv(&nsipcbuf);
		if (r < 0)
			continue;
		if ((thisenv->env_ipc_from != ns_envid) ||
		    (thisenv->env_ipc_value != NSREQ_OUTPUT)) {
			continue;
		}

		memset(&td, 0, sizeof(td));

		if (thisenv->env_ipc_value == NSREQ_OUTPUT) {
			td.addr = (uint32_t)nsipcbuf.pkt.jp_data;
			td.length = nsipcbuf.pkt.jp_len;
			td.cmd = 9;
#if debug
			hexdump("debug output:", (void *)&nsipcbuf.pkt.jp_data, td.length);
#endif
			r = sys_net_try_put_tx_desc(&td, 0);
		}
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
