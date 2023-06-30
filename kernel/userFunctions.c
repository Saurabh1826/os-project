// NOTE: When implementing waitpid, need to check for state change in a while loop.
// This is because the process will be scheduled if any of its children have a state
// change, but the process that the waitpid was called on may itself not have had
// a state change, so we need to have a while loop to check for state change

// NOTE: Whenever a call is made to k_process_create or k_process_create2, it is 
// the responsibility of the caller to malloc a new ucontext and put its address in 
// the variable newContext. 

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/types.h>
#include <fcntl.h>

#include "kernel.h"
#include "kernelFunctions.h"
#include "userFunctions.h"
#include "fat_fs/headers.h"
#include "fat_fs/mkfs.h"
#include "fat_fs/touch.h"

extern pcb *currentProcessPcb;
extern ucontext_t *kernelContext;
extern ucontext_t *newContext;
extern linkedList *waitpidBlocked;
extern linkedList *sleepBlocked;
extern int currentProcessPid;
extern int foregroundProcessPid;

// Function to implement p_spawn. Spawns a new process. NOTE: The array argv 
// must be null terminated. 
// Arguments: 
//     func: The function the newly created process is to execute 
//     argv: The argument array with which to execute func 
//     fd0: File descriptor for the input file 
//     fd1: File descriptor for the output file
// Returns: 
//     pid of the child process on success, -1 on error
int p_spawn(void (*func)(), char *argv[], int fd0, int fd1) {
    // Malloc a new ucontext and put it in the variable newContext for 
    // k_process_create to use
    newContext = malloc(sizeof(ucontext_t));
    // Initialize newContext
    getcontext(newContext);
    void *stack = malloc(STACK_SIZE);
    (newContext -> uc_stack).ss_sp = stack;
    (newContext -> uc_stack).ss_size = STACK_SIZE;
    (newContext -> uc_stack).ss_flags = 0;
    sigemptyset(&(newContext -> uc_sigmask));
    newContext -> uc_link = kernelContext; // If a thread completes execution,
                                             // run the kernel thread afterwards
    makecontext(newContext, func, 1, argv);
    
    // Create the new process
    pcb *newProcessPcb = k_process_create(currentProcessPcb);

    // Change file descriptors 0 and 1 in the new process if necessary
    if (dup2Func(fd0, 0, newProcessPcb) == -1) return -1;
    if (dup2Func(fd1, 1, newProcessPcb) == -1) return -1;

    return newProcessPcb -> pid;
}


// Function to implement dup2. If it doesn't exist, the file descriptor newFd is 
// created and made to point to the file pointed to by oldFd. If newFd exists,
// the file it was pointing to is replaced by the file pointed to by oldFd.
// If oldFd == newFd, do nothing
// Arguments: 
//     oldFd: The file descriptor to be duplicated
//     newFd: The new file descriptor 
//     processPcb: The pcb of the process to run this function on 
// Returns: 
//     0 on success, -1 otherwise (eg: oldFd does not exist)
int dup2Func(int oldFd, int newFd, pcb *processPcb) {
    // If oldFd and newFd are the same, do nothing
    if (oldFd == newFd) return 0;
    
    char *fileName = NULL;
    int mode;
    uint32_t loc;
    // Get the file pointed to by oldFd
    lNode *currNode = processPcb -> fdTable -> head;
    while (currNode != NULL) {
        fdEntry *entry = (fdEntry*)(currNode -> payload);
        if ((entry -> fd) == oldFd) { // We have found the entry for oldFd
            fileName = entry -> fileName;
            mode = entry -> mode;
            loc = entry -> loc;
            break;
        }
        currNode = currNode -> next;
    }
    // If fileName is still null, then oldFd does not exist
    if (fileName == NULL) return -1;
    
    // Malloc a new string for the new entry and copy fileName over to the new 
    // string
    char *copy = malloc(strlen(fileName) + 1);
    strcpy(copy, fileName);
    // Find newFd and replace the file it points to with fileName, if newFd exists
    currNode = processPcb -> fdTable -> head;
    while (currNode != NULL) {
        fdEntry *entry = (fdEntry*)(currNode -> payload);
        if ((entry -> fd) == newFd) { 
            // Free the string that entry -> fileName currently points to
            free(entry -> fileName);
            // Set the new string
            entry -> fileName = copy;
            // Set the other fields so that all fields for the newFd entry are 
            // the same as those for the oldFd entry
            entry -> mode = mode;
            entry -> loc = loc; 

            return 0;
        }
        currNode = currNode -> next;
    }
    // If we get here, then newFd does not exist and needs to be created
    fdEntry *newEntry = malloc(sizeof(fdEntry));
    newEntry -> fd = newFd;
    newEntry -> fileName = copy;
    // Set the other fields so that all fields for the newFd entry are 
    // the same as those for the oldFd entry
    newEntry -> mode = mode;
    newEntry -> loc = loc; 
    // Add newEntry to fdTable
    addNodeTail(processPcb -> fdTable, newEntry);

    return 0;
}


// Function to implement p_waitpid 
// Arguments: 
//     pid: The pid of the process to wait on. If pid is -1, then the first child 
//     process of the calling process to undergo a state change is waited on 
//     wstatus: A pointer to an integer in which to store status information on 
//     the type of state change that occured 
//     nohang: An integer. If 0, the calling process is blocked, otherwise the 
//     calling process is not blocked
// Returns: 
//     pid of the child that changed status on success, and -1 otherwise 
int p_waitpid(pid_t pid, int *wstatus, int nohang) {
    lNode *currNode;
    // Run a while loop until a valid state change occurs
    while (1) {
        while(1) {
            currNode = currentProcessPcb -> stateChanges -> head;
            // If stateChanges is empty, block the calling process if nohang 0. 
            // Otherwise, return -1
            if (currNode == NULL) {
                if (!nohang) {
                    // Block the calling process
                    blockProcess(currentProcessPcb -> pid, waitpidBlocked, 0);
                    // Swap from calling thread to kernel thread
                    swapcontext(currentProcessPcb -> uc, kernelContext);
                } else {
                    return -1;
                }
            }
            currNode = currentProcessPcb -> stateChanges -> head;
            if (currNode != NULL) break;
        }
        if (pid == -1) { // If pid is -1, then just wait on the first change in 
                         // stateChanges
            
            stateChange *change = (stateChange*)(currNode -> payload);
            // Fill wstatus with the changeType field of change
            *wstatus = change -> changeType;
            int returnPid = change -> pid;
            
            // Remove change from stateChanges
            removeNode(currentProcessPcb -> stateChanges, currNode);
            // Call k_process_cleanup on the child process if it is a zombie
            if (*wstatus == TERM_CHANGE || *wstatus == EXIT_CHANGE) 
                k_process_cleanup(getPcb(returnPid));
            
            return returnPid;
        }
        // Search for a state change in stateChanges with the desired pid
        while (currNode != NULL) {
            stateChange *change = (stateChange*)(currNode -> payload);
            if ((change -> pid) == pid) {
                // Fill wstatus with the changeType field of change
                *wstatus = change -> changeType;
                // Remove change from stateChanges
                removeNode(currentProcessPcb -> stateChanges, currNode);
                // Call k_process_cleanup on the child process if it is a zombie
                if (*wstatus == TERM_CHANGE || *wstatus == EXIT_CHANGE) 
                    k_process_cleanup(getPcb(pid));
                return pid;
            }
            currNode = currNode -> next;
        }

        if (!nohang) {
            // If we get here, then nohang is false and no appropriate state change was 
            // found, so block the calling process
            blockProcess(currentProcessPcb -> pid, waitpidBlocked, 0);
            // Swap from calling thread to kernel thread
            swapcontext(currentProcessPcb -> uc, kernelContext);
        } else {
            return -1;
        }
    }

    return -1;
}


// Function to implement p_kill
// Arguments: 
//     pid: pid of the process to which to send the signal 
//     sig: The signal to send the specified process 
// Returns: 
//     0 on success, -1 otherwise (eg: the specified process does not exist/is dead)
int p_kill(int pid, int sig) {
    // Get the pcb of process pid
    pcb *processPcb = findProcess(pid);
    // If process does not exist, return -1
    if (processPcb == NULL) return -1;
    k_process_kill(processPcb, sig);

    return 0;
}


// Function to implement p_exit. Exits the calling process unconditionally
// Arguments: 
//     None 
// Returns: 
//     None 
void p_exit(void) {
    // Terminate the calling process
    terminateProcess(currentProcessPcb -> pid, EXIT_CHANGE);
    // Set context to the kernel context
    setcontext(kernelContext);
}


// Function to implement p_ps, the user level function for the ps function which
// lists the pid, ppid, state, and priority of all processes
// Arguments: 
//     None 
// Returns: 
//     None 
void p_ps(void) {
    // Call k_ps
    k_ps();
}


// Functions for the macros that return booleans based on the status returned by
// p_waitpid
int W_WIFEXITED(int status) {
    return status == EXIT_CHANGE;
}
int W_WIFSTOPPED(int status) {
    return status == STOP_CHANGE;
}
int W_WIFSIGNALED(int status) {
    return status == TERM_CHANGE;
}
int W_WIFCONTINUED(int status) {
    return status == CONT_CHANGE;
}


// Function to implement p_nice. Sets the priority of process pid to priority
// Arguments: 
//     pid: The pid of the process whose pid to set 
//     priority: The priority to set process pid to 
// Returns:
//     0 on success, -1 otherwise (eg: process pid does not exist)
int p_nice(int pid, int priority) {
    return k_p_nice(pid, priority);
}


// Function to implement p_sleep. Blocks the calling process until ticks of the 
// system clock elapse
// Arguments: 
//     ticks: The number of clock ticks to block the calling process for 
// Returns: 
//     None
void p_sleep(unsigned int ticks) {
    // Block the calling process
    blockProcess(currentProcessPcb -> pid, sleepBlocked, ticks);
    // Swap from calling thread to kernel thread
    swapcontext(currentProcessPcb -> uc, kernelContext);
}


// Function to implement f_open. Creates a new entry in the current process's 
// file descriptor table with the appropriate fields
// Arguments: 
//     fileName: Name of the file to open 
//     mode: The mode in which to open the file 
// Returns: 
//     File descriptor of the new file on success, -1 otherwise
int f_open(char *fileName, int mode) {
    // If mode is F_READ and file does not exist, return -1
    if (mode == F_READ && findFile(fileName) == -1)
        return -1;

    // If mode is F_WRITE and an instance of the file is already open with mode 
    // F_WRITE, return -1
    if (mode == F_WRITE) {
        lNode *currNode = currentProcessPcb -> fdTable -> head;
        while (currNode != NULL) {
            fdEntry *entry = (fdEntry*)(currNode -> payload);
            if (strcmp(entry -> fileName, fileName) == 0)
                if ((entry -> mode) == F_WRITE)
                    return -1;
            currNode = currNode -> next;
        }
        // If we get here, then truncate fileName if it exists, and create it 
        // otherwise
        deleteFile(fileName);
        createNewFile(fileName, REG_FILE, YYN);
    }

    // Create and initialize new fdTable entry
    fdEntry *entry = malloc(sizeof(fdEntry));
    char *str = malloc(strlen(fileName) + 1);
    strcpy(str, fileName);
    entry -> fd = getNewFd(currentProcessPcb);
    entry -> fileName = str;
    entry -> mode = mode;
    entry -> loc = 0;

    // If mode is F_APPEND, create the file if it does not exist. If it exists,
    // set loc in the fdTable entry to the end of the file
    if (mode == F_APPEND) {
        if (findFile(fileName) == -1) {
            createNewFile(fileName, REG_FILE, YYN);
        } else {
            entry -> loc = getFileSize(fileName);
        }
    }

    // Add new fdTable entry to fdTable
    addNodeTail(currentProcessPcb -> fdTable, entry);

    return entry -> fd;
}


// TODO: IMPLEMENT ABILITY TO ALLOW FILE OFFSET OF A FILE DESCRIPTOR TO BE 
// BEYOND THE END OF THE FILE (SEE MAN PAGE FOR LSEEK) FOR F_READ/F_WRITE. FOR 
// NOW, THE BEHAVIOR IS UNDEFINED IF FILE OFFSET IS BEYOND THE END OF A FILE
// TODO: Note that f_read essentially ignore the specification "returns
// 0 if EOF is reached"

// Function to implement f_read.
// Arguments: 
//     fd: File descriptor to read from 
//     buf: The buffer to put the read bytes in 
//     n: The number of bytes to read 
// Returns: 
//     Number of bytes read on success, -1 on failure (eg: fd does not exist)
int f_read(int fd, char *buf, int n) {
    // Find the fdTable entry associated with fd
    fdEntry *entry = NULL;
    lNode *currNode = currentProcessPcb -> fdTable -> head;
    while (currNode != NULL) {
        entry = (fdEntry*)(currNode -> payload);
        if ((entry -> fd) == fd) // We have found the desired entry
            break;
        currNode = currNode -> next;
    }
    // If currNode is null here, fd does not exist
    if (currNode == NULL) return -1;

    if (strcmp(entry -> fileName, "stdin") == 0) { // We need to read from stdin (ie:
                                                // the terminal)
        // If calling process is not the foreground process, send the calling
        // process a S_SIGSTOP signal and return -1
        if (foregroundProcessPid != currentProcessPid) {
            k_process_kill(currentProcessPcb, S_SIGSTOP);
            return -1;
        }
        return read(STDIN_FILENO, buf, n);
    } else { // We are reading from a file in the FAT filesystem
        // Read in the entire file
        uint32_t fileSize;
        char *fullFile = readFile(entry -> fileName, &fileSize);
        // Get file offset of fd
        uint32_t loc = entry -> loc;
        // Set n to the number of bytes to be read from fullFile (ie: set n to 
        // min(n, fileSize - loc))
        n = (n < fileSize - loc) ? n : fileSize - loc;
        // Read the bytes from fullFile to buf
        for (int i = 0; i < n; i++) 
            buf[i] = fullFile[loc + i];
        // Advance the loc field in entry
        entry -> loc += n;
        // Free fullFile
        free(fullFile);
        
        // Return the number of bytes read
        return n;
    }
}


// Function to implement f_write. 
// Arguments: 
//     fd: File descriptor of the file to write to 
//     str: The string from which to write to the target file 
//     n: The number of bytes to write 
// Returns: 
//     The number of bytes written on success, or -1 on error (eg: fd does not 
//     exist)
int f_write(int fd, char *str, int n) {
    // Find the fdTable entry associated with fd
    fdEntry *entry = NULL;
    lNode *currNode = currentProcessPcb -> fdTable -> head;
    while (currNode != NULL) {
        entry = (fdEntry*)(currNode -> payload);
        if ((entry -> fd) == fd) // We have found the desired entry
            break;
        currNode = currNode -> next;
    }
    // If currNode is null here, fd does not exist
    if (currNode == NULL) return -1;
    // If fd exists but is read only, return -1
    if ((entry -> mode) == F_READ) return -1;
    
    // If fd is associated with file stdout, then write to stdout
    if (strcmp(entry -> fileName, "stdout") == 0) 
        return write(STDOUT_FILENO, str, n);

    // Read in the full file
    uint32_t fileSize;
    char *fullFile = readFile(entry -> fileName, &fileSize);
    // Get file offset
    int loc = entry -> loc;
    // Calculate the final file size after the write is completed
    uint32_t finalSize = (n + loc < fileSize) ? fileSize : n + loc;
    // Allocate a buffer that will contain the final contents of the file
    char buffer[finalSize];
    // Write to buffer
    for (int i = 0; i < loc; i++)
        buffer[i] = fullFile[i];
    for (int i = 0; i < n; i++) 
        buffer[i + loc] = str[i];
    for (int i = loc + n; i < fileSize; i++) 
        buffer[i] = fullFile[i];
    // Write buffer to the file
    writeFile(entry -> fileName, buffer, finalSize, 0);
    // Advance loc field in entry
    entry -> loc += n;

    // Return number of bytes written
    return n;
}




// Function to implement f_close. Removes the associated fdTable entry from 
// fdTable
// Arguments: 
//     fd: File descriptor to close 
// Returns: 
//     0 on success, -1 on failure (eg: fd does not exist)
int f_close(int fd) {
    // Find entry associated with this file descriptor in fdTable
    lNode *currNode = currentProcessPcb -> fdTable -> head;
    while (currNode != NULL) {
        fdEntry *entry = (fdEntry*)(currNode -> payload);
        if ((entry -> fd) == fd) { // We have found the desired entry
            // Remove the entry from the file descriptor table
            free(entry -> fileName);
            removeNode(currentProcessPcb -> fdTable, currNode);
            return 0;
        }
        currNode = currNode -> next;
    }

    return -1;
}


// Function to implement f_unlink. TODO: CREATE CENTRAL TABLE WITH ALL FILES 
// AND ALL PROCESS' REFERENCES TO THEM TO KEEP TRACK OF WHETHER A FILE IS IN USE
// BY A PROCESS OR NOT
// TODO: IMPLEMENT F_UNLINK FUNCTION


// Function to implement f_lseek. 
// Arguments: 
//     fd: File descriptor of the file to seek in 
//     offset: The desired offset 
//     whence: The manner in which the seek is done 
// Returns: 
//     The file offset from the start of the file after the seek is performed on 
//     success, -1 otherwise
int f_lseek(int fd, int offset, int whence) {
    // Find the fdTable entry for the file descriptor
    lNode *currNode = currentProcessPcb -> fdTable -> head;
    while (currNode != NULL) {
        fdEntry *entry = (fdEntry*)(currNode -> payload);
        if ((entry -> fd) == fd) { // We have found the desired fdTable entry
            // Set the loc field in entry based on whence
            if (whence == F_SEEK_SET) 
                entry -> loc = offset;
            else if (whence == F_SEEK_CUR) 
                entry -> loc += offset;
            else if (whence == F_SEEK_END)
                entry -> loc = offset + getFileSize(entry -> fileName);
            
            // Return the offset from the beginning of the file
            return entry -> loc;
        }
        currNode = currNode -> next;
    }

    // If we get here, then we know fd does not exist
    return -1;
}


// TODO: IMPLEMENT F_LS









// TODO: MAKE PARENT PROCESS RECEIVE A STATE CHANGE OF CHANGETYPE EXIT_CHANGE WHEN
// CHILD THREAD RETURNS NORMALLY, EVEN WHEN CHILD THREAD DOES NOT CALL P_EXIT()









