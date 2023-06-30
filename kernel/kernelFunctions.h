#ifndef KERNEL_FUNCTIONS_H
#define KERNEL_FUNCTIONS_H

#include "kernel.h"

pcb *k_process_create(pcb *parentPcb);
pcb *k_process_create2(int ppid, int priority, int state);
void createFdEntry(int pid, int fd, char *fileName, int mode);
int getNewFd(pcb *processPcb);
int k_process_kill(pcb *processPcb, int signal);
int terminateProcess(int pid, int type);
void blockProcess(int pid, linkedList *list, int ticks);
int unblockProcess(int pid);
void addStateChange(int ppid, int pid, int changeType);
int isWaitpidBlocked(int pid);
int isSleepBlocked(int pid);
void removeFromBlockedList(int pid, linkedList *blockedList);
void k_process_cleanup(pcb *processPcb);
pcb *findProcess(int pid);
pcb *getPcb(int pid);
pcb *scheduler();
void addToScheduler(int pid, int priority);
void removeFromScheduler(int pid, int priority);
int k_p_nice(int pid, int priority);
void k_ps(void);


#endif