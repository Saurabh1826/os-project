#include "shell.h"

// Function to read contents in buffer into an array of strings
char** readInput(char *buffer, int *argn) {
    int flag = 1;
    int cnt = 0;
    char **ptr;

    buffer[MAX_LEN-1] = '\0';
    for (int i = 0; i < MAX_LEN; i++) {
        if (buffer[i] == EOF || buffer[i] == '\n') {
            buffer[i] = '\0';
            break;
        }
        if (buffer[i] == ' ' || buffer[i] == '\t') {
            flag = 1;
            buffer[i] = '\0';
        } else {
            cnt = (flag) ? cnt + 1 : cnt;
            flag = 0;
        }
    }
    
    if (cnt == 0) {
        *argn = 0;
        return NULL;
    }
    *argn = cnt;
    if ((ptr = malloc((cnt + 1) * sizeof(char*))) == NULL) {
        perror("Malloc error");
        exit(EXIT_FAILURE);
    }
    flag = 1;
    int ind = 0;
    for (int i = 0; i < MAX_LEN; i++) {
        if (ind == cnt) break;
        if (buffer[i] != '\0') {
            if (!flag) continue;
            ptr[ind] = &buffer[i];
            flag = 0;
            ind++;
        } else {
            flag = 1;
        }
    }
    ptr[cnt] = NULL;
    return ptr;
}