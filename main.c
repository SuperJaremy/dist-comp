#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ipc.h"
#include "ipc_proc.h"
#include "pa1.h"

#define PIPES_LOG "pipes.log"
#define EVENTS_LOG "events.log"

static int get_process_number(int argc, char *argv[]);

static void do_child_proc();

static void close_all_extra_pipes(local_id id, int process_cnt, int read_pipes[], int write_pipes[]);

static int get_offset_by_neighbour_id(int me, int neighbour){
  return me > neighbour ? neighbour : neighbour - 1;
}

int main(int argc, char *argv[]) {
  int N, working = 0, exit_status = EXIT_SUCCESS, wstatus;
  size_t i, j;
  FILE *pipes_log, *events_log;
  int *read_pipes, *write_pipes;
  struct ipc_proc *procs;
  N = get_process_number(argc, argv);
  if (N < 1) {
    fprintf(stderr, "Usage: %s -p p_num\n", argv[0]);
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
        if(pipe(pipefd)) {
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
  for (i = 1; i < N + 1; i++) {
    struct ipc_proc proc = ipc_proc_init(i);
    procs[i-1] = proc;
    for (j = 0; j < N + 1; j++) {
      if (i == j) continue;
      if (j != 0) {
        int idx = i * N + get_offset_by_neighbour_id(i, j);
        ipc_proc_add_neighbour(&proc, j, read_pipes[idx], write_pipes[idx]);
        fprintf(pipes_log, "P_%d_%d registered (read: %d; write: %d)\n",
                (int)(i), (int)j, read_pipes[idx], write_pipes[idx]);

      } else {
        ipc_proc_add_neighbour(&proc, j, 0, write_pipes[i * N]);
        fprintf(pipes_log, "P_%d_%d registered (read: %d; write: %d)\n",
                (int)(i), (int)j, 0, write_pipes[i * N]);
        close(read_pipes[i]);
      }
    }
  }
  fclose(pipes_log);
  for (i = 0; i < N; i++) {
    pid_t pid, parent;
    parent = getpid();
    pid = fork();
    if (pid == -1) {
      fprintf(stderr, "Could not fork new process\n");
      goto end;
    } else if (pid == 0) {
      close_all_extra_pipes(i + 1, N + 1, read_pipes, write_pipes);
      do_child_proc(parent, procs[i]);
    }
    working++;
  }
end:
  for (i = 0; i < working; i++) {
    wait(&wstatus);
  }
  free(read_pipes);
  free(write_pipes);
  exit(exit_status);
}

static int get_process_number(int argc, char *argv[]) {
  int opchar;
  int N = -1;
  while (-1 != (opchar = getopt(argc, argv, "p:"))) {
    switch (opchar) {
      case 'p': {
        N = atoi(optarg);
        break;
      }
      default:
        return -1;
    }
  }
  return N;
}

static void do_child_proc(pid_t parent, struct ipc_proc proc) {
  char started_msg[128];
  char done_msg[128];
  int exit_status = EXIT_SUCCESS;

  sprintf(started_msg, log_started_fmt, proc.id, getpid(), parent);
  sprintf(done_msg, log_done_fmt, proc.id);
  FILE *log = fopen(EVENTS_LOG, "a");
  fprintf(log, log_started_fmt, proc.id, getpid(), parent);
  fflush(log);
  if (send_started(&proc, started_msg, strlen(started_msg)) != 0) {
    perror("Error while sending started\n");
    exit_status = EXIT_FAILURE;
    goto end;
  }

  if(receive_all_started(&proc) != 0) {
    perror("Error while receiving started\n");
    exit_status = EXIT_FAILURE;
    goto end;
  }
  fprintf(log, log_received_all_started_fmt, proc.id);
  fflush(log);
  
  fprintf(log, log_done_fmt, proc.id);
  fflush(log);
  if (send_done(&proc, started_msg, strlen(started_msg)) != 0) {
    perror("Error while sending done\n");
    exit_status = EXIT_FAILURE;
    goto end;
  }

  if(receive_all_done(&proc) != 0) {
    perror("Error while receiving started\n");
    exit_status = EXIT_FAILURE;
    goto end;
  }

  fprintf(log, log_received_all_done_fmt, proc.id);
  fflush(log);

end:
  ipc_proc_destroy(&proc);
  fclose(log);
  exit(exit_status);
}

static void close_all_extra_pipes(local_id id, int process_cnt, int read_pipes[], int write_pipes[]) {
  for (local_id i = 0; i < process_cnt; i++) {
    if (i != id) {
      for (local_id j = 0; j < process_cnt - 1; j++) {
        close(read_pipes[i*(process_cnt - 1) + j]);
        close(write_pipes[i*(process_cnt - 1) + j]);
      }
    }
  }
}
