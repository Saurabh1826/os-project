#include <string.h>
#include "shell.h"
#include "shellFunctions.h"
#include "../kernel.h"
#include "../userFunctions.h"

// Creates child process. Returns pid of the newly created process
int createChild(int numCommands, int inRedirect, int outRedirect, char*** args) {
    // Variable that is the pointer to the function that will be run by the
    // child thread
    void *f = NULL; 
    // Handle redirection
    int fd0 = 0, fd1 = 1;
    if (inRedirect != -1)
        fd0 = inRedirect;
    if (outRedirect != -1)
        fd1 = outRedirect;
    
    // Assign f based on the input command
    if (strcmp(args[0][0], "cat") == 0) {
        f = &shellCat;
    } else if (strcmp(args[0][0], "sleep") == 0) {
        f = &shellSleep;
    } else if (strcmp(args[0][0], "busy") == 0) {
        f = &shellBusy;
    } else if (strcmp(args[0][0], "echo") == 0) {
        f = &shellEcho;
    } else if (strcmp(args[0][0], "ls") == 0) {
        f = &shellLs;
    } else if (strcmp(args[0][0], "touch") == 0) {
        f = &shellTouch;
    } else if (strcmp(args[0][0], "mv") == 0) {
        f = &shellMv;
    } else if (strcmp(args[0][0], "cp") == 0) {
        f = &shellCp;
    } else if (strcmp(args[0][0], "rm") == 0) {
        f = &shellRm;
    } else if (strcmp(args[0][0], "chmod") == 0) {
        f = &shellChmod;
    } else if (strcmp(args[0][0], "ps") == 0) {
        f = &shellPs;
    } else if (strcmp(args[0][0], "kill") == 0) {
        f = &shellKill;
    } else if (strcmp(args[0][0], "zombify") == 0) {
        f = &shellZombify;
    } else if (strcmp(args[0][0], "orphanify") == 0) {
        f = &shellOrphanify;
    } else {

    }

    int n = 0;
    while (args[0][n++] != NULL) ;
    char **argv = malloc(n * sizeof(char*));
    for (int i = 0; i < n - 1; i++) {
        argv[i] = malloc(strlen(args[0][i]) + 1);
        strcpy(argv[i], args[0][i]);
    }
    argv[n - 1] = NULL;

    // Spawn the child process
    // return p_spawn(f, args[0], fd0, fd1);
    return p_spawn(f, argv, fd0, fd1);
}


