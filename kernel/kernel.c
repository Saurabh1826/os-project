#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include <sys/types.h>
#include <fcntl.h>
#define _OPEN_SYS_ITOA_EXT

#include "linkedList.h"
#include "kernelFunctions.h"
#include "kernel.h"
#include "userFunctions.h"
#include "fat_fs/touch.h"
#include "fat_fs/mkfs.h"
#include "shell/shell.h"

extern linkedList *lowPriorityQueue;
extern linkedList *middlePriorityQueue;
extern linkedList *highPriorityQueue;
extern linkedList *waitpidBlocked;
extern linkedList *sleepBlocked;
extern linkedList *processTable;
extern ucontext_t *kernelContext;
extern ucontext_t *newContext;
extern ucontext_t *idleProcessContext;
extern pcb *currentProcessPcb;
extern int currentProcessPid;
extern char *bitmap;
extern int foregroundProcessPid;

int wstatus;

void handler(int);
void idleProcessFunc(void);
void f(void);
void f2(void);

int main(void) {

    // Register signal handlers
    signal(SIGALRM, handler);
    signal(SIGINT, handler);
    signal(SIGTSTP, handler);

    // Initialize kernel lists
    lowPriorityQueue = createList();
    middlePriorityQueue = createList();
    highPriorityQueue = createList();
    processTable = createList();
    waitpidBlocked = createList();
    sleepBlocked = createList();
    // Initialize kernelContext
    kernelContext = malloc(sizeof(ucontext_t));

    // Set seed for rand
    srand(time(NULL));

    // Create the filesystem
    mkfs("fs", 1, 0, bitmap);

    // Create root process
    newContext = malloc(sizeof(ucontext_t));
    // Initialize newContext
    getcontext(newContext);
    void *stack = malloc(STACK_SIZE);
    (newContext -> uc_stack).ss_sp = stack;
    (newContext -> uc_stack).ss_size = STACK_SIZE;
    (newContext -> uc_stack).ss_flags = 0;
    sigemptyset(&(newContext -> uc_sigmask));
    newContext -> uc_link = kernelContext; // If thread completes execution,
                                             // run the kernel thread afterwards
    makecontext(newContext, shell, 0); // The root process is the process running
                                       // the shell
    k_process_create2(-1, -1, RUNNING_STATE);

    // Initialize foregroundProcessPid to be the shell process
    foregroundProcessPid = 1;

    // Initialize the idleProcessContext
    idleProcessContext = malloc(sizeof(ucontext_t));
    // Initialize idleProcessContext
    getcontext(idleProcessContext);
    stack = malloc(STACK_SIZE);
    (idleProcessContext -> uc_stack).ss_sp = stack;
    (idleProcessContext -> uc_stack).ss_size = STACK_SIZE;
    (idleProcessContext -> uc_stack).ss_flags = 0;
    sigemptyset(&(idleProcessContext -> uc_sigmask));
    idleProcessContext -> uc_link = kernelContext; // If thread completes execution,
                                             // run the kernel thread afterwards
    makecontext(idleProcessContext, idleProcessFunc, 0);

    // Create the timer for the system clock
    struct itimerval timer;
    struct timeval tval_interval;
    struct timeval tval_value;

    tval_interval.tv_sec = 0;
    tval_interval.tv_usec = 100000;
    tval_value.tv_sec = 0;
    tval_value.tv_usec = 100000;
    // tval_interval.tv_sec = 3;
    // tval_interval.tv_usec = 0;
    // tval_value.tv_sec = 3;
    // tval_value.tv_usec = 0;

    timer.it_interval = tval_interval;
    timer.it_value = tval_value;

    setitimer(ITIMER_REAL, &timer, NULL);

    // TODO: IN F_READ FUNCTION, IN THE READ FROM STDIN CASE, CHECK IF THE CURRENT
    // PROCESS IS THE FOREGROUND PROCESS AND IF IT IS NOT, SEND A SIGSTOP TO THE 
    // CURRENT PROCESS 
    // TODO: CHANGE THE FILENAMES 'STDIN'/'STDOUT' TO 'TERMINAL' IN THE FILE 
    // DESCRIPTOR TABLE AND MAKE THE NECESSARY CHANGES IN THE FUNCTIONS
    // TODO: COMMENT EVERYTHING IN THE SHELL FOLDER
    // TODO: IN TERMINATEPROCESS FUNCTION, CHECK IF THE PROCESS BEING TERMINATED
    // IS THE FOREGROUND PROCESS. IF SO, MAKE SHELL THE FOREGROUND PROCESS

    // Run a while loop for the kernel. Chooses the next process to run, decrements
    // entries in the sleepBlocked queue, etc
    // while (1) {
    //     // Examine the sleepBlocked queue

    //     // Choose next process to run from the scheduler
    //     currentProcessPcb = scheduler();
    //     printf("Next process: %d, priority: %d\n", currentProcessPcb -> pid, currentProcessPcb -> priority);
    //     // Swap to the next process's context
    //     swapcontext(kernelContext, currentProcessPcb -> uc);
    // }
    

    // TODO: AT THE END OF EACH CLOCK TICK, DECREMENT THE ticks field in all entries
    // of the sleepBlocked queue. If any ticks field becomes 0, unblock that 
    // process
    // TODO: Manage swapping context between kernelContext and other thread contexts
    // TODO: When swapping from kernel context to a thread context, set 
    // currentProcessPcb
    // TODO: Assign newContext a new malloc'd ucontext whenever spawning a process
    // TODO: DIRECT CTRL+Z AND CTRL+C APPROPRIATELY BASED ON WHICH PROCESS IS 
    // THE CURRENT FOREGROUND PROCESS
    // TODO: ADD NICE FUNCTION
    // TODO: ADD OTHER CP MODES TO THE SHELLCP FUNCTION

    currentProcessPcb = findProcess(1);

    // char *argv[1] = {NULL};
    // p_spawn(&f2, argv, 0, 1);
    // p_spawn(&f, argv, 0, 1);
    // p_nice(2, 0);
    // p_nice(3, 1);

    // swapcontext(kernelContext, idleProcessContext);

    int low = 0, med = 0, high = 0;

    // int fd = open("log", O_RDWR | O_CREAT, 0644);

    while (1) {
        // If the process that was just running is not the idle process and is 
        // still runnable, put it back in the appropriate scheduler queue
        if (currentProcessPid != -1) { // We were not just running the idle process
            if (findProcess(currentProcessPid) != NULL) { // The process that was
                                                          // just running still
                                                          // exists and is not dead
                int state = currentProcessPcb -> state;
                // Check if the process that was just running is still runnable
                if (state == RUNNING_STATE || state == ORPHANED_STATE) 
                    addToScheduler(currentProcessPid, currentProcessPcb -> priority);
            }
        }
            
        // Examine the sleepBlocked queue
        lNode *currNode = sleepBlocked -> head;
        while (currNode != NULL) {
            sleepBlockedEntry *entry = (sleepBlockedEntry*)(currNode -> payload);
            entry -> ticks -= 1;
            // If entry -> ticks is now 0, we need to unblock this entry's process
            if ((entry -> ticks) == 0)
                unblockProcess(entry -> pid);
            
            currNode = currNode -> next;
        }

        // Choose next process to run from the scheduler
        currentProcessPcb = scheduler();
        // If scheduler function returns null, schedule idle process
        if (currentProcessPcb == NULL) {
            // Set currentProcessPid
            currentProcessPid = -1;
            // printf("Next process: idle process\n");
            swapcontext(kernelContext, idleProcessContext);
        } else {
            // Set currentProcessPid
            currentProcessPid = currentProcessPcb -> pid;

            // char c = 48 + currentProcessPid;
            // write(fd, &c, 1);
            // write(fd, "\n", 1);
            // pcb *p = findProcess(1);
            // if ((p -> stateChanges -> length) != 0) {
            //     stateChange *change = (stateChange*)(p -> stateChanges -> head -> payload);
            //     char c = 48 + change -> changeType;
            //     write(fd, &c, 1);
            //     write(fd, "\n", 1);
            // }

            // printf("Next process: %d, priority: %d\n", currentProcessPcb -> pid, currentProcessPcb -> priority);
            int pri = currentProcessPcb -> priority;
            if (pri == -1)
                high += 1;
            else if (pri == 0)
                med += 1;
            else if (pri == 1)
                low += 1;
            // printf("%d, %d, %d\n", high, med, low);
            // Swap to the next process's context
            swapcontext(kernelContext, currentProcessPcb -> uc);
        }

        // Print all processes
        // printf("All processes:\n");
        // currNode = processTable -> head;
        // while (currNode != NULL) {
        //     pcb *currPcb = (pcb*)(currNode -> payload);
        //     printf("pid: %d, ppid: %d, priority: %d, state: %d\n", currPcb->pid, currPcb->ppid, currPcb->priority, currPcb->state);
        //     currNode = currNode -> next;
        //     printf("\n");
        // }
    }

    // DEBUGGING/DEVELOPMENT STUFF: 

    // lNode *currNode;
    // // Print all processes
    // printf("All processes:\n");
    // currNode = processTable -> head;
    // while (currNode != NULL) {
    //     pcb *currPcb = (pcb*)(currNode -> payload);
    //     printf("pid: %d, ppid: %d, priority: %d, state: %d\n", currPcb->pid, currPcb->ppid, currPcb->priority, currPcb->state);
    //     printf("Fd Table:\n");
    //     lNode *node = currPcb -> fdTable -> head;
    //     while (node != NULL) {
    //         fdEntry *entry = (fdEntry*)(node -> payload);
    //         printf("fd: %d, file: %s, mode: %d, loc: %u\n", entry -> fd, entry -> fileName, entry -> mode, entry -> loc);
    //         node = node -> next;
    //     }
    //     currNode = currNode -> next;
    //     printf("\n");
    // }

    // // Print scheduler queues
    // printf("High priority:\n");
    // currNode = highPriorityQueue -> head;
    // while (currNode != NULL) {
    //     printf("%d ", *((int*)(currNode -> payload)));
    //     currNode = currNode -> next;
    // }
    // printf("\n");
    // printf("Medium priority:\n");
    // currNode = middlePriorityQueue -> head;
    // while (currNode != NULL) {
    //     printf("%d ", *((int*)(currNode -> payload)));
    //     currNode = currNode -> next;
    // }
    // printf("\n");
    // printf("Low priority:\n");
    // currNode = lowPriorityQueue -> head;
    // while (currNode != NULL) {
    //     printf("%d ", *((int*)(currNode -> payload)));
    //     currNode = currNode -> next;
    // }
    // printf("\n");

    
    // pcb *s;
    // for (int i = 0; i < 12; i++) {
    //     s = scheduler();
    //     if (s == NULL) {
    //         printf("null\n");
    //         continue;
    //     }
    //     printf("%d\n", s->pid);
    // }
    
}

void handler(int signo) {
    if (signo == SIGALRM) {
        // Swap context from currently running thread context to kernelContext. 
        // If currentProcessPcb is null, then idle process was running, otherwise
        // a true process was running
        if (currentProcessPcb == NULL) {
            // If idle process was running, then just set the context to the 
            // kernelContext
            setcontext(kernelContext);
        } else {
            swapcontext(currentProcessPcb -> uc, kernelContext);
        }   
    } else if (signo == SIGINT) {
        // Signal gets sent to the foreground process. If the foreground process
        // is the shell, the signal is ignored. Otherwise, the foreground process
        // is terminated with a S_SIGTERM signal. The kernel is run after this 
        // signal handler runs
        if (foregroundProcessPid != SHELL_PID) {
            k_process_kill(findProcess(foregroundProcessPid), S_SIGTERM);
            // Run the kernel. If the process that was running when the SIGINT was
            // received is still alive and is not the idle process, save its 
            // context. This is because if the process that was running was not 
            // the foreground process, it is still runnable
            if (findProcess(currentProcessPid) != NULL) {
                swapcontext(currentProcessPcb -> uc, kernelContext);
            } else { // The process that was just running was killed by the signal,
                     // so we don't need to save its context
                setcontext(kernelContext);
            }
        }
    } else if (signo == SIGTSTP) {
        // Signal gets sent to the foreground process. If the foreground process
        // is the shell, the signal is ignored. Otherwise, the foreground process
        // is stopped with a S_SIGSTOP signal. The kernel is run after this 
        // signal handler runs
        if (foregroundProcessPid != SHELL_PID) {
            k_process_kill(findProcess(foregroundProcessPid), S_SIGSTOP);
            // Run the kernel. Save the context of the process that was just
            // running, unless it was the idle process
            if (currentProcessPcb != NULL)
                swapcontext(currentProcessPcb -> uc, kernelContext);
            else 
                setcontext(kernelContext);
        }
    }
}

void idleProcessFunc(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigsuspend(&mask);
}

void f(void) {
    while(1) sleep(100);
}

void f2(void) {
    // while(1) sleep(100);
    char buffer[20];
    f_read(0, buffer, 4);
    buffer[5] = '\0';
    f_write(1, buffer, 4);
    while(1) sleep(1);
}

