#include "shell.h"
#include "read.h"
#include "parser.h"
#include "createChild.h"
#include "queue.h"
#include "jobControl.h"
#include "../userFunctions.h"
#include "../kernel.h"

#define PROMPT "~/$ "

void shellHandler(int);
void registerHandlers(void);
void displayPrompt(void);
char *getJobName(char*);
void copyStr(char*, char*);

// Variable for the job queue
static queue *jobQueue;

extern int foregroundProcessPid;
extern int currentProcessPid;

void shell(void) {

    int fd = open("log", O_RDWR | O_CREAT, 0644);

	// Register alarm, child, and sigint handlers
	registerHandlers();
    
	// Display prompt
	displayPrompt();

    // Create job queue
    jobQueue = createQueue();

    // Read input to the shell
	char buffer[MAX_LEN];
	while(1) {
        f_read(0, buffer, MAX_LEN);
        int argn = 0;
        char *jobNameStr = getJobName(buffer);
		char **args = readInput(buffer, &argn);
        int pid;
        int builtInCommand = 0;
        int print = 0;

        
        
        if (args == NULL) {
            free(jobNameStr);
            builtInCommand = 1;
        }
        
        // Check for exit or job control commands
        if (argn == 1 && !strcmp(EXIT, args[0])) {
            // TODO: IMPLEMENT PROPERLY
            exit(0);
        }
        if (argn == 1 && !strcmp(JOBS, args[0])) {
            print = 1;
            free(jobNameStr);
            builtInCommand = 1;
        }
        if (argn == 2 && !strcmp(KILL, args[0])) { 
            killJob(jobQueue, atoi(args[1]));
            free(jobNameStr);
            builtInCommand = 1;
        }
        if (argn == 2 && !strcmp(FG, args[0])) {
            foregroundJob(jobQueue, atoi(args[1]));
            free(jobNameStr);
            builtInCommand = 1;
        }
        if (argn == 2 && !strcmp(BG, args[0])) {
            backgroundJob(jobQueue, atoi(args[1]));
            // foregroundPGID = getpgid(0);
            free(jobNameStr);
            builtInCommand = 1;
        }
        if (argn == 3 && !strcmp("nice_pid", args[0])) {
            p_nice(atoi(args[2]), atoi(args[1]));
            builtInCommand = 1;
        }

        // Parse input to get number of commands, arguments for each command, and redirections
        int numCommands;
        char ***argsArr;
        char* (*redirects)[2];
        int inRedirect, outRedirect;
        int append;
        
        if (!builtInCommand) {
            int bg = 0; // 0 indicates job should run in foreground, 1 indicates it should run in background
            argsArr = parse(args, argn, &numCommands, &redirects, &bg, &append);

            // Handle first/last command redirection
            if (redirects[0][0] == NULL) {
                inRedirect = -1;
            } else {
                if ((inRedirect = f_open(redirects[0][0], F_READ)) == -1) {
                    printf("%s: No such file or directory\n", redirects[0][0]);
                    displayPrompt(); 
                    continue;
                }
            }
            
            if (redirects[numCommands-1][1] == NULL) {
                outRedirect = -1;
            } else {
                if (append)
                    outRedirect = f_open(redirects[numCommands-1][1], F_APPEND);
                else 
                    outRedirect = f_open(redirects[numCommands-1][1], F_WRITE);
            }
            
            // Create children and necessary pipes/redirections
            pid = createChild(numCommands, inRedirect, outRedirect, argsArr);

            // Add this job to the job queue
            qNode *job = createNode(RUNNING_NUM, jobNameStr, pid);
            addNode(jobQueue, job);
            
            // Run the new job in foreground or background
            if (!bg) {
                foregroundProcessPid = pid;
            } else {
                printf("[%d]  %d\n", jobQueue -> tail -> jobNum, pid);
            }
        }

        // If shell is in background, wait for foreground process to complete
        // before continuing
        int status;
        int childPid = -1;
        
        if (foregroundProcessPid != currentProcessPid) {
            childPid = p_waitpid(foregroundProcessPid, &status, 0);

            // If the child was in the foreground and underwent a state change (
            // became stopped, terminated, or exited), then shell needs to regain
            // terminal control
            foregroundProcessPid = currentProcessPid;

            // Update the jobs queue appropriately based on the state change of the
            // child we just waited on
            int jobNum = findJobPID(jobQueue, childPid);
            if (W_WIFSTOPPED(status)) {
                // Change the job state of the stopped child
                changeJobState(jobQueue, jobNum, STOPPED_NUM);
                setCurrJob(jobQueue, jobNum);

                // Print message about job state change 
                printf("[%d]  %s  %s\n", jobNum, "Stopped", findJob(jobQueue, jobNum) -> jobName);
            } else if (W_WIFEXITED(status) || W_WIFSIGNALED(status)) {
                qNode *node = findJob(jobQueue, jobNum);
                if (node == NULL)
                    continue;
                
                // Remove job from job queue
                removeNodeJobControl(jobQueue, jobNum);
            }
        }
        
        // If childPid is -1, then we just started a job in the background or we 
        // are running a shell command, so wait for any child process before 
        // continuing
        if (childPid == -1) {

            childPid = p_waitpid(-1, &status, 1);

            if (childPid != -1) { // We have waited on a child
                int jobNum = findJobPID(jobQueue, childPid);
                if (W_WIFSTOPPED(status)) {
                    changeJobState(jobQueue, jobNum, STOPPED_NUM);
                    setCurrJob(jobQueue, jobNum);
                    
                    printf("[%d]  %s  %s\n", jobNum, "Stopped", findJob(jobQueue, jobNum) -> jobName);
                } else if (W_WIFCONTINUED(status)) {
                    changeJobState(jobQueue, jobNum, RUNNING_NUM);
                } else if (W_WIFEXITED(status)) {
                    qNode *node = findJob(jobQueue, jobNum);
                    if (node == NULL)
                        continue;
                    write(fd, "hh", 2);
                    printf("[%d]  %s  %s\n", jobNum, "Done", node -> jobName);
                    write(fd, "hh", 2);
                    // Remove job from job queue
                    removeNodeJobControl(jobQueue, jobNum);
                } else if (W_WIFSIGNALED(status)) {
                    qNode *node = findJob(jobQueue, jobNum);
                    if (node == NULL)
                        continue;
                        
                    printf("[%d]  %s  %s\n", jobNum, "Killed", node -> jobName);
                    // Remove job from job queue
                    removeNodeJobControl(jobQueue, jobNum);
                }

            }
        }

        // Close the files used for redirects
        f_close(inRedirect);
        f_close(outRedirect);

        if (print) {
            printJobs(jobQueue);
        }
        
        if (!builtInCommand) {
            for (int i = 0; i < numCommands; i++) {
                free(argsArr[i]);
            }
            free(argsArr);
            free(redirects);
        }
        free(args);
        displayPrompt();
	}
}


void registerHandlers() {
	// TODO: IMPLEMENT
}

void shellHandler(int signum) {
	if (signum == SIGINT || signum == SIGTSTP) {
        if (write(STDOUT_FILENO, "\n", 1) == -1) perror("Write error");
		displayPrompt();
	}
}

void displayPrompt() {
	if (write(STDOUT_FILENO, PROMPT, strlen(PROMPT)) == -1) perror("Write error");
}

char *getJobName(char *buffer) {
    int len = 0;
    char *p = buffer;
    while (*p++ != '\n')
        len++;
    char *name = malloc(len * sizeof(char) + 1);
    int ind = 0;
    while (*buffer != '\n') {
        name[ind] = *buffer++;
        ind++;
    }
    name[len] = '\0';
    return name;
}

void copyStr(char *src, char *dst) {
    for (int i = 0; i < strlen(dst); i++) {
        src[i] = dst[i];
    }
    src[strlen(dst)] = '\0';
}







