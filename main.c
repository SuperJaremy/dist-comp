#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ipc_proc.h"

static int get_process_number(int argc, char *argv[]) {
  int opchar;
  int N;
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

int main(int argc, char *argv[]) {
  int N;
  size_t i, j;
  int *read_pipes, *write_pipes;
  N = get_process_number(argc, argv);
  if (N < 1) {
    fprintf(stderr, "Usage: %s -p p_num\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  /*
  * pipe_num = 2 * combination(N+1,2)
  * 2 * combination(N+1,2) = N * (N + 1)
  * read|write_pipe_num = pipe_num / 2
  */
  read_pipes = malloc(sizeof(int) * N * (N+1) / 2);
  write_pipes = malloc(sizeof(int) * N * (N+1) / 2);
  for(i = 0; i < N * (N-1) / 2; i++){
    int pipefd[2];
    if(!pipe(pipefd)){
      perror("Could not allocate pipes");
      exit(EXIT_FAILURE);
    }
    read_pipes[i] = pipefd[0];
    write_pipes[i] = pipefd[1];
  }
  for(i = 0; i < N; i++){
    struct ipc_proc proc = ipc_proc_init(i+1);
    for(j = 0; j < N + 1; j++){
      if(i + 1 == j)
        continue;
      if(j != 0){
        ipc_proc_add_neighbour(&proc, j, read_pipes[i*N+j], write_pipes[i*N + j]);
      }
      else {
        ipc_proc_add_neighbour(&proc, j, -1, write_pipes[i*N + j]);
        close(read_pipes[i*N + j]);
      }
    }
  }
  exit(EXIT_SUCCESS);
}