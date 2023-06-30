#ifndef PENN_SHELL_H 
#define PENN_SHELL_H 

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>

#define CATCHPHRASE "hello"
#define NO_FILE "No such file or directory"
#define MAX_LEN 4096
#define MAX_NUM_PROCESSES 5000
#define USAGE "USAGE: ./penn-shredder [TIMEOUT]\n"
#define EXIT "exit"
#define JOBS "jobs"
#define FG "fg"
#define BG "bg"
#define KILL "kill"

void shell(void);

#endif