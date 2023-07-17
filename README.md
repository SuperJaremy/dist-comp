# Simple Banking System. Lamport's logical clock
Interprocess messaging with lamport time.  
Description is available in Russian (original) and English (translated and shorted).
## ru
[Task Description in Russian](https://github.com/SuperJaremy/dist-comp/blob/pa3/pa3.pdf)
## en
### Description
Modification of system implemented in previous task. In that task the sum of balances in the system may have varied as each child process used its own physical clock and these clocks could have been out of sync with each other. By using Lamport timestamp we should eliminate this problem. It also gives us an ability to account states of channel. After this modification the sum of all balances of child processes and money that are still in transfer always remains the same.
### Build
`clang -std=c99 -Wall -pedantic *.c -L. -lruntime`
### Run
```
export LD_LIBRARY_PATH="LD_LIBRARY_PATH:/path/to/pa3/dir";  
### empty line
LD_PRELOAD=/full/path/to/llibruntime.so ./pa3 -p <proc_num> <proc_balance_1 ... proc_balalance_n>
```