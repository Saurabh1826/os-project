#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "headers.h"

// Name of the filesystem
char *fsName;
// Bitmap for keeping track of free blocks in the filesystem
char *bitmap;
// Number of blocks in the data region of the filesystem (the FAT has numBlocks+1 entries)
int numBlocks;
// Size of FAT region of the filesystem
int fatSize;
// Size of blocks in the filesystem
int blockSize;
// Array to convert from integer to month name
const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int mkfs(char *fs_Name, int blocksInFat, int blockSizeConfig, char *bitmapPointer) {

    // Check for valid parameters
    if (blockSizeConfig < 0 || blockSizeConfig > 4 || blocksInFat <= 0 || blocksInFat > 32)
        return -1;
    
    fsName = fs_Name;

    int numEntries, fsSize;
    switch (blockSizeConfig) {
        case 0: blockSize = 256; break;
        case 1: blockSize = 512; break;
        case 2: blockSize = 1024; break;
        case 3: blockSize = 2048; break;
        default: blockSize = 4096;
    }

    fatSize = blockSize * blocksInFat; // Size of FAT
    numEntries = (int) (fatSize / 2); // Number of entries in FAT
    numBlocks = numEntries - 1;
    fsSize = fatSize + blockSize * numBlocks; // Total size of filesystem file

    // Create new file for filesystem and fill with 0's, except for first two FAT entries
    char buffer[4];
    int fd = open(fsName, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    // Write first entry of FAT
    buffer[0] = (char) blocksInFat;
    buffer[1] = (char) blockSizeConfig;
    // Write second entry of FAT
    buffer[2] = buffer[3] = (char) 255;
    write(fd, buffer, 4);

    buffer[0] = 0;
    for (int i = 0; i < fsSize - 4; i++) {
        write(fd, buffer, 1);
    }

    // Create bitmap to keep track of free space (implemented as char arr)
    bitmap = malloc((numEntries - 1) * sizeof(char));
    // Initialize bitmap
    for (int i = 0; i < numEntries - 1; i++)
        bitmap[i] = FREE;

    // Mark block 1 as occupied (it is occupied by the root directory)
    bitmap[0] = OCCUPIED;
    bitmapPointer = bitmap;
    
    return 0;
}

// TODO:
// 1. ADD ALL ERROR CHECKING