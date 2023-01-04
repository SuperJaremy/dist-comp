#include "ipc_child.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "banking.h"
#include "common.h"
#include "ipc.h"
#include "ipc_proc.h"
#include "pa2345.h"

static int stopped = 0;
static int replied = 0;

static int handle_message(struct ipc_child *ipc_child, local_id from, Message *m, timestamp_t time);
static int handle_cs_request(struct ipc_child *ipc_child, local_id from, Message *m);
static int handle_cs_reply(struct ipc_child *child, Message *m, timestamp_t time);
static int handle_cs_release(struct ipc_child *ipc_child, Message *m);
static int handle_done(Message *message);

struct cs_queue {
  local_id id;
  timestamp_t time;
  struct cs_queue *next;
};

static struct cs_queue *cs_queue_init(local_id id, timestamp_t time) {
  struct cs_queue *cs_q = malloc(sizeof(struct cs_queue));
  cs_q->id = id;
  cs_q->time = time;
  cs_q->next = NULL;
  return cs_q;
}

// static void cs_queue_print(struct ipc_child *child) {
//   struct cs_queue *entry = child->cs_queue;
//   printf("%d: ", child->ipc_proc->id);
//   while (entry) {
//     printf("{%d, %d}, ", entry->id, entry->time);
//     entry = entry->next;
//   }
//   printf("\n");
//   fflush(stdout);
// }

static void cs_queue_add(struct cs_queue **queue, local_id id, timestamp_t time) {
  struct cs_queue *entry = cs_queue_init(id, time);
  struct cs_queue *head = *queue;
  if (!head) {
    *queue = entry;
  } else if (head->time > entry->time || (head->time == entry->time && entry->id < head->id)) {
    entry->next = head;
    *queue = entry;
  } else {
    while (head->next) {
      struct cs_queue *next = head->next;
      if (next->time > entry->time || (next->time == entry->time && entry->id < next->id)) {
        entry->next = next;
        break;
      }
      head = head->next;
    }
    head->next = entry;
  }
}

static int cs_queue_pop(struct cs_queue **queue) {
  if (*queue) {
    struct cs_queue *head = *queue;
    local_id ret = head->id;
    *queue = head->next;
    free(head);
    return ret;
  }
  return -1;
}

static int cs_queue_front(struct cs_queue *queue) {
  if (queue)
    return queue->id;
  return -1;
}

struct ipc_child ipc_child_init(struct ipc_proc *proc) {
  struct ipc_child child = {.ipc_proc = proc, .cs_queue = NULL};
  return child;
}

int ipc_child_listen(struct ipc_child *child, pid_t parent, bool mutexl) {
  char started_msg[128];
  int exit_status = EXIT_SUCCESS;
  local_id i;
  local_id id = child->ipc_proc->id;
  local_id loops = id * 5;
  int neighbours = child->ipc_proc->neighbours_cnt - 1;
  timestamp_t time = get_lamport_time();
  Message *m = NULL;
  int ret;

  sprintf(started_msg, log_started_fmt, time, child->ipc_proc->id, getpid(),
          parent, 0);
  FILE *log = fopen(events_log, "a");

  fprintf(log, "%s\n", started_msg);
  fflush(log);
  if (send_started(child->ipc_proc, started_msg, strlen(started_msg), time) !=
      0) {
    perror("Error while sending started\n");
    exit_status = EXIT_FAILURE;
    goto end;
  }

  m = malloc(sizeof(Message));

  for (i = 1; i <= child->ipc_proc->neighbours_cnt; i++) {
    if (i == child->ipc_proc->id)
      continue;
    time = get_lamport_time();
    while((ret = receive(child->ipc_proc, i, m)) == NO_READ);
    if (ret != 0 || m->s_header.s_type != STARTED) {
      exit_status = EXIT_FAILURE;
      goto end;
    }
  }

  fprintf(log, log_received_all_started_fmt, time, child->ipc_proc->id);
  fflush(log);

  for (i = 0; i < loops; i++) {
    if (mutexl && request_cs(child) != 0) {
      exit_status = EXIT_FAILURE;
      goto end;
    }
    sprintf(started_msg, log_loop_operation_fmt, id, i + 1, loops);
    print(started_msg);
    if (mutexl && release_cs(child) != 0) {
      exit_status = EXIT_FAILURE;
      goto end;
    }
  }

  time = get_lamport_time();
  sprintf(started_msg, log_done_fmt, time, child->ipc_proc->id, 0);
  fprintf(log, "%s\n", started_msg);
  fflush(log);
  if (send_done(child->ipc_proc, started_msg, strlen(started_msg), time) != 0) {
    exit_status = EXIT_FAILURE;
    goto end;
  }
  while (stopped < neighbours) {
    while ((ret = receive_any(child->ipc_proc, m)) == NO_READ);
    if (ret < 0) {
      exit_status = EXIT_FAILURE;
      goto end;
    }
    ret = handle_message(child, ret, m, time);
    if (ret < 0) {
      exit_status = EXIT_FAILURE;
      goto end;
    }
  }

end:
  fclose(log);
  free(m);
  return exit_status;
}

void ipc_child_destroy(struct ipc_child *child) {
  ipc_proc_destroy(child->ipc_proc);
}

int request_cs(const void *self) {
  struct ipc_child *child = (void *)self;
  local_id id = child->ipc_proc->id;
  Message *m = malloc(sizeof(Message));
  timestamp_t time = get_lamport_time();
  int ret;
  int neighbours = child->ipc_proc->neighbours_cnt - 1;
  m->s_header.s_magic = MESSAGE_MAGIC;
  m->s_header.s_type = CS_REQUEST;
  m->s_header.s_local_time = time;
  m->s_header.s_payload_len = 0;
  if ((ret = send_multicast(child->ipc_proc, m)) != 0) {
    free(m);
    return ret;
  }
  while (replied <= neighbours && cs_queue_front(child->cs_queue) != id) {
    while ((ret = receive_any(child->ipc_proc, m)) == NO_READ)
      ;
    if (ret < 0)
      return ret;
    ret = handle_message(child, ret, m, time);
    if (ret < 0)
      return ret;
  }
  replied = 0;
  return 0;
}
int release_cs(const void * self) {
  struct ipc_child *child = (void *)self;
  int ret = 0;
  cs_queue_pop(&(child->cs_queue));
  Message *m = malloc(sizeof(Message));
  m->s_header.s_local_time = get_lamport_time();
  m->s_header.s_magic = MESSAGE_MAGIC;
  m->s_header.s_payload_len = 0;
  m->s_header.s_type = CS_RELEASE;
  ret = send_multicast(child->ipc_proc, m);
  return ret;
}

static int handle_message(struct ipc_child *ipc_child, local_id from, Message *m, timestamp_t time) {
  int ret;
  switch (m->s_header.s_type) {
    timestamp_t _time = get_lamport_time();
    while (_time < m->s_header.s_local_time)
      _time = get_lamport_time();
  case DONE:
    ret = handle_done(m);
    break;
  case CS_REQUEST:
    ret = handle_cs_request(ipc_child, from, m);
    break;
  case CS_REPLY:
    ret = handle_cs_reply(ipc_child, m, time);
    break;
  case CS_RELEASE:
    ret = handle_cs_release(ipc_child, m);
    break;
  }
  return ret;
}

static int handle_done(Message *message) {
  stopped++;
  return 0;
}

static int handle_cs_request(struct ipc_child *ipc_child, local_id from, Message *m) {
  cs_queue_add(&(ipc_child->cs_queue), from, m->s_header.s_local_time);
  // cs_queue_print(ipc_child);
  m->s_header.s_local_time = get_lamport_time();
  m->s_header.s_type = CS_REPLY;
  m->s_header.s_payload_len = 0;
  return send(ipc_child->ipc_proc, from, m);
}

static int handle_cs_reply(struct ipc_child *child, Message *m, timestamp_t time) {
  int neighbours = child->ipc_proc->neighbours_cnt - 1;
  replied++;
  if (replied == neighbours) {
    cs_queue_add(&(child->cs_queue), child->ipc_proc->id, time);
    // cs_queue_print(child);
  }
  return 0;
}

static int handle_cs_release(struct ipc_child *ipc_child, Message *m) {
  cs_queue_pop(&(ipc_child->cs_queue));
  // cs_queue_print(ipc_child);
  return 0;
}
