#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
  N = get_process_number(argc, argv);
  if (N < 1) {
    fprintf(stderr, "Usage: %s -p p_num\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  printf("Process number: %d\n", N);
  exit(EXIT_SUCCESS);
}