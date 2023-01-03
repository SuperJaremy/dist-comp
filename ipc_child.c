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
#include "ipc_banking.h"
#include "ipc_proc.h"
#include "pa2345.h"

static int handle_stop(struct ipc_child *child, Message *m, FILE *log);
static int handle_transfer(struct ipc_child *child, Message *m, FILE *log);
static int handle_done(Message *message);

struct ipc_child ipc_child_init(struct ipc_proc *proc, balance_t init_balance) {
  struct ipc_child child = {.ipc_proc = proc, .balance = init_balance};
  child.b_history = balance_history_init(proc->id, init_balance);
  return child;
}

int ipc_child_listen(struct ipc_child *child, pid_t parent) {
  bool done = false;
  char started_msg[128];
  int exit_status = EXIT_SUCCESS;
  int stopped = 0;
  local_id i;
  int neighbours = child->ipc_proc->neighbours_cnt - 1;
  timestamp_t time = get_lamport_time();
  Message *m = NULL;

  sprintf(started_msg, log_started_fmt, time, child->ipc_proc->id, getpid(),
          parent, child->balance);
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
    int ret;
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

  while (!done || (stopped < neighbours)) {
    int ret;
    ret = receive_any(child->ipc_proc, m);
    if (ret < 0) {
      perror("Error while receiving messages\n");
      exit_status = EXIT_FAILURE;
      break;
    } else if (ret == NO_READ) {
      continue;
    } else {
      switch (m->s_header.s_type) {
      case TRANSFER:
        ret = handle_transfer(child, m, log);
        if (ret != 0) {
          exit_status = EXIT_FAILURE;
          goto end;
        }
        break;
      case STOP:
        ret = handle_stop(child, m, log);
        if (ret == 0) {
          done = true;
        } else {
          exit_status = EXIT_FAILURE;
          goto end;
        }
        break;
      case DONE:
        handle_done(m);
        stopped++;
        break;
      default:
        perror("Unknown message type received\n");
        break;
      }
    }
  }

end:
  if (exit_status == EXIT_SUCCESS) {
    time = get_lamport_time();
    balance_history_add_state(child->b_history, time, time, child->balance);
    BalanceHistory *balance_history = child->b_history;
    uint16_t payload = sizeof(balance_history->s_id) + sizeof(balance_history->s_history_len) +
        sizeof(BalanceState) * balance_history->s_history_len;
    fprintf(log, log_received_all_done_fmt, time, child->ipc_proc->id);
    m->s_header.s_type = BALANCE_HISTORY;
    m->s_header.s_local_time = time;
    m->s_header.s_magic = MESSAGE_MAGIC;
    m->s_header.s_payload_len = payload;
    memcpy(m->s_payload, balance_history, payload);
    send(child->ipc_proc, PARENT_ID, m);
  }
  fclose(log);
  free(m);
  return exit_status;
}

void ipc_child_destroy(struct ipc_child *child) {
  ipc_proc_destroy(child->ipc_proc);
  free(child->b_history);
}

static int handle_transfer(struct ipc_child *child, Message *message, FILE *log) {
  int ret;
  timestamp_t time = get_lamport_time();
  while (time <= message->s_header.s_local_time) {
    time = get_lamport_time();
  }
  TransferOrder *order = (TransferOrder *)message->s_payload;
  if (child->ipc_proc->id == order->s_src) {
    balance_t new_balance = child->balance - order->s_amount;
    fprintf(log, log_transfer_out_fmt, time, order->s_src, order->s_amount,
            order->s_dst);
    fflush(log);
    child->balance = new_balance;
    balance_history_add_state(child->b_history, time, time, new_balance);
    time = get_lamport_time();
    message->s_header.s_local_time = time;
    if ((ret = send(child->ipc_proc, order->s_dst, message)) != 0)
      return ret;
  } else if (child->ipc_proc->id == order->s_dst) {
    balance_t new_balance = child->balance + order->s_amount;
    balance_history_add_state(child->b_history, message->s_header.s_local_time - 1, time, new_balance);
    fprintf(log, log_transfer_in_fmt, time, order->s_dst, order->s_amount, order->s_src);
    fflush(log);
    time = get_lamport_time();
    child->balance = new_balance;
    message->s_header.s_local_time = time;
    message->s_header.s_payload_len = 0;
    message->s_header.s_type = ACK;
    if ((ret = send(child->ipc_proc, PARENT_ID, message)) != 0)
      return ret;
  }
  return 0;
}

static int handle_stop(struct ipc_child *child, Message *m, FILE *log) {
  char done_msg[128];
  timestamp_t time;
  int ret = 0;

  time = get_lamport_time();
  while (time <= m->s_header.s_local_time) {
    time = get_lamport_time();
  }
  sprintf(done_msg, log_done_fmt, time, child->ipc_proc->id, child->balance);
  fprintf(log, "%s\n", done_msg);
  fflush(log);
  if (send_done(child->ipc_proc, done_msg, strlen(done_msg), time) !=
      0) {
    perror("Error while sending done\n");
    ret = -1;
  }
  return ret;
}

static int handle_done(Message *message) {
  timestamp_t time = get_lamport_time();
  while (time <= message->s_header.s_local_time) {
    time = get_lamport_time();
  }
  return 0;
}
