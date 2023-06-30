#include "shell.h"
#include "queue.h"
#include "../userFunctions.h"


extern int foregroundProcessPid;

// TODO: CHANGE ALL PRINTF'S TO F_WRITE'S

void printJobs(queue *q) {
    printQueue(q);
}

void killJob(queue *q, int jobNum) {
    qNode *node = findJob(q, jobNum);
    if (node == NULL) {
        printf("kill %d: no such job\n", jobNum);
        return;
    }
    
    if (q -> currJob == jobNum) 
        printf("[%d]+  Terminated   %s\n", jobNum, node -> jobName);
    else 
        printf("[%d]  Terminated    %s\n", jobNum, node -> jobName);
    // Send SIGKILL signal to the process 
    p_kill(node -> pid, S_SIGTERM);
    removeNodeJobControl(q, jobNum);
}

// Function to foreground a job in the job queue
void foregroundJob(queue *q, int jobNum) {
    qNode *node = findJob(q, jobNum);
    if (node == NULL) {
        printf("fg %d: no such job\n", jobNum);
        return;
    }
    // Send SIGCONT signal to the process  
    if (p_kill(node -> pid, S_SIGCONT) == -1) {
        printf("fg: job has been terminated");
        return;
    }
    
    // Update status of job
    changeJobState(q, jobNum, RUNNING_NUM);
    printf("%s\n", node -> jobName);
    // Move process to foreground
    foregroundProcessPid = node -> pid;
}

// Function to background a job in the job queue
void backgroundJob(queue *q, int jobNum) {
    qNode *node = findJob(q, jobNum);
    if (node == NULL) {
        printf("bg %d: no such job\n", jobNum);
        return;
    }
    
    // Send SIGCONT signal to the process group 
    if (p_kill(node -> pid, S_SIGCONT) == -1) {
        printf("bg: job has been terminated");
        return;
    }
    // Update status of job
    changeJobState(q, jobNum, RUNNING_NUM);
    
    
    if (q -> currJob == jobNum) 
        printf("[%d]+  %s\n", node -> jobNum, node -> jobName);
    else 
        printf("[%d]  %s\n", node -> jobNum, node -> jobName);
}