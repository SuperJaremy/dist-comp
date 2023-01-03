#include "ipc_parent.h"
#include "banking.h"
#include "ipc.h"
#include "ipc_proc.h"
#include <stdlib.h>

struct ipc_parent ipc_parent_init(struct ipc_proc *proc) {
	struct ipc_parent parent = {0};
	parent.ipc_proc = proc;
	parent.history = malloc(sizeof(AllHistory));
	parent.history->s_history_len = 0;
	return parent;
}

void ipc_parent_do_work(struct ipc_parent *parent) {
	timestamp_t time = 0;
	int recieved = 0;
	int neighbours_cnt = parent->ipc_proc->neighbours_cnt;
	AllHistory *history = parent->history;
	Message *m = malloc(sizeof(Message));
	receive_all_started(parent->ipc_proc, &time);

	bank_robbery(parent, parent->ipc_proc->neighbours_cnt);
	m->s_header.s_magic = MESSAGE_MAGIC;
	m->s_header.s_type = STOP;
	m->s_header.s_payload_len = 0;
	m->s_header.s_local_time = get_lamport_time();
	send_multicast(parent->ipc_proc, m);
	receive_all_done(parent->ipc_proc, &time);
	while(recieved < neighbours_cnt) {
		if (receive_any(parent->ipc_proc, m) == 0 && m->s_header.s_type == BALANCE_HISTORY) {
			BalanceHistory *h = (void *)(m->s_payload);
			history->s_history[history->s_history_len++] = *h;
			recieved++;
		}
	}
}

AllHistory *ipc_parent_get_history(struct ipc_parent *parent) {
	return parent->history;
}

void ipc_parent_destroy(struct ipc_parent *parent) {
	free(parent->history);
	ipc_proc_destroy(parent->ipc_proc);
}
