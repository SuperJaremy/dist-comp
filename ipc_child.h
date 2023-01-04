#ifndef _IPC_CHILD_H_
#define _IPC_CHILD_H_

#include "banking.h"
#include "ipc_proc.h"
#include <sys/types.h>

struct ipc_child {
	struct ipc_proc *ipc_proc;
	bool *deffered_replies;
};

struct ipc_child ipc_child_init(struct ipc_proc *proc);

int ipc_child_listen(struct ipc_child *child, pid_t parent, bool mutexl);

void ipc_child_destroy(struct ipc_child *child);

#endif
