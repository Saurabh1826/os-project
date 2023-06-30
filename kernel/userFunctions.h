#ifndef USER_FUNCTIONS_H
#define USER_FUNCTIONS_H

#include "kernel.h"

int p_spawn(void (*func)(), char *argv[], int fd0, int fd1);
int dup2Func(int oldFd, int newFd, pcb *processPcb);
int p_waitpid(pid_t pid, int *wstatus, int nohang);
int p_kill(int pid, int sig);
void p_exit(void);
void p_ps(void);
int W_WIFEXITED(int status);
int W_WIFSTOPPED(int status);
int W_WIFSIGNALED(int status);
int W_WIFCONTINUED(int status);
int p_nice(int pid, int priority);
void p_sleep(unsigned int ticks);
int f_open(char *fileName, int mode);
int f_read(int fd, char *buf, int n);
int f_write(int fd, char *str, int n);
int f_close(int fd);
int f_lseek(int fd, int offset, int whence);



#endif