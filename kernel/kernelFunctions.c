#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <time.h>
#include "linkedList.h"
#include "kernel.h"
#include "kernelFunctions.h"

// Variables for the scheduler queues. The scheduler queues are queues of 
// pids
linkedList *lowPriorityQueue;
linkedList *middlePriorityQueue;
linkedList *highPriorityQueue;
// Variables for blocked lists. 
linkedList *waitpidBlocked; // List of pids of processes blocked on a waitpid call.
linkedList *sleepBlocked; // List of processes blocked on a sleep call. Is a list
                          // of sleepBlockedEntry entries. 


// Variable for the process table (implemented as a list of PCBs)
linkedList *processTable;
// Variable for the ucontext of the kernel thread
ucontext_t *kernelContext;
// Variable for the ucontext of a new process that is going to be created. 
// When the user level process spawn function is called, it will initialize this 
// variable with all the necessary information for the new thread before calling
// k_process_create. k_process_create will then use this context to create
// the new thread and pcb. NOTE: this variable should be a pointer returned by 
// malloc, in order to prevent the information it points to from being overwritten
// later
ucontext_t *newContext;
// Variable that is a pointer to the ucontext for the idle process
ucontext_t *idleProcessContext;
// Variable to keep track of the current highest pid that has been allocated. 
// When a new pid needs to be allocated, we simply allocate highestPid + 1 to
// the new process and increment highestPid
int highestPid = 0;
// Variable that is the pcb of the currently running process. If the idle process
// is running, it will be null
pcb *currentProcessPcb = NULL;
// Variable that is the pid of the currently running process. If the idle process
// is running, it will be -1
int currentProcessPid = -1;
// Variable that is the pid of the process that currently has terminal control (ie: 
// the process that is currently the foreground process)
int foregroundProcessPid = -1;



// Function to implement k_process_create. Creates a new child thread and 
// associated PCB. Most PCB fields of the newly created child are the same as those
// of the parent. NOTE: The ucontext for the newly created process is taken from
// the variable newContext. Thus, it is the caller's responsibility to malloc a new
// ucontext for the new child process and put it in newContext before calling this 
// function
// Arguments: 
//     parentPCB: A pointer to the PCB of the parent
// Returns: 
//     A pointer to the PCB of the newly created child
pcb *k_process_create(pcb *parentPcb) {
    // Create a new pcb entry for the child 
    pcb *newPcb = malloc(sizeof(pcb));
    newPcb -> uc = newContext;
    newPcb -> pid = (highestPid++) + 1;
    newPcb -> ppid = parentPcb -> pid;
    newPcb -> childPids = createList();
    // Create a new fd table for the child that is a copy of the parent's fd table
    linkedList *fdTable = createList();
    lNode *currNode = parentPcb -> fdTable -> head;
    while (currNode != NULL) {
        fdEntry *newEntry = malloc(sizeof(fdEntry));
        fdEntry *oldEntry = (fdEntry*)(currNode -> payload);
        // Copy the contents from oldEntry to newEntry
        newEntry -> fd = oldEntry -> fd;
        char *str = malloc(strlen(oldEntry -> fileName));
        strcpy(str, oldEntry -> fileName);
        newEntry -> fileName = str;
        newEntry -> mode = oldEntry -> mode;
        newEntry -> loc = oldEntry -> loc;
        // Add entry to fdTable
        addNodeTail(fdTable, newEntry);
        currNode = currNode -> next;
    }
    newPcb -> fdTable = fdTable;
    newPcb -> priority = parentPcb -> priority; // Child inherits parent's priority
    newPcb -> state = RUNNING_STATE;
    newPcb -> stateChanges = createList();

    // Add the child to the parent PCB's childPids list
    int *pidPtr = malloc(sizeof(int)); // Pointer to newPcb's pid 
    *pidPtr = newPcb -> pid;
    addNodeTail(parentPcb -> childPids, pidPtr);

    // Add newPcb to the process table
    addNodeTail(processTable, newPcb);

    // Add the newly created process to the appropriate scheduler queue
    addToScheduler(newPcb -> pid, newPcb -> priority);

    return newPcb;
} 


// Function to create a new process and add it to processTable. Intended to be
// used to create the root process.
// Arguments: 
//     uc: The ucontext for the new process 
//     ppid: The ppid for the new process 
//     priority: The priority of the process 
//     state: The state of the process 
// Returns: 
//     A pointer to the pcb of the newly created process
pcb *k_process_create2(int ppid, int priority, int state) {
    pcb *newPcb = malloc(sizeof(pcb));
    newPcb -> uc = newContext;
    newPcb -> pid = (highestPid++) + 1;
    newPcb -> ppid = ppid;
    newPcb -> childPids = createList();
    // Initialize the fdTable with the standard file descriptors 0 and 1 for 
    // stdin and stdout, respectively
    newPcb -> fdTable = createList();
    fdEntry *entry = malloc(sizeof(fdEntry));
    char *fileName = malloc(strlen("stdin") + 1);
    strcpy(fileName, "stdin");
    entry -> fd = 0;
    entry -> fileName = fileName;
    entry -> mode = F_READ;
    entry -> loc = 0;
    addNodeTail(newPcb -> fdTable, entry);
    entry = malloc(sizeof(fdEntry));
    fileName = malloc(strlen("stdout") + 1);
    strcpy(fileName, "stdout");
    entry -> fd = 1;
    entry -> fileName = fileName;
    entry -> mode = F_WRITE;
    entry -> loc = 0;
    addNodeTail(newPcb -> fdTable, entry);
    newPcb -> priority = priority;
    newPcb -> state = state;
    newPcb -> stateChanges = createList();
    // Add the child to the parent PCB's childPids list if parent exists
    pcb *parentPcb = findProcess(ppid);
    if (parentPcb != NULL) {
        int *pidPtr = malloc(sizeof(int)); // Pointer to newPcb's pid 
        *pidPtr = newPcb -> pid;
        addNodeTail(parentPcb -> childPids, pidPtr);
    }

    // Add newPcb to the process table
    addNodeTail(processTable, newPcb);

    // Add the newly created process to the appropriate scheduler queue
    addToScheduler(newPcb -> pid, newPcb -> priority);

    return newPcb;
}


// Function to create a new entry in the file descriptor table for a specified
// process. Assumes the specified process is valid
// Arguments: 
//     pid: pid of the process whose file descriptor table to add the entry to 
//     fd: The file descriptor of the new entry 
//     fileName: The name of the file the entry points to 
//     mode: The mode in which the file is open (F_READ, F_WRITE, etc)
// Returns: 
//     None 
void createFdEntry(int pid, int fd, char *fileName, int mode) {
    // Get pcb of the process
    pcb *processPcb = findProcess(pid);
    // Malloc a new fdEntry and string and copy over fileName to the newly malloc'd
    // string
    fdEntry *entry = malloc(sizeof(fdEntry));
    char *name = malloc(strlen(fileName) + 1);
    strcpy(name, fileName);
    // Initalize entry fields
    entry -> fd = fd;
    entry -> fileName = name;
    entry -> mode = mode;
    entry -> loc = 0;
    // Add entry to file descriptor table
    addNodeTail(processPcb -> fdTable, entry);
}


// Function to obtain a new file descriptor. Returns the smallest integer greater
// than all currently open file descriptors
// Arguments: 
//     processPcb: The pcb of the process for which to get the new file descriptor 
// Returns: 
//     An integer that is the minimum unused file descriptor
int getNewFd(pcb *processPcb) {
    int fd = 0;
    lNode *currNode = processPcb -> fdTable -> head;
    while (currNode != NULL) {
        fdEntry *entry = (fdEntry*)(currNode -> payload);
        fd = ((entry -> fd) > fd) ? entry -> fd : fd;
        currNode = currNode -> next;
    }

    return fd + 1;
}



// Function to implement k_process_kill. Takes the appropriate action on the 
// specified process based on the signal and also signals the parent of the
// state change if necessary
// Arguments: 
//     processPcb: A pointer to the PCB of the process that will receive the signal
//     signal: An integer specifying the signal to be delivered. 
// Returns: 
//     0 on success, -1 otherwise (eg: specified process is zombied or not in 
//     processTable)
int k_process_kill(pcb *processPcb, int signal) {
    // Check that the specified process is valid
    if (processPcb == NULL) return -1;
    // Get pid of the process
    int pid = processPcb -> pid;
    // Get state of the process
    int state = processPcb -> state;
    if (signal == S_SIGSTOP) {
        // If process was already stopped, do nothing
        if (state == STOPPED_STATE) return 0;
        // Change the process state
        processPcb -> state = STOPPED_STATE;
        // Remove the process from its scheduler queue
        removeFromScheduler(pid, processPcb -> priority);
        // Add this state change to the parent process's stateChanges list
        addStateChange(processPcb -> ppid, pid, STOP_CHANGE);
        // Unblock parent process if it exists and is in waitpidBlocked
        if (isWaitpidBlocked(processPcb -> ppid))
            unblockProcess(processPcb -> ppid);
    } else if (signal == S_SIGCONT) {
        // If process is not currently stopped, then sigcont signal does nothing
        if (state != STOPPED_STATE) return 0;
        // Add this state change to the parent process's stateChanges list
        addStateChange(processPcb -> ppid, pid, CONT_CHANGE);
        // Unblock parent process if it exists and is in waitpidBlocked
        if (isWaitpidBlocked(processPcb -> ppid))
            unblockProcess(processPcb -> ppid);
        // Change the process state
        // If the process is blocked, change its state to blocked
        if (isWaitpidBlocked(pid) || isSleepBlocked(pid)) {
            processPcb -> state = BLOCKED_STATE;
            return 0;
        } 
        // If process is an orphan, set its state to orphaned state and add it
        // to the appropriate scheduler queue
        if (findProcess(processPcb -> ppid) == NULL && (processPcb -> ppid) != -1) {
            processPcb -> state = ORPHANED_STATE;
            addToScheduler(pid, processPcb -> priority);
            return 0;
        }
        // If we get here, we know the process's state should now be running state
        processPcb -> state = RUNNING_STATE;
        // Put process back in the appropriate scheduler queue if state is now
        // orphaned or running
        addToScheduler(pid, processPcb -> priority);
    } else if (signal == S_SIGTERM) { // Kill the process 
        terminateProcess(pid, TERM_CHANGE);
    }

    return 0;
}

// Function to terminate a process. The function notifies the process's parent,
// calls k_process_cleanup on its zombie children, removes the process from all
// scheduler/blocked queues, and becomes a zombie or calls k_process_cleanup on the
// process (if its parent process is terminated)
// Arguments: 
//     pid: pid of the process to terminate 
//     type: The manner in which the process is being terminated (ie: EXIT_CHANGE or
//     TERM_CHANGE)
// Returns: 
//     0 on success, -1 otherwise (eg: process is not in processTable or is a zombie)
int terminateProcess(int pid, int type) {
    //  Get the pcb of the process
    pcb *processPcb = findProcess(pid);
    // If the process is a zombie or does not exist, return -1
    if (processPcb == NULL) return -1;

    // Remove the process from scheduler and blocked queues
    removeFromScheduler(pid, processPcb -> priority);
    removeFromBlockedList(pid, waitpidBlocked);
    removeFromBlockedList(pid, sleepBlocked);

    // Call k_process_cleanup on the zombie children of the process by iterating
    // through the process's stateChanges list and searching for those processes
    // whose changeType is either exit or term. 
    lNode *currNode = processPcb -> stateChanges -> head;
    while (currNode != NULL) {
        stateChange *change = (stateChange*)(currNode -> payload);
        int changeType = change -> changeType;
        if (changeType == EXIT_CHANGE || changeType == TERM_CHANGE) 
            k_process_cleanup(getPcb(change -> pid));
        currNode = currNode -> next;
    }

    // Change the state of the remaining children of the process to orphaned, if 
    // they are not blocked or stopped (we already the removed the zombied children)
    currNode = processPcb -> childPids -> head;
    while (currNode != NULL) {
        int childPid = *((int*)(currNode -> payload));
        pcb *childPcb = findProcess(childPid);
        if ((childPcb -> state) != BLOCKED_STATE && (childPcb -> state) != STOPPED_STATE)
            childPcb -> state = ORPHANED_STATE;
        currNode = currNode -> next;
    }

    // If the process's parent is dead, call k_process_cleanup on this process
    if (findProcess(processPcb -> ppid) == NULL) {
        k_process_cleanup(getPcb(pid));
    } else { // If parent process is still alive, add this process's state change to
             // the parent's stateChanges list and change the process's state
             // to zombie
        processPcb -> state = ZOMBIED_STATE;
        addStateChange(processPcb -> ppid, pid, type);
        // Unblock parent process if it exists and is in waitpidBlocked
        if (isWaitpidBlocked(processPcb -> ppid))
            unblockProcess(processPcb -> ppid);
    }

    return 0;
}


// Function to block a process. Sets the specified process state to blocked and 
// removes it from its current scheduler queue if necessary. Assumes the specified
// process exists and is not zombied
// Arguments: 
//     pid: pid of the process to block 
//     list: The blocked list in which to put the process (ie: sleepBlocked or 
//     waitpidBlocked)
//     ticks: If list is sleepBlocked, then this argument will be used to determine
//     how many ticks to block the process for
// Returns: 
//     None
void blockProcess(int pid, linkedList *list, int ticks) {
    pcb *processPcb = findProcess(pid);
    // Set process state to blocked
    processPcb -> state = BLOCKED_STATE;
    // Remove process from scheduler queue
    removeFromScheduler(pid, processPcb -> priority);
    //  Put process in appropriate blocked queue
    if (list == waitpidBlocked) {
        int *temp = malloc(sizeof(int)); // Create int as payload for the node to be
                                        // added to the blocked queue
        *temp = pid;
        addNodeTail(list, temp);
    } else {
        sleepBlockedEntry *entry = malloc(sizeof(sleepBlockedEntry));
        entry -> pid = pid;
        entry -> ticks = ticks;
        addNodeTail(list, entry);
    }
}


// Function to unblock a process.
// Arguments: 
//     pid: pid of process to unblock. 
// Returns: 
//     0 on success, -1 otherwise (eg: process does not exist/is dead)
int unblockProcess(int pid) {
    //  Get the pcb of the process
    pcb *processPcb = findProcess(pid);
    // If process does not exist/is dead, return -1
    if (processPcb == NULL) return -1;
    // Remove process from blocked queue
    removeFromBlockedList(pid, waitpidBlocked);
    removeFromBlockedList(pid, sleepBlocked);
    // Restore the appropriate state of the process (either stopped, running,
    // or orphaned), and add to scheduler queue if necessary
    if (processPcb -> state == STOPPED_STATE) return 0; 
    if (findProcess(processPcb -> ppid) == NULL && (processPcb -> ppid) != -1) { // Process is an orphan
        processPcb -> state = ORPHANED_STATE;
        addToScheduler(pid, processPcb -> priority);
        return 0;
    }
    // If we get here, the process must now be in the running state
    processPcb -> state = RUNNING_STATE;
    addToScheduler(pid, processPcb -> priority);

    return 0;
}



// Function to add a state change to a process's stateChanges list.
// Arguments: 
//     ppid: pid of the process whose stateChanges list is being edited 
//     pid: pid of the process that underwent the state change
//     changeType: The type of state change 
// Returns: 
//     None
void addStateChange(int ppid, int pid, int changeType) {
    // Get the pcb for the parent process
    pcb *parentPcb = findProcess(ppid);
    // If parent does not exist or is dead, do nothing
    if (parentPcb == NULL) return;
    // Check if parent process's stateChanges list has an entry for the process pid
    lNode *currNode = parentPcb -> stateChanges -> head;
    while (currNode != NULL) {
        stateChange *change = (stateChange*)(currNode -> payload);
        if ((change -> pid) == pid) break;
        currNode = currNode -> next;
    }

    if (currNode == NULL) { // Parent process's stateChanges list does not have an 
                            // entry for the process pid
        // Add a new entry in parent process's stateChanges list with the current
        // state change
        stateChange *change = malloc(sizeof(stateChange));
        change -> pid = pid;
        change -> changeType = changeType;
        addNodeTail(parentPcb -> stateChanges, change);
    } else { // Parent process's stateChanges list already has an entry for process
             // pid
        // Get the stateChange object that is currently in the parent process's
        // stateChanges list for process pid
        stateChange *change = (stateChange*)(currNode -> payload);
        // If change is for an exit or term change, do nothing
        if ((change -> changeType) == EXIT_CHANGE || (change -> changeType) == TERM_CHANGE) 
            return;
        // If change is for a stopped state and changeType is continue or vice 
        // versa, remove change from parent process's stateChanges
        if (((change -> changeType) == CONT_CHANGE && changeType == STOP_CHANGE) || 
            ((change -> changeType) == STOP_CHANGE && changeType == CONT_CHANGE)) {
                removeNode(parentPcb -> stateChanges, currNode);
                return;
            }
        // If we get here, we need to replace the parent process's stateChanges 
        // entry with the new state change
        change -> changeType = changeType;
    }
}


// Function to check if the specified process is in the waitpidBlocked list
// Arguments: 
//     pid: pid of the process to check 
// Returns: 
//     1 if the process is in waitpidBlocked, 0 otherwise 
int isWaitpidBlocked(int pid) {
    lNode *currNode = waitpidBlocked -> head;
    while (currNode != NULL) {
        if (*((int*)(currNode -> payload)) == pid) return 1;
        currNode = currNode -> next;
    } 

    return 0;
}


// Function to check if the specified process is in the sleepBlocked list
// Arguments: 
//     pid: pid of the process to check 
// Returns: 
//     1 if the process is in sleepBlocked, 0 otherwise 
int isSleepBlocked(int pid) {
    lNode *currNode = sleepBlocked -> head;
    while (currNode != NULL) {
        sleepBlockedEntry *entry = (sleepBlockedEntry*)(currNode -> payload);
        if ((entry -> pid) == pid) return 1;
        currNode = currNode -> next;
    } 

    return 0;
}


// Function to remove the specified process from the specified blocked list
// Arguments: 
//     pid: pid of the process to remove from the blocked list 
//     blockedList: The blocked list from which to remove the specified process
// Returns: 
//     None 
void removeFromBlockedList(int pid, linkedList *blockedList) {
    // Try to remove the process from blockedList
    lNode *currNode = blockedList -> head;
    while (currNode != NULL) {
        if (blockedList == waitpidBlocked) {
            if (*((int*)(currNode -> payload)) == pid) 
                removeNode(blockedList, currNode);
        } else {
            sleepBlockedEntry *entry = (sleepBlockedEntry*)(currNode -> payload);
            if ((entry -> pid) == pid)
                removeNode(blockedList, currNode);
        }
        currNode = currNode -> next;
    }
}



// Function to implement k_process_cleanup. Frees memory of the pcb as well as 
// the necessary fields within the pcb. Also removes the pcb from the process
// table. If the parent process is still in the process table, the function also
// removes the specified process from the childPids list in the parent process. 
// The function assumes processPcb is a valid process table entry. This function
// is to be called when it is time to remove a process from the process table
// Arguments: 
//     processPcb: A pointer to the PCB to be cleaned up
// Returns: 
//     None
void k_process_cleanup(pcb *processPcb) {
    // Remove process from parent's childPids list if parent still exists
    int pid = processPcb -> pid;
    int ppid = processPcb -> ppid;
    lNode *currNode = processTable -> head;
    while (currNode != NULL) {
        if (((pcb*)(currNode -> payload)) -> pid == ppid) { // currNode is the parent process
                                                  // node is processTable
            // Find the process's entry in the parent's childPids list
            linkedList *childPids = ((pcb*)(currNode -> payload)) -> childPids;
            currNode = childPids -> head;
            while (currNode != NULL) {
                if (*((int*)(currNode -> payload)) == pid) {
                    removeNode(childPids, currNode);
                    break;
                }
                currNode = currNode -> next;
            }
            break;
        }
        currNode = currNode -> next;
    }

    // Free memory of the linked lists, the ucontext, and the pcb itself
    free((processPcb -> uc -> uc_stack).ss_sp); // Free thread's stack
    free(processPcb -> uc);
    freeList(processPcb -> childPids);
    // TODO: IN THE FOLLOWING LINE, NEED TO ALSO FREE THE STRINGS IN THE FDTABLE
    // ENTRIES THAT ARE THE FILENAME FIELDS
    freeList(processPcb -> fdTable);
    freeList(processPcb -> stateChanges);
    
    // Remove processPcb from the process table (this also frees processPcb 
    // because processPcb is in the processTable)
    lNode *processPcbNode = findNode(processTable, processPcb);
    removeNode(processTable, processPcbNode);
}


