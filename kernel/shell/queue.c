#include "shell.h"
#include "queue.h"

void addNode(queue *q, qNode *node) {
    if (q -> head == NULL) {
        q -> head = node;
        q -> tail = node;
        node -> jobNum = 1;
        q -> length = 1;
        q -> currJob = 1;
        return;
    }
    q -> tail -> next = node;
    node -> jobNum = q -> tail -> jobNum + 1;
    q -> tail = node;
    q -> length += 1;
    q -> prevCurrJob = q -> currJob;
    q -> currJob = node -> jobNum;
}

// Returns -1 if no such job is in the queue, or if queue is empty
int removeNodeJobControl(queue *q, int job) {
    int ret = -1;
    qNode *node = q -> head;
    qNode *prev = NULL;
    while (node != NULL && (node -> jobNum) != job) {
        prev = node;
        node = node -> next;
    }
    if (node != NULL) {
        ret = 0;
        q -> length -= 1;
        if (prev == NULL) {
            q -> head = node -> next;
            if (q -> head == NULL) {
                q -> tail = NULL;
            } else {
                if (q -> currJob == job) {
                    q -> currJob = (q -> prevCurrJob != -1) ? q -> prevCurrJob : q -> head -> jobNum;
                    q -> prevCurrJob = -1;
                }
            }
        } else {
            prev -> next = node -> next;
            if (q -> tail == node)
                q -> tail = prev;
            if (q -> currJob == job) {
                q -> currJob = (q -> prevCurrJob != -1) ? q -> prevCurrJob : q -> head -> jobNum;;
                q -> prevCurrJob = -1;
            }
        }
        // printf("%s\n", node -> jobName);
        free(node -> jobName);
        free(node);
    }
    return ret;
}

// Returns -1 on error (ie: if newCurr is not a job in q)
int setCurrJob(queue *q, int newCurr) {
    int ret = -1;
    qNode *node = findJob(q, newCurr);
    if (node != NULL) {
        ret = 0;
        q -> prevCurrJob = q -> currJob;
        q -> currJob = newCurr;
    }
    return ret;
}

int changeJobState(queue *q, int job, int newState) {
    int ret = -1;
    qNode *node = findJob(q, job);
    if (node != NULL) {
        ret = 0;
        node -> state = newState;
    }
    return ret;
}

// Find job with given job number. If the job doesn't exist, return NULL
qNode* findJob(queue *q, int job) {
    qNode *node = q -> head;
    while (node != NULL && (node -> jobNum) != job)
        node = node -> next;
    return node;
}

// Find job number of a job from its pid. If job doesn't exist, returns -1
int findJobPID(queue *q, int pid) {
    int ret = -1;
    qNode *node = q -> head;
    while (node != NULL && (node -> pid) != pid)
        node = node -> next;
    if (node != NULL)
        ret = node -> jobNum;
    return ret;
}

void printQueue(queue *q) {
    qNode *node = q -> head;
    while(node != NULL) {
        char *state = ((node -> state) == STOPPED_NUM) ? STOPPED_STR : RUNNING_STR;
        if (q -> currJob == node -> jobNum) {
            printf("[%d]+  %s    %s\n", node -> jobNum, state, node -> jobName);
        } else {
            printf("[%d]  %s    %s\n", node -> jobNum, state, node -> jobName);
        }
        node = node -> next;
    }
}

qNode* createNode(int state, char *jobName, int pid) {
    qNode *node = malloc(sizeof(qNode));
    node -> state = state;
    node -> jobName = jobName;
    node -> pid = pid;
    node -> next = NULL;
    return node;
}

queue* createQueue() {
    queue *q = malloc(sizeof(queue));
    q -> head = NULL;
    q -> tail = NULL;
    q -> length = 0;
    q -> currJob = q -> prevCurrJob = -1;
    return q;
}

void freeQueue(queue *q) {
    qNode *node = q -> head;
    qNode *prev = NULL;
    while (node != NULL) {
        prev = node;
        free(node -> jobName);
        node = node -> next;
        free(prev);
    }
    free(q);
}

