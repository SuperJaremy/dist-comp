#include "ipc_parent.h"
#include "banking.h"
#include "ipc.h"
#include "ipc_proc.h"
#include "pa2345.h"
#include <stdio.h>
#include <stdlib.h>

struct ipc_parent ipc_parent_init(struct ipc_proc *proc) {
	struct ipc_parent parent = {0};
	parent.ipc_proc = proc;
	return parent;
}

void ipc_parent_do_work(struct ipc_parent *parent, FILE *log) {
  timestamp_t time = 0;

  receive_all_started(parent->ipc_proc, &time);
  fprintf(log, log_received_all_started_fmt, time, PARENT_ID);
  fflush(log);

  // receive_all_done(parent->ipc_proc, &time);
  unsigned int neighbours_cnt = parent->ipc_proc->neighbours_cnt;
  local_id i = 1;
  Message *m = malloc(sizeof(Message));
  while (i < neighbours_cnt + 1) {
    int ret;
    while ((ret = receive(parent->ipc_proc, i, m) == NO_READ))
      ;
    if (ret == -1) {
      free(m);
      break;
    } else if (ret == 0) {
      while (time < m->s_header.s_local_time)
        time = get_lamport_time();
      time = get_lamport_time();
      if (m->s_header.s_type == DONE) {
        fprintf(log, "%d: parent received done from process %d\n", time, i);
        fflush(log);
        i++;
      } else {
        fprintf(log, "%d: parent received message from process %d\n", time, i);
        fflush(log);
      }
    }
  }
  free(m);
  fprintf(log, log_received_all_done_fmt, time, PARENT_ID);
  fflush(log);
}

void ipc_parent_destroy(struct ipc_parent *parent) {
	ipc_proc_destroy(parent->ipc_proc);
}