// Function to find the pcb of a process given its pid. Will return NULL if the 
// specified process is still in processTable but is a zombie
// Arguments: 
//     pid: pid of the process whose pcb to find 
// Returns: 
//     NULL if a process with the given pid is not present in processTable, and the
//     appropriate pcb otherwise
pcb *findProcess(int pid) {
    lNode *currNode = processTable -> head;
    pcb *currPcb;
    while (currNode != NULL) {
        currPcb = (pcb*)(currNode -> payload);
        if ((currPcb -> pid) == pid && (currPcb -> state) != ZOMBIED_STATE) 
            return currPcb;
        currNode = currNode -> next;
    }

    return NULL;
}


// Function to find the pcb of a process given its pid. Is the same as the function
// findProcess, but returns the pcb even if the specified process is a zombie
// Arguments: 
//     pid: pid of the process whose pcb to find 
// Returns: 
//     NULL if the specified process is not in the processTable, and the desired pcb
//     otherwise
pcb *getPcb(int pid) {
    lNode *currNode = processTable -> head;
    pcb *currPcb;
    while (currNode != NULL) {
        currPcb = (pcb*)(currNode -> payload);
        if ((currPcb -> pid) == pid) return currPcb;
        currNode = currNode -> next;
    }

    return NULL;
}


// Function to implement the scheduler. The function will choose a process to 
// run and return a pointer to its pcb. If all the queues are empty, a NULL 
// pointer is returned, and the kernel will run the idle process in response. 
// The kernel.c program will run the process chosen by the function--the function's
// responsibility is simply to choose the process to run. NOTE: by the end of the 
// time quantum, if the chosen process is still runnable and not terminated, the 
// kernel.c program should put the process back in the appropriate scheduler queue
// Arguments: 
//     None
// Returns: 
//     NULL if all the queues are empty, otherwise a pointer to the pcb of the 
//     process to run
pcb *scheduler() {
    // Check if all scheduler queues are empty
    if (lowPriorityQueue -> length == 0 && middlePriorityQueue -> length == 0 && highPriorityQueue -> length == 0)
        return NULL;
    
    int r;
    lNode *node; 
    pcb *processToRun;
    int processPid;
    
    while (1) {
        r = rand() % 19; // r is a random integer between 0 and 18, inclusive
        if (r <= 3) { // Pick from the low priority queue
            if (lowPriorityQueue -> length == 0) continue;
            node = lowPriorityQueue -> head;
            processPid = *((int*)(node -> payload));
            processToRun = findProcess(processPid);
            // Remove the chosen process from the scheduler queue
            removeNode(lowPriorityQueue, node);
            break;
        } else if (r >= 4 && r <= 9) {
            if (middlePriorityQueue -> length == 0) continue;
            node = middlePriorityQueue -> head;
            processPid = *((int*)(node -> payload));
            processToRun = findProcess(processPid);
            // Remove the chosen process from the scheduler queue
            removeNode(middlePriorityQueue, node);
            break;
        } else {
            if (highPriorityQueue -> length == 0) continue;
            node = highPriorityQueue -> head;
            processPid = *((int*)(node -> payload));
            processToRun = findProcess(processPid);
            // Remove the chosen process from the scheduler queue
            removeNode(highPriorityQueue, node);
            break;
        }
    }

    return processToRun;
}


