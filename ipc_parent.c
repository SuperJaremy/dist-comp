#include "ipc_parent.h"
#include "banking.h"
#include "ipc.h"
#include "ipc_proc.h"
#include <stdlib.h>

struct ipc_parent ipc_parent_init(struct ipc_proc *proc) {
	struct ipc_parent parent = {0};
	parent.ipc_proc = proc;
	return parent;
}

void ipc_parent_do_work(struct ipc_parent *parent) {
	timestamp_t time = 0;
	receive_all_started(parent->ipc_proc, &time);

	receive_all_done(parent->ipc_proc, &time);
}


void ipc_parent_destroy(struct ipc_parent *parent) {
	ipc_proc_destroy(parent->ipc_proc);
}
