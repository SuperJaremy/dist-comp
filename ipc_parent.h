#ifndef _IPC_PARENT_H_
#define _IPC_PARENT_H_
#include "ipc_proc.h"
#include "banking.h"

struct ipc_parent {
	struct ipc_proc *ipc_proc;
	AllHistory *history;
};

struct ipc_parent ipc_parent_init(struct ipc_proc *proc);

void ipc_parent_do_work(struct ipc_parent *parent);

AllHistory *ipc_parent_get_history(struct ipc_parent *parent);

void ipc_parent_destroy(struct ipc_parent *parent);

#endif
