#include "ipc_proc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipc.h"

static struct ipc_neighbour *ipc_neighbour_create(local_id id, int read_pipe_fd,
                                                  int write_pipe_fd);

static struct ipc_neighbour *ipc_neighbours_last(
    struct ipc_neighbour *neighbours);

static void ipc_neighbours_add_back(struct ipc_neighbour **neighbours,
                                    struct ipc_neighbour *neighbour);

static void ipc_neighbours_destroy(struct ipc_neighbour *neighbours);

static struct ipc_neighbour *ipc_neighbour_get_by_local_id(
    struct ipc_neighbour *neighbours, local_id dst);

struct ipc_proc ipc_proc_init(local_id id) {
  struct ipc_proc proc = {.id = id, .ipc_neighbours = NULL};
  return proc;
}

void ipc_proc_destroy(struct ipc_proc *ipc_proc) {
  if (!ipc_proc || !ipc_proc->ipc_neighbours) return;
  ipc_neighbours_destroy(ipc_proc->ipc_neighbours);
}

int ipc_proc_add_neighbour(struct ipc_proc *self, local_id dst,
                           int read_pipe_fd, int write_pipe_fd) {
  if (!self) return -1;
  struct ipc_neighbour *neighbour =
      ipc_neighbour_create(dst, read_pipe_fd, write_pipe_fd);
  if (!neighbour) return -1;
  ipc_neighbours_add_back(&(self->ipc_neighbours), neighbour);
  return 0;
}

int send(void *self, local_id dst, const Message *msg) {
  if (!self || !msg) return -1;
  struct ipc_proc *ipc_proc = (struct ipc_proc *)self;
  size_t write_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;
  const char *buf = (void *)msg;
  struct ipc_neighbour *neighbour =
      ipc_neighbour_get_by_local_id(ipc_proc->ipc_neighbours, dst);
  if (!neighbour) return -1;
  int write_pipe_fd = neighbour->write_pipe_fd;
  if (write_pipe_fd < 0) return -1;
  while (write_len) {
    ssize_t xfered = write(write_pipe_fd, buf, write_len);
    if (xfered == -1) return -1;
    write_len -= xfered;
    buf += xfered;
  }
  return 0;
}

int send_multicast(void *self, const Message *msg) {
  if (!self || !msg) return -1;
  struct ipc_proc *ipc_proc = self;
  struct ipc_neighbour *neighbour = ipc_proc->ipc_neighbours;
  while (neighbour) {
    if (!send(self, neighbour->id, msg)) return -1;
    neighbour = neighbour->next;
  }
  return 0;
}

int receive(void *self, local_id from, Message *msg) {
  if (!self || !msg) return -1;
  struct ipc_proc *ipc_proc = (struct ipc_proc *)self;
  size_t read_len = sizeof(MessageHeader);
  char *buf = (void *)msg;
  struct ipc_neighbour *neighbour =
      ipc_neighbour_get_by_local_id(ipc_proc->ipc_neighbours, from);
  if (!neighbour) return -1;
  int read_pipe_fd = neighbour->read_pipe_fd;
  if (read_pipe_fd < 0) return -1;
  while (read_len) {
    ssize_t xfered = read(read_pipe_fd, buf, read_len);
    if (xfered == -1) return -1;
    read_len -= xfered;
    buf += xfered;
  }
  read_len = msg->s_header.s_payload_len;
  buf += sizeof(MessageHeader);
  while (read_len) {
    ssize_t xfered = read(read_pipe_fd, buf, read_len);
    if (xfered == -1) return -1;
    read_len -= xfered;
    buf += xfered;
  }
  return 0;
}

int receive_any(void *self, Message *msg) {
  if (!self || !msg) return -1;
  struct ipc_proc *ipc_proc = (struct ipc_proc *)self;
  struct ipc_neighbour *neighbour = ipc_proc->ipc_neighbours;
  while (neighbour) {
    if (!receive(self, neighbour->id, msg)) return -1;
    neighbour = neighbour->next;
  }
  return 0;
}

static struct ipc_neighbour *ipc_neighbour_create(local_id id, int read_pipe_fd,
                                                  int write_pipe_fd) {
  struct ipc_neighbour *neighbour = malloc(sizeof(struct ipc_neighbour));
  neighbour->id = id;
  neighbour->read_pipe_fd = read_pipe_fd;
  neighbour->write_pipe_fd = write_pipe_fd;
  return neighbour;
}

static void ipc_neighbours_destroy(struct ipc_neighbour *neighbours) {
  struct ipc_neighbour *neighbour = neighbours;
  while (neighbour) {
    struct ipc_neighbour *next = neighbour->next;
    free(neighbour);
    neighbour = next;
  }
}

static void ipc_neighbours_add_back(struct ipc_neighbour **neighbours,
                                    struct ipc_neighbour *neighbour) {
  struct ipc_neighbour *tail = *neighbours;
  if (!tail) {
    *neighbours = neighbour;
    return;
  }
  ipc_neighbours_last(*neighbours)->next = neighbour;
}

static struct ipc_neighbour *ipc_neighbours_last(
    struct ipc_neighbour *neighbours) {
  if (!neighbours) return NULL;
  while (neighbours->next) neighbours = neighbours->next;
  return neighbours;
}

static struct ipc_neighbour *ipc_neighbour_get_by_local_id(
    struct ipc_neighbour *neighbours, local_id id) {
  while (neighbours) {
    if (neighbours->id == id) return neighbours;
    neighbours = neighbours->next;
  }
  return NULL;
}