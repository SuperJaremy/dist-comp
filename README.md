# Lamport's distributed mutual exclusion algorithm
Implementation of Lamport's distributed mutual exclusion algorith based on a library made in task 1.  
Description is available in Russian (original) and English (translated and shorted).
## ru
[Task description in Russian](https://github.com/SuperJaremy/dist-comp/blob/pa4/pa4.pdf)
## en
### Description
Each child process should print to the terminal message in _log_loop_operation_fmt_ (specified in _pa2345.h_) id * 5 times.
### Build
`clang -std=c99 -Wall -pedantic *.c -L. -lruntime`
### Run
```
export LD_LIBRARY_PATH="LD_LIBRARY_PATH:/path/to/pa4/dir";  
### empty line
LD_PRELOAD=/full/path/to/llibruntime.so ./pa4 -p <proc_num> [--mutexl]
```
`--mutexl` - if flag is specified, the child process will enter the critical section before printing to terminal.