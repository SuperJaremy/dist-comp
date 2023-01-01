#ifndef _IPC_PROC_H_
#define _IPC_PROC_H_

#include <stdbool.h>
#include <stddef.h>

#include "ipc.h"

#define NO_READ 2

struct ipc_neighbour;

struct message_queue;

struct ipc_proc {
  local_id id;
  struct ipc_neighbour *ipc_neighbours;
  unsigned int neighbours_cnt;
  struct message_queue *message_queue;
};

struct ipc_neighbour {
  local_id id;
  int read_pipe_fd;
  int write_pipe_fd;
  struct ipc_neighbour *next;
};

struct message_queue {
  Message *message;
  struct message_queue *next;
};

struct ipc_proc ipc_proc_init(local_id id);

void ipc_proc_destroy(struct ipc_proc *ipc_proc);

int ipc_proc_add_neighbour(struct ipc_proc *self, local_id dst, int read_pipe_fd, int write_pipe_fd);

int send_started(struct ipc_proc *me, const char* msg, size_t msg_size, timestamp_t time);

int send_done(struct ipc_proc *me, const char* msg, size_t msg_size, timestamp_t time);

int receive_all_started(struct ipc_proc *me, bool is_child);

int receive_all_done(struct ipc_proc *me);

Message *message_queue_pop(struct ipc_proc *ipc_proc);

#endif
