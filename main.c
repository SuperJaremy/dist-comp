#define _GNU_SOURCE
#include <getopt.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "banking.h"
#include "ipc.h"
#include "ipc_parent.h"
#include "ipc_proc.h"
#include "ipc_child.h"
#include "pa2345.h"

#define PIPES_LOG "pipes.log"
#define EVENTS_LOG "events.log"

static int get_process_number(int argc, char *argv[], bool *mutexl);

static void do_child_proc(pid_t parent, struct ipc_proc *proc, bool mutexl, FILE *log);

static void close_all_extra_pipes(local_id id, int process_cnt, int read_pipes[], int write_pipes[]);

static int get_offset_by_neighbour_id(int me, int neighbour){
  return me > neighbour ? neighbour : neighbour - 1;
}

int main(int argc, char *argv[]) {
  int N, working = 0, exit_status = EXIT_SUCCESS, wstatus;
  size_t i, j;
  FILE *pipes_log, *events_log;
  int *read_pipes, *write_pipes;
  struct ipc_proc parent_proc;
  struct ipc_proc *procs;
  bool mutexl = false;
  struct ipc_parent ipc_parent;
  N = get_process_number(argc, argv, &mutexl);
  if (N < 1) {
    fprintf(stderr, "Usage: %s -p p_num p_balance...\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  events_log = fopen(EVENTS_LOG, "w");
  fclose(events_log);
  read_pipes = malloc(sizeof(int) * N * (N + 1));
  write_pipes = malloc(sizeof(int) * N * (N + 1));
  procs = malloc(sizeof(struct ipc_proc) * N);
  for(i = 0; i < N + 1; i++){
    for(j = 0; j < N + 1; j++){
      if(i == j) {
        continue;
      } else {
        int pipefd[2];
        if(pipe2(pipefd, O_NONBLOCK)) {
          perror("Could not allocate pipes\n");
          exit(EXIT_FAILURE);
        }
        write_pipes[i * N + get_offset_by_neighbour_id(i, j)] = pipefd[1];
        read_pipes[j * N + get_offset_by_neighbour_id(j, i)] = pipefd[0];
      }
    }
  }
  pipes_log = fopen(PIPES_LOG, "w");
  if (!pipes_log) {
    exit_status = EXIT_FAILURE;
    goto end;
  }
  for (i = 0; i < N + 1; i++) {
    struct ipc_proc proc = ipc_proc_init(i);
    for (j = 0; j < N + 1; j++) {
      if (i == j) continue;
      int idx = i * N + get_offset_by_neighbour_id(i, j);
      ipc_proc_add_neighbour(&proc, j, read_pipes[idx], write_pipes[idx]);
      fprintf(pipes_log, "P_%d_%d registered (read: %d; write: %d)\n", (int)(i),
              (int)j, read_pipes[idx], write_pipes[idx]);
    }
    if (i == 0)
      parent_proc = proc;
    else
      procs[i - 1] = proc;
  }
  fclose(pipes_log);
  events_log = fopen(EVENTS_LOG, "a");
  for (i = 0; i < N; i++) {
    pid_t pid, parent;
    parent = getpid();
    pid = fork();
    if (pid == -1) {
      fprintf(stderr, "Could not fork new process\n");
      goto end;
    } else if (pid == 0) {
      close_all_extra_pipes(i + 1, N + 1, read_pipes, write_pipes);
      do_child_proc(parent, &procs[i], mutexl, events_log);
    }
    working++;
  }
  close_all_extra_pipes(0, N + 1, read_pipes, write_pipes);
  ipc_parent = ipc_parent_init(&parent_proc);
  ipc_parent_do_work(&ipc_parent, events_log);
end:
  for (i = 0; i < working; i++) {
    wait(&wstatus);
  }
  fclose(events_log);
  ipc_parent_destroy(&ipc_parent);
  free(read_pipes);
  free(write_pipes);
  exit(exit_status);
}

static int get_process_number(int argc, char *argv[], bool *mutexl) {
  int opchar;
  int N = -1;
  struct option long_option[] = {
    {"mutexl", no_argument, 0, 'm'},
    {0       , 0          , 0, 0}
  };
  while (-1 != (opchar = getopt_long(argc, argv, "p:", long_option, NULL))) {
    switch (opchar) {
      case 'p': {
        N = atoi(optarg);
        break;
      }
    case 'm': 
      *mutexl = true;
      break;
    default:
        return -1;
    }
  }
  return N;
}

static void do_child_proc(pid_t parent, struct ipc_proc *proc, bool mutexl, FILE *log) {
  int exit_status;
  struct ipc_child child = ipc_child_init(proc);
  exit_status = ipc_child_listen(&child, parent, mutexl, log);
  ipc_child_destroy(&child);
  exit(exit_status);
}

static void close_all_extra_pipes(local_id id, int process_cnt, int read_pipes[], int write_pipes[]) {
  for (local_id i = 0; i < process_cnt; i++) {
    if (i != id) {
      for (local_id j = 0; j < process_cnt - 1; j++) {
        size_t idx = i * (process_cnt - 1) + j;
        close(read_pipes[idx]);
        close(write_pipes[idx]);
      }
    }
  }
}
