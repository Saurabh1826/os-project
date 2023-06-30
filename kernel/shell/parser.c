#include "shell.h"
#include <string.h>

char*** parse(char **inStrs, int argn, int *numC, char*(**redirections)[], int *bg, int *append) {
    // Get numbers of commands
    int numCommands = 1;
    for (int i = 0; i < argn; i++) {
        numCommands += (inStrs[i][0] == '|');
        if (inStrs[i][0] == '&') {
            *bg = 1;
            argn -= 1;
        }
    }
    
    // Handle redirections (redirects[i][0] is stdin, redirects[i][1] is stdout)
    char* (*redirects)[2];
    if ((redirects = malloc(sizeof(char*[numCommands][2]))) == NULL)
        perror("Malloc error");
    int numArgs[numCommands]; // Array that holds number of arguments for each command
    for (int i = 0; i < numCommands; i++) {
        redirects[i][0] = redirects[i][1] = NULL;
        numArgs[i] = 0;
    }
    int ind = 0;
    for (int i = 0; i < argn; i++) {
        if (inStrs[i][0] == '|') {
            ind += 1;
            continue;
        }

        if (inStrs[i][0] == '<') {
            redirects[ind][0] = inStrs[i+1];
            i += 1;
        } else if (inStrs[i][0] == '>') {
            redirects[ind][1] = inStrs[i+1];
            
            // Check if the redirection is for an append because we parsed '>>'
            if (strlen(inStrs[i]) > 1 && inStrs[i][1] == '>') 
                *append = 1;
            else 
                *append = 0;
            
            i += 1;
        } else {
            numArgs[ind] += 1;
        }
    }
    
    // Create final array of array of strings for the commands
    char*** args;
    if ((args = malloc(numCommands * sizeof(char**))) == NULL)
        perror("Malloc error");
    ind = 0;
    int i = 0;
    while (i < argn) {
        char **arr;
        if ((arr  = malloc((numArgs[ind] + 1) * sizeof(char*))) == NULL)
            perror("Malloc error");
        int tempInd = 0;
        while (i < argn && inStrs[i][0] != '|') {
            if (inStrs[i][0] == '<' || inStrs[i][0] == '>') {
                i += 1;
            } else {
                arr[tempInd] = inStrs[i];
                tempInd += 1;
            }
            i += 1;
        }
        arr[numArgs[ind]] = NULL;
        args[ind] = arr;
        ind += 1;
        i += 1;
    }

    *redirections = redirects;
    *numC = numCommands;
    return args;
}


