# Simple Banking System. Real clock
Interprocess messaging with physical time.  
Description is available in Russian (original) and English (translated and shorted).
## ru
[Task Description in Russian](https://github.com/SuperJaremy/dist-comp/blob/pa2/pa2.pdf)
## en
### Description
Simple banking system implemented using library from previous task.  
System consists of one parent process and up to ten child processes. Architecture is similar to one from previous task.  
This time every child process has an initial balance. In its second phase child process is doing transfers from C(hild)<sub>src</sub> to C(hild)<sub>dst</sub>. All transfers are orchestrated by parent process. After getting a _TRANSFER_ message C<sub>src</sub> subtract a stated amount from its balance and sends _TRANSFER_ message to C<sub>dst</sub>. C<sub>src</sub> and C<sub>dst</sub> ids with amount to be transfered are within _TRANSFER_ message body. Each transfer is tracked by child process and marked with timestamp.  
After begin synchronization phase parent process run _bank\_robbery()_ function wich executes a series of transfers between child processes. After that it sends _STOP_ message to all child processes, waits until the end of end synchronization and prints all their balance histories.  
All events are printed to _events.log_ file.
### Build
clang -std=c99 -Wall -pedantic *.c -L. -lruntime
### Run
```
export LD_LIBRARY_PATH="LD_LIBRARY_PATH:/path/to/pa2/dir";  
### empty line
LD_PRELOAD=/full/path/to/llibruntime.so ./pa2 -p <proc_num> <proc_balance_1 ... proc_balalance_n>
```
