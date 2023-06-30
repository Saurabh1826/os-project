#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

void printJobs(queue *q);
void killJob(queue *q, int jobNum);
void foregroundJob(queue *q, int jobNum);
void backgroundJob(queue *q, int jobNum);

#endif