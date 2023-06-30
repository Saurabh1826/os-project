#ifndef QUEUE_H
#define QUEUE_H

#define STOPPED_STR "Stopped"
#define RUNNING_STR "Running"
#define FINISHED_STR "Done"
#define STOPPED_NUM 0
#define RUNNING_NUM 1
#define FINISHED_NUM 2

typedef struct qNode {
    int jobNum;
    int state; // 0 is stopped, 1 is running
    int pid;
    char *jobName;
    struct qNode *next;
} qNode;

typedef struct queue {
    qNode *head;
    qNode *tail;
    int length;
    int currJob;
    int prevCurrJob;
} queue;

void addNode(queue *q, qNode *node);
int removeNodeJobControl(queue *q, int job);
int setCurrJob(queue *q, int newCurr);
int changeJobState(queue *q, int job, int newState);
qNode* findJob(queue *q, int job);
int findJobPID(queue *q, int pid);
void printQueue(queue *q);
qNode* createNode(int state, char *jobName, int pid);
queue* createQueue();
void freeQueue(queue *q);

#endif