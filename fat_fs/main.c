#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mkfs.h"
#include "touch.h"

#define MAX_SIZE 5000

int main(void) {
    char *fsName = "fs";
    char *bitmap = NULL;
    mkfs(fsName, 1, 0, bitmap);
    ls();
}



