#include "ipc_child.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
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

static int handle_message(struct ipc_child *ipc_child, local_id from, Message *m, timestamp_t req_time);
static int handle_cs_request(struct ipc_child *ipc_child, local_id from, Message *m, timestamp_t req_time);
static int handle_cs_reply(struct ipc_child *child, Message *m);
static int handle_done(Message *message);

struct ipc_child ipc_child_init(struct ipc_proc *proc) {
  size_t i;
  unsigned int replies = proc->neighbours_cnt + 1;
  struct ipc_child child = {0};
  child.ipc_proc = proc;
  child.deffered_replies = malloc(sizeof(bool) * replies);
  for (i = 0; i < replies; i++)
    child.deffered_replies[i] = false;
  return child;
}

int ipc_child_listen(struct ipc_child *child, pid_t parent, bool mutexl, FILE *log) {
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
    ret = handle_message(child, ret, m, -1);
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
  free(child->deffered_replies);
  ipc_proc_destroy(child->ipc_proc);
}

int request_cs(const void *self) {
  struct ipc_child *child = (void *)self;
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
  while (replied < neighbours) {
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
  size_t i;
  int ret = 0;
  Message *m = malloc(sizeof(Message));
  bool *deffered_replies = child->deffered_replies;
  unsigned int procs = child->ipc_proc->neighbours_cnt + 1;
  for (i = 0; i < procs; i++)
    if(deffered_replies[i]) {
      deffered_replies[i] = false;
      m->s_header.s_magic = MESSAGE_MAGIC;
      m->s_header.s_local_time = get_lamport_time();
      m->s_header.s_payload_len = 0;
      m->s_header.s_type = CS_REPLY;
      if ((ret = send(child->ipc_proc, i, m)) < 0)
        break;
    }
  free(m);
  return ret;
}

static int handle_message(struct ipc_child *ipc_child, local_id from, Message *m, timestamp_t req_time) {
  int ret;
  switch (m->s_header.s_type) {
    timestamp_t _time = get_lamport_time();
    while (_time < m->s_header.s_local_time)
      _time = get_lamport_time();
  case DONE:
    ret = handle_done(m);
    break;
  case CS_REQUEST:
    ret = handle_cs_request(ipc_child, from, m, req_time);
    break;
  case CS_REPLY:
    ret = handle_cs_reply(ipc_child, m);
    break;
  }
  return ret;
}

static int handle_done(Message *message) {
  stopped++;
  return 0;
}

static int handle_cs_request(struct ipc_child *ipc_child, local_id from,
                             Message *m, timestamp_t req_time) {
  timestamp_t m_time = m->s_header.s_local_time;
  local_id my_id = ipc_child->ipc_proc->id;
  if (req_time >= 0 && (req_time < m_time || (req_time == m_time && my_id < from))) {
    ipc_child->deffered_replies[from] = true;
    return 0;
  } else {
    m->s_header.s_local_time = get_lamport_time();
    m->s_header.s_type = CS_REPLY;
    m->s_header.s_payload_len = 0;
    return send(ipc_child->ipc_proc, from, m);
  }
}

static int handle_cs_reply(struct ipc_child *child, Message *m) {
  replied++;
  return 0;
}
