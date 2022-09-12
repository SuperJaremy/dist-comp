#include "ipc_proc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipc.h"

struct ipc_proc ipc_proc_init(local_id id, unsigned int neighbours_num) {
  int *read_pipes = malloc(sizeof(int) * neighbours_num);
  int *write_pipes = malloc(sizeof(int) * neighbours_num);
  struct ipc_proc proc = {.id = id,
                          .neighbours_num = neighbours_num,
                          .read_pipes = read_pipes,
                          .write_pipes = write_pipes};
  return proc;
}

void ipc_proc_destroy(struct ipc_proc *ipc_proc) {
  free(ipc_proc->read_pipes);
  free(ipc_proc->write_pipes);
  return;
}

int ipc_proc_add_pipe(struct ipc_proc *ipc_proc, local_id dst, int pipefd,
                      bool is_read) {
  local_id src = ipc_proc->id;
  int *pipes = is_read ? ipc_proc->read_pipes : ipc_proc->write_pipes;
  if (src == dst || dst > ipc_proc->neighbours_num)
    return -1;
  else if (src > dst)
    pipes[dst - 1] = pipefd;
  else
    pipes[dst - 2] = pipefd;
  return 0;
}

static int get_pipe_by_local_id(struct ipc_proc *ipc_proc, local_id dst,
                                bool is_read) {
  local_id src = ipc_proc->id;
  int *pipes = is_read ? ipc_proc->read_pipes : ipc_proc->write_pipes;
  if (src == dst || dst > ipc_proc->neighbours_num)
    return -1;
  else if (src > dst)
    return pipes[dst - 1];
  else
    return pipes[dst - 2];
}

int send(void *self, local_id dst, const Message *msg) {
	struct ipc_proc *ipc_proc = (struct ipc_proc *) self;
	size_t write_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;
	int writefd = get_pipe_by_local_id(ipc_proc, dst, false);
	return write(writefd, msg, write_len);
}

int send_multicast(void *self, const Message *msg) {
	
}

int receive(void *self, local_id from, Message *msg) {}

int receive_any(void *self, Message *msg) {}