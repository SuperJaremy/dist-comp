# Distributed Architecture

Library for interprocess communication.   
Description is available in Russian (original) and English (translated and shorted).

## ru
[Task description in Russian](https://github.com/SuperJaremy/dist-comp/blob/pa1/pa1.pdf)
## en

### Description

Interporcess communcation should be implemented using Linux pipes. Once parent process is started, it should create several child processes (1 to 10) by _fork()_ syscall and organize fully-connected topology between them. Then it waits for all child processes to exit.  
Child process' lifecycle is of three phases: 

- Begin syncronization  
Each child process sends message _STARTED_ to all other processes and waits for all replies. After that it can proceed to next phase.
- Work  
In this task no work is done
- End synchronization  
Each child sends message _DONE_ to all other processes and waits for all replies. After that it exits.  
All events are logged to _events.log_. Formats for log messages are in _pa1.h_.  
It is prohibitted to use shared memory, synchronization primitives, functions _select()_ and _poll()_.
#### Build
`clang -std=c99 -Wall -pedantic *.c`
#### Run
./\<exec-name> -p \<proc_num>