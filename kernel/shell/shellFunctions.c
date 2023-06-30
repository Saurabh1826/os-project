#include <string.h>
#include "shell.h"
#include "../kernel.h"
#include "../userFunctions.h"
#include "../fat_fs/touch.h"
#include "shellFunctions.h"

#define MAX_ARGS 100

// TODO: UPDATE COMMENTS

// Function to implement the shell built in cat. Is a wrapper for the FAT 
// filesystem cat function
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellCat(char *args[]) {
    int n = 0;
    while (args[n++] != NULL) ;
    n -= 2;
    char *fileNames[n];

    for (int i = 0; i < n; i++) 
        fileNames[i] = args[1 + i];
    
    // Call cat function
    cat(fileNames, n, 0, NULL);

    p_exit();
}


// Function to implement the shell built in sleep. Is a wrapper for p_sleep
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellSleep(char *args[]) {

    // Call p_sleep
    // printf("%d\n", atoi(args[1]));
    p_sleep(atoi(args[1]));

    p_exit();
}


// Function to implement the shell built in busy. Note that the arguments are 
// not used
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellBusy(char *args[]) {
    while (1) ;

    p_exit();
}


// Function to implement the shell built in echo. 
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellEcho(char *args[]) {
    int ind = 0;

    while (args[ind] != NULL) {
        if (ind > 0) {
            f_write(1, args[ind], strlen(args[ind]));
            f_write(1, " ", 1);
        }
        ind += 1;
    }
    f_write(1, "\n", 1);

    p_exit();
}


// Function to implement the shell built in ls. Is a wrapper for FAT filesystem
// function ls. Note that the arguments are not used
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellLs(char *args[]) {
    ls();

    p_exit();
}


// Function to implement the shell built in touch. Is a wrapper for FAT filesystem
// function touch 
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellTouch(char *args[]) {
    int n = 0;
    while (args[n++] != NULL) ;
    n -= 2;
    char *fileNames[n];

    for (int i = 0; i < n; i++) 
        fileNames[i] = args[1 + i];
    
    // Call touch function
    touch(fileNames, n);

    p_exit();
}


// Function to implement the shell built in mv. Is a wrapper for FAT filesystem
// function mv 
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellMv(char *args[]) {
    // Call mv function
    mv(args[1], args[2]);

    p_exit();
}


// Function to implement the shell built in cp. Is a wrapper for FAT filesystem
// function cp 
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellCp(char *args[]) {
    // Call cp function
    cp(args[1], args[2], 0);

    p_exit();
}


// Function to implement the shell built in rm. Is a wrapper for FAT filesystem
// function rm 
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellRm(char *args[]) {
    int ind = 0;
    while (args[ind] != NULL) {
        if (ind > 0) {
            deleteFile(args[ind]);
        }
        ind += 1;
    }
    
    p_exit();
}


// Function to implement the shell built in chmod. Is a wrapper for FAT filesystem
// function chmod 
// Arguments: 
//     s: A dummy argument whose value will be null (because there must be at least 
//     one named argument before ...)
//     ...: An arbitrary number of strings that are the command line arguments 
//     the function was called with (excluding the command name)
// Returns: 
//     None
void shellChmod(char *args[]) {
    // Call chmod function
    chmod(args[1], atoi(args[2]));

    p_exit();
}


void shellPs(char *args[]) {
    // Call p_ps
    p_ps();

    p_exit();
}


void shellKill(char *args[]) {
    // Call p_kill
    p_kill(atoi(args[2]), atoi(args[1]));

    p_exit();
}


void shellZombify(char *args[]) {
    p_spawn(&zombieChild, args, 0, 1);
    while (1) ;

    p_exit();
}


void zombieChild(char *args[]) {
    p_exit();
}


void shellOrphanify(char *args[]) {
    p_spawn(&orphanChild, args, 0, 1);

    p_exit();
}


void orphanChild(char *args[]) {
    while (1);

    p_exit();
}
