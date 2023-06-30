#ifndef KERNEL_H
#define KERNEL_H

#include <ucontext.h>
#include <stdint.h>
#include "linkedList.h"

// Definitions for integer encodings of process states
#define RUNNING_STATE 0
#define BLOCKED_STATE 1
#define STOPPED_STATE 2
#define ZOMBIED_STATE 3
#define ORPHANED_STATE 4
// Definitions for integer encodings of signals
#define S_SIGSTOP 0
#define S_SIGCONT 1
#define S_SIGTERM 2
// Definitions for integer encodings of priorities
#define LOW_PRIORITY 1
#define MED_PRIORITY 0
#define HIGH_PRIORITY -1
// Definitions for integer encodings of state changes
#define STOP_CHANGE 0 // State change where child was stopped
#define CONT_CHANGE 1 // State change where child was continued
#define TERM_CHANGE 2 // State change where child was killed by a signal
#define EXIT_CHANGE 3 // State change where child exited normally
// Definitions for integer encodings of file open modes
#define F_WRITE 0
#define F_READ 1
#define F_APPEND 2
// Definitions for the integer encodings of the whence arguments in f_lseek
#define F_SEEK_SET 0
#define F_SEEK_CUR 1
#define F_SEEK_END 2
// Definition for stack size for newly created threads
#define STACK_SIZE 10000000
// Definition for the pid of the shell process, which is always 1
#define SHELL_PID 1

// Definition of struct for a PCB
typedef struct pcb {
    ucontext_t *uc; // Pointer to process's ucontext
    int pid;
    int ppid;
    linkedList *childPids; // Pointer to list of the children's pids. 
    linkedList *fdTable; // Pointer to file descriptor table, implemented as a 
                         // list of file descriptor entries
    int priority; // Priority level of the process (-1, 0, or 1)
    int state; // State of the process (running, zombie, etc)
    // List of stateChange structs to keep track of the state changes of the 
    // process's children. This list is used by the waitpid function to check
    // whether the children have undergone a state change to report
    linkedList *stateChanges; 
} pcb;

// Definition of struct for entries of a file descriptor table
typedef struct fdEntry {
    int fd; // The file descriptor. File descriptor 0 is for the process's input
            // file, and 1 is for the process's output file
    char *fileName; // The file name of the file associated with fd. If the fd is 
                    // associated with stdin/stdout, the fileName will be stdin/
                    // stdout, respectively. 
    int mode; // Mode in which the file is opened (F_WRITE, F_READ, or F_APPEND)
    uint32_t loc; // The current location we are at in the file (for reading, writing,
             // seeking, etc)
} fdEntry;
// TODO: CHANGE ALL THE FUNCTIONS INVOLVING FDENTRY TO MATCH THE FOLLOWING:
// Add the following fields to fdEntry: 
// mode: The mode in which the file is opened
// loc: The current location we are at in the file (for reading, writing, seeking, etc)

// Definition for struct for entries of the sleepBlocked list
typedef struct sleepBlockedEntry {
    int pid; // The pid of the blocked process
    int ticks; // The number of clock ticks remaining until the process becomes 
               // unblocked
} sleepBlockedEntry;

// Definition of struct for elements of the stateChanges list
typedef struct stateChange {
    int pid; // The pid this state change is associated with
    int changeType; // The type of change of this stateChange
} stateChange;


#endif