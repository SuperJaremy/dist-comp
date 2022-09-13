#include <stdbool.h>

#include "ipc.h"

struct ipc_neighbour;

struct ipc_proc {
  local_id id;
  struct ipc_neighbour *ipc_neighbours;
};

struct ipc_neighbour {
  local_id id;
  int read_pipe_fd;
  int write_pipe_fd;
  struct ipc_neighbour *next;
};

struct ipc_proc ipc_proc_init(local_id id);

void ipc_proc_destroy(struct ipc_proc *ipc_proc);

int ipc_proc_add_neighbour(struct ipc_proc *self, local_id dst, int read_pipe_fd, int write_pipe_fd);