// Function to add a process to an appropriate scheduler queue. Assumes specified 
// process is runnable
// Arguments: 
//     pid: pid of the process to be added to a scheduler queue 
//     priority: Priority of the process 
// Returns: 
//     None
void addToScheduler(int pid, int priority) {
    linkedList *queue;
    if (priority == LOW_PRIORITY)
        queue = lowPriorityQueue;
    else if (priority == MED_PRIORITY)
        queue = middlePriorityQueue;
    else 
        queue = highPriorityQueue;
    // Malloc an integer as the payload of the new node to add to the queue
    int *payload = malloc(sizeof(int));
    *payload = pid;
    addNodeTail(queue, payload);
}


// Function to remove a process from the appropriate scheduler queue. If the 
// process is not in the appropriate queue (eg: process is not runnable), the
// function does nothing
// Arguments: 
//     pid: pid of the process to be removed from a scheduler queue 
//     priority: Priority of the process 
// Returns: 
//     None
void removeFromScheduler(int pid, int priority) {
    lNode *currNode;
    linkedList *queue;
    if (priority == LOW_PRIORITY) {
        currNode = lowPriorityQueue -> head;
        queue = lowPriorityQueue;
    }
    else if (priority == MED_PRIORITY) {
        currNode = middlePriorityQueue -> head;
        queue = middlePriorityQueue;
    }
    else {
        currNode = highPriorityQueue -> head;
        queue = highPriorityQueue;
    }
    while (currNode != NULL) { // Traverse the appropriate scheduler queue to 
                               // find the process
        if (*((int*)(currNode -> payload)) == pid) {
            removeNode(queue, currNode);
            break;
        }
        currNode = currNode -> next;
    }
}



// Function to implement k_p_nice, the kernel level function for the user level
// function p_nice. Sets the priority of the specified thread to the specified
// priority. If the thread is runnable, also remove the thread from its current
// scheduler queue and put it in the appropriate scheduler queue
// Arguments: 
//     pid: pid of the process whose priority to change 
//     priority: The new priority of the specified process 
// Returns: 
//     0 on success, -1 otherwise (eg: the process pid does not exist)
int k_p_nice(int pid, int priority) {
    pcb *processPcb = findProcess(pid);
    // If process pid does not exist, return -1
    if (processPcb == NULL) return -1;
    // If the specified process already has the specified priority, do nothing
    if ((processPcb -> priority) == priority) return 0;

    // Remove the process from its current scheduler queue, if it is there
    removeFromScheduler(pid, processPcb -> priority);

    // Change the priority of the process in its pcb
    processPcb -> priority = priority;

    // Put the process in the appropriate scheduler queue, if it is runnable
    int processState = processPcb -> state;
    if (processState == RUNNING_STATE || processState == ORPHANED_STATE) 
        addToScheduler(pid, priority);

    return 0;
}


// Function to implement the ps function, which lists the pid, ppid, state, and 
// priority of all processes
// Arguments: 
//     None 
// Returns: 
//     None 
void k_ps(void) {
    // Iterate through the process table
    lNode *currNode = processTable -> head;
    while (currNode != NULL) {
        // Get the pcb of the process
        pcb *processPcb = (pcb*)(currNode -> payload);
        printf("pid: %d, ppid: %d, priority: %d, state: ", processPcb -> pid, processPcb -> ppid, processPcb -> priority);
        int state = processPcb -> state;
        if (state == RUNNING_STATE) {
            printf("Running\n");
        } else if (state == BLOCKED_STATE) {
            printf("Blocked\n");
        } else if (state == ZOMBIED_STATE) {
            printf("Zombied\n");
        } else if (state == ORPHANED_STATE) {
            printf("Orphaned\n");
        } else if (state == STOPPED_STATE) {
            printf("Stopped\n");
        }

        currNode = currNode -> next;
    }
}



