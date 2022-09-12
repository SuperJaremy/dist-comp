#include "ipc.h"
#include <stdbool.h>
struct ipc_proc {
  local_id id;
  unsigned int neighbours_num;
  int *read_pipes;
  int *write_pipes;
};

struct ipc_proc ipc_proc_init(local_id id, unsigned int neighbours_num);

void ipc_proc_destroy(struct ipc_proc *ipc_proc);

int ipc_proc_add_pipe(struct ipc_proc *ipc_proc, local_id dst, int pipefd,
                      bool is_read);