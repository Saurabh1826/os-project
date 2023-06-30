#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include "headers.h"
#include "../userFunctions.h"

// Name of the filesystem
extern char *fsName;
// Bitmap for keeping track of free blocks in the filesystem
char *bitmap;
// Number of blocks in the data region of the filesystem (the FAT has numBlocks+1 entries)
extern int numBlocks;
// Size of FAT region of the filesystem
extern int fatSize;
// Size of blocks in the filesystem
extern int blockSize;
// Array to convert from integer to month name
extern const char *months[12];

// Function to find the first free block in the data region of the filesystem. 
// Arguments: 
//     
// Returns: 
//     -1 on error (eg: there are no free blocks), or the number of the first free 
//     block otherwise
int findFreeBlock() {
    for (int i = 0; i < numBlocks; i++) {
        // Block 0xffff is invalid
        if (i == 0xffff - 1) continue;
        if (bitmap[i] == FREE) 
            return i + 1;
    }
    return -1;
}

// Function to add another block to a file
// Arguments: 
//     lastBlock: Number of the last block of the file being extended. If lastBlock
//      is -1, the function will allocate the new block as the first block in 
//      a file
// Returns: 
//     -1 on error (eg: there are no free blocks), or the number of the new block 
//     otherwise
int addBlock(int lastBlock) {
    int newBlock = findFreeBlock();
    // There are no free blocks
    if (newBlock == -1)
        return -1;

    // Set the new entries in the FAT
    uint16_t entry1 = newBlock;
    uint16_t entry2 = 0xffff;

    int fd = open(fsName, O_RDWR);

    // Write entry for lastBlock in FAT
    if (lastBlock > 0) {
        lseek(fd, lastBlock * 2, SEEK_SET);
        write(fd, &entry1, sizeof(uint16_t));
    }

    // Write entry for newBlock in FAT
    lseek(fd, newBlock * 2, SEEK_SET);
    write(fd, &entry2, sizeof(uint16_t));

    close(fd);

    // Mark newBlock as occupied 
    bitmap[newBlock - 1] = OCCUPIED;

    return newBlock;
}


// Function to create new directory entry for a file. Assumes the file does not
// exist
// Arguments: 
//     fileName: Name of file
//     type: Type of the file
//     perm: File permissions
// Returns: 
//     -1 on failure, 0 on success
int createNewFile(char *fileName, uint8_t type, uint8_t perm) {
    // Check if fileName is valid. ALSO NEED TO CHECK IF THE ACTUAL NAME IS VALID
    if (strlen(fileName) > 32)
        return -1;

    // Allocate first block of file in the data region of the filesystem
    int firstBlock = addBlock(-1);

    int fd = open(fsName, O_RDWR);

    // Create the directory entry 
    dirEntry entry;
    strcpy(entry.name, fileName);
    entry.size = 0;
    entry.firstBlock = (uint16_t) firstBlock;
    entry.type = type;
    entry.perm = perm;
    entry.mtime = time(NULL);

    // Go through all the entries of the root directory, checking if there is a 
    // deleted entry to replace. If not, check if there is space in the last block
    // of the directory to add a new entry. Otherwise, add a new block to the 
    // directory and add a new entry there
    int currBlock = 1; // Current block we are at in the directory
    int currLoc = 0; // Current location in the current block (in bytes)
    int created = 0; // Boolean to identify whether the directory entry has been 
                     // created yet
    while (currBlock != 0xffff) {
        while (currLoc < blockSize) {
            // seek to beginning of current directory entry
            lseek(fd, fatSize + (currBlock - 1) * blockSize + currLoc, SEEK_SET);
            // Inspect first byte of the directory entry
            char id;
            read(fd, &id, 1);

            // If we are at a deleted entry or the end of the directory, we can 
            // create the new directory entry here
            if (id == 0 || id == 1) {
                lseek(fd, fatSize + (currBlock - 1) * blockSize + currLoc, SEEK_SET);
                // Write the directory entry
                write(fd, &entry, DIR_ENTRY_SIZE);
                // Reset end of directory if we overwrote previous end of directory
                if (id == 0) {
                    // If we are at the end of the block, we need to add a new block
                    // to the directory
                    if (currLoc + DIR_ENTRY_SIZE >= blockSize) {
                        currBlock = addBlock(currBlock);
                        if (currBlock == -1) {
                            close(fd);
                            return -1;
                        }
                        // Seek to beginning of new block
                        lseek(fd, fatSize + (currBlock - 1) * blockSize, SEEK_SET);
                        // Write 0 to first byte of the new block
                        write(fd, &id, 1);
                    } else {
                        write(fd, &id, 1);
                    }
                }
                created = 1;
            }

            if (created) break;
            currLoc += DIR_ENTRY_SIZE;
        }
        if (created) break;
        // Get next block in the directory from FAT
        lseek(fd, currBlock * 2, SEEK_SET);
        uint16_t nextBlock;
        read(fd, &nextBlock, sizeof(uint16_t));
        // If there are no more blocks in the file, we need to add a block
        if (nextBlock == (uint16_t) 0xfff) {
            currBlock = addBlock(currBlock);
        } else {
            currBlock = nextBlock;
        }

        if (currBlock == -1) {
            close(fd);
            return -1;
        }
        currLoc = 0;
    }

    close(fd);

    return 0;
}

// Function to find a file in the root directory
// Arguments: 
//     fileName: Name of the file to be found
// Returns: 
//     -1 if file not found, otherwise returns the offset that we need to seek to 
//     in the filesystem file to get to the beginning of the fileName's directory 
//     entry 
off_t findFile(char *fileName) {
    int currBlock = 1; // Current block we are at in the directory
    int currLoc = 0; // Current location in the current block (in bytes)

    int fd = open(fsName, O_RDWR);

    while (currBlock != 0xffff) {
        while (currLoc < blockSize) {
            // seek to beginning of current directory entry
            lseek(fd, fatSize + (currBlock - 1) * blockSize + currLoc, SEEK_SET);
            char name[32];
            // Read the name of the file of the current entry
            read(fd, name, 32);
            if (strcmp(name, fileName) == 0) {
                close(fd);
                return fatSize + (currBlock - 1) * blockSize + currLoc;
            }
            currLoc += DIR_ENTRY_SIZE;
        }
        lseek(fd, currBlock * 2, SEEK_SET);
        uint16_t nextBlock;
        read(fd, &nextBlock, sizeof(uint16_t));
        currBlock = nextBlock;
        currLoc = 0;
    }

    close(fd);

    return -1;
}

// Function to touch files. If the file already exists, update its mtime in its 
// directory entry. Otherwise, create the file. 
// Arguments: 
//     fileNames: Array of strings that are the names of the files to be touched
//     numFiles: Length of fileNames
// Returns: 
//     -1 on failure (eg: there is not enough space to create all the files), and 0 
//     otherwise
int touch(char **fileNames, int numFiles) {

    int fd = open(fsName, O_RDWR);

    for (int i = 0; i < numFiles; i++) {
        off_t offset = findFile(fileNames[i]);
        if (offset == -1) { // If file does not exist, create it. By default,
                            // make it a regular file with YYN permissions
            if (createNewFile(fileNames[i], REG_FILE, YYN) == -1) {
                close(fd);
                return -1;
            }
        } else { // If file does exist, update its mtime
            // Seek to the location of mtime in the file's directory entry
            lseek(fd, offset + MTIME_OFFSET, SEEK_SET);
            time_t currTime = time(NULL);
            // Update the directory entry's time
            write(fd, &currTime, sizeof(time_t));
        }
    }

    close(fd);

    return 0;
}

// Function to delete the specified file
// Arguments: 
//     fileName: Name of file to delete
// Returns: 
//     0 on success, -1 otherwise (eg: file does not exist)
int deleteFile(char *fileName) {
    off_t offset = findFile(fileName);
    if (offset == -1) return -1; // File does not exist

    // Get the first block of the file
    int fd = open(fsName, O_RDWR);
    lseek(fd, offset + FIRST_BLOCK_OFFSET, SEEK_SET);
    uint16_t firstBlock;
    read(fd, &firstBlock, sizeof(uint16_t));

    // Delete file's directory entry by setting name[0] in its directory entry to 1
    lseek(fd, offset, SEEK_SET);
    char buffer = 1;
    write(fd, &buffer, sizeof(char));

    // Free all of the file's blocks in the data region by setting them to free in
    // the bitmap and setting the blocks to 0 in the FAT
    int currBlock = firstBlock;
    while (currBlock != 0xffff) {
        bitmap[currBlock - 1] = FREE; // currBlock is index currBlock - 1 in bitmap
        // Get next block in the file
        lseek(fd, currBlock * 2, SEEK_SET);
        uint16_t nextBlock;
        read(fd, &nextBlock, sizeof(uint16_t));
        // Set the current block to 0 in the FAT
        lseek(fd, currBlock * 2, SEEK_SET);
        uint16_t zero = 0;
        write(fd, &zero, sizeof(uint16_t));
        // Go to next block in the file
        currBlock = nextBlock;
    } 

    close(fd);

    return 0;
}

// Function to read a file on the FAT filesystem. 
// Arguments: 
//     fileName: The name of the file to read from
//     retFileSize: A pointer to a uint32_t variable that the function will set to 
//                  the size of the file, if it exists
// Returns: 
//     Null pointer if fileName does not exist, otherwise a string that is the 
//     contents of the file 
char *readFile(char *fileName, uint32_t *retFileSize) {
    off_t offset = findFile(fileName);
    if (offset == -1) return NULL; // If file does not exist return null pointer

    int fd = open(fsName, O_RDWR);

    // If file exists, find its size 
    uint32_t fileSize;
    lseek(fd, offset + SIZE_OFFSET, SEEK_SET);
    read(fd, &fileSize, sizeof(uint32_t));
    // Allocate char array to store contents of the file
    char *buffer = malloc(fileSize * sizeof(char));
    // Set retFileSize
    *retFileSize = fileSize;
    // Find first block of file
    uint16_t currBlock;
    lseek(fd, offset + FIRST_BLOCK_OFFSET, SEEK_SET);
    read(fd, &currBlock, sizeof(uint16_t));

    // Read the contents of the file into buffer
    uint32_t ind = 0; // Variable to keep track of where in the buffer we are 
                      // currently writing to
    while (currBlock != 0xffff) {
        // Seek to the beginning of the current block
        lseek(fd, fatSize + (currBlock - 1) * blockSize, SEEK_SET);
        if (fileSize >= blockSize) { // If we have a block or more to write to the
                                     // buffer, write the entire block at once 
            // Read the current block into the buffer at the appropriate location
            read(fd, buffer + ind, blockSize);
            ind += blockSize;
            fileSize -= blockSize; // fileSize keeps track of the number of bytes 
                                   // left to read
        } else {
            read(fd, buffer + ind, fileSize);
            break;
        }
        uint16_t nextBlock;
        lseek(fd, currBlock * 2, SEEK_SET);
        read(fd, &nextBlock, sizeof(uint16_t));
        currBlock = nextBlock;
    }

    close(fd);

    return buffer;
}

// Function to get the size of a file. Assumes the specified file is a valid 
// entry in the FAT filesystem
// TODO: ADD ERROR CHECKING IF GIVEN FILE IS INVALID
// Arguments: 
//     fileName: The name of the file to get the size of 
// Returns: 
//     The size of the file 
uint32_t getFileSize(char *fileName) {
    off_t offset = findFile(fileName);

    int fd = open(fsName, O_RDWR);

    // If file exists, find its size 
    uint32_t fileSize;
    lseek(fd, offset + SIZE_OFFSET, SEEK_SET);
    read(fd, &fileSize, sizeof(uint32_t));

    return fileSize;
}


// Function to write the contents of a string to a file. Writes contents of the 
// input buffer to the specified file in the FAT filesystem. There are two modes. 
//  In mode 0, the target file is overwritten. In mode 1, the target file is 
// appended to. If the target file does not exist, it is created
// Arguments: 
//     fileName: Name of the file to write to
//     buffer: The string to write to the file
//     size: Size of buffer (in bytes)
//     mode: The mode in which to write to the file
// Returns: 
//     0 if successful, -1 otherwise
int writeFile(char *fileName, char *buffer, uint32_t size, int mode) {

    off_t offset = findFile(fileName);
    if (offset == -1) { // If fileName does not exist, we can treat it the same 
                        // way we treat any mode 0 query to writeFile
        mode = 0; 
    }

    int fd = open(fsName, O_RDWR);
    uint16_t firstBlock;
    int currBlock; // Variable to keep track of the current block we are in
    int currLoc; // Variable to keep track of our location within the current block

    // If mode is 0, just delete the file and create a new file and then write 
    // to the new file
    if (mode == 0) {
        deleteFile(fileName);
        if (createNewFile(fileName, REG_FILE, YYN) == -1) {
            close(fd);
            return -1;
        }
        // Set the size of the newly created file
        offset = findFile(fileName);
        lseek(fd, offset + SIZE_OFFSET, SEEK_SET);
        write(fd, &size, sizeof(uint32_t));
        // Get the first block of the file and seek to it to begin writing
        lseek(fd, offset + FIRST_BLOCK_OFFSET, SEEK_SET);
        read(fd, &firstBlock, sizeof(uint16_t));
        currBlock = firstBlock;
        lseek(fd, fatSize + (currBlock - 1) * blockSize, SEEK_SET);
        currLoc = 0;
    } else { // This is the case in which we append to an existing file
        // Find the current size of the file and set the new size of the file
        uint32_t currSize; // Variable to get current size of fileName
        lseek(fd, offset + SIZE_OFFSET, SEEK_SET);
        read(fd, &currSize, sizeof(uint32_t));
        uint32_t newSize = size + currSize; // CHECK TO SEE SIZE+CURRSIZE DOESN'T 
                                            // EXCEED MAX FILE SIZE
        lseek(fd, offset + SIZE_OFFSET, SEEK_SET);
        write(fd, &newSize, sizeof(uint32_t));
        // Get the first block in the file
        lseek(fd, offset + FIRST_BLOCK_OFFSET, SEEK_SET);
        read(fd, &firstBlock, sizeof(uint16_t));
        currBlock = firstBlock;

        // Update file mtime
        lseek(fd, offset + MTIME_OFFSET, SEEK_SET);
        time_t newTime = time(NULL);
        write(fd, &newTime, sizeof(time_t));

        // Seek to the end of fileName to begin writing 
        while (currSize > 0) {
            if (currSize == blockSize) {
                currSize -= blockSize;
                // The file ends right at the end of its last block, so we need
                // to add a new block to the file to start writing the contents 
                // of buffer to fileName 
                currBlock = addBlock(currBlock);
                if (currBlock == -1) {
                    close(fd);
                    return -1;
                }
                currLoc = 0;
            } else if (currSize > blockSize) {
                currSize -= blockSize;
                uint16_t nextBlock;
                lseek(fd, currBlock * 2, SEEK_SET);
                read(fd, &nextBlock, sizeof(uint16_t));
                currBlock = nextBlock;
            } else {
                currLoc = currSize;
                currSize = 0;
            }
        }
        lseek(fd, fatSize + (currBlock - 1) * blockSize + currLoc, SEEK_SET);
    }

    // Write the contents of buffer to fileName. size keeps track of the amount
    // left to write
    int ind = 0; // Variable to keep track of where in buffer we are currently
                 // writing from
    // If we are currently in the middle of a block, write to the end of the block
    // before starting the while loop
    if (currLoc > 0) {
        if (size >= blockSize - currLoc) {
            write(fd, buffer + ind, blockSize - currLoc);
            ind += blockSize - currLoc;
            size -= (blockSize - currLoc);
            // Don't add a new block if size is now 0
            if (size != 0) {
                currBlock = addBlock(currBlock);
                if (currBlock == -1) {
                    close(fd);
                    return -1;
                }
                // Seek to the beginning of the new block 
                lseek(fd, fatSize + (currBlock - 1) * blockSize, SEEK_SET);
            }
        } else {
            write(fd, buffer + ind, size);
            size = 0;
        }
    }
    while (size > 0) {
        if (size >= blockSize) {
            write(fd, buffer + ind, blockSize);
            ind += blockSize;
            size -= blockSize;
            // Don't add a new block if size is now 0
            if (size != 0) {
                currBlock = addBlock(currBlock);
                if (currBlock == -1) {
                    close(fd);
                    return -1;
                }
                // Seek to the beginning of the new block 
                lseek(fd, fatSize + (currBlock - 1) * blockSize, SEEK_SET);
            }
        } else {
            write(fd, buffer + ind, size);
            size = 0;
        }
    }

    close(fd);
    return 0;
}


// Function to implement cp. Copies src to dest. There are three modes. If mode is 
// 0, both src and dest are in the FAT filesystem. If mode is 1, src is from the 
// host OS. If mode is 2, dest is from the host OS. dest gets created if it does
// not exist. If dest exists, it gets overwritten
// Arguments: 
//     src: Source filename
//     dest: Destination filename
//     mode: The mode in which to carry out the function
// Returns: 
//     0 on success, -1 on failure (eg: src does not exist)
int cp(char *src, char *dest, int mode) {
    // Variable for the string that will hold the contents from src that we will 
    // write to dest
    char *buffer;
    uint32_t fileSize;
    if (mode == 0 || mode == 2) { // src is in the FAT filesystem
        buffer = readFile(src, &fileSize);
        // If file does not exist, return -1
        if (buffer == NULL) return -1;
    } else { // src is from host OS
        int fd = open(src, O_RDWR);
        // If file does not exist, return -1
        if (fd == -1) return -1;
        // Get the size of src
        fileSize = lseek(fd, 0, SEEK_END);
        buffer = malloc(fileSize * sizeof(char));
        lseek(fd, 0, SEEK_SET); // Seek back to beginning of src
        read(fd, buffer, fileSize);

        close(fd);
    }

    if (mode == 0 || mode == 1) { // dest is in FAT filesystem
        writeFile(dest, buffer, fileSize, 0);
    } else { // dest is in host OS
        int fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        write(fd, buffer, fileSize);

        close(fd);
    }

    // If buffer was malloc'd, free buffer.
    if (mode == 1) free(buffer);

    return 0;
}

// Function to implement cat. There are three modes. If mode is 0, the specified 
// files are concatenated together and printed to stdout. If mode is 1, the 
// files are concatenated together and written to a specified file, and the file is 
// overwritten. If mode is 2, it is the same as mode 1, but the specified file is 
// appended to. If outFile does not exist, it is created
// Arguments: 
//     fileNames: Array of the names of the files to be concatenated
//     numFiles: Length of fileNames
//     mode: The mode in which to carry out the function
//     outFile: The file to write the output of the function to (unused if mode is 0)
// Returns: 
//     0 on success, -1 on failure
int cat(char **fileNames, int numFiles, int mode, char *outFile) {
    // Variable to keep track of whether outFile has been written to yet
    int firstWriteDone = 0;
    for (int i = 0; i < numFiles; i++) {
        uint32_t fileSize;
        char *buffer = readFile(fileNames[i], &fileSize);
        if (buffer == NULL) { // The file fileNames[i] does not exist
            char *str = ": No such file or directory\n";
            if (mode == 0) {
                f_write(STDOUT_FILENO, fileNames[i], strlen(fileNames[i]));
                f_write(STDOUT_FILENO, str, strlen(str));
            } else if (mode == 1) { // If mode is 1 and we are doing the first write
                                    // to outFile, we need to overwrite it. Otherwise,
                                    // we need to append to it
                if (firstWriteDone) 
                    writeFile(outFile, fileNames[i], strlen(fileNames[i]), 1);
                else
                    writeFile(outFile, fileNames[i], strlen(fileNames[i]), 0);
                
                firstWriteDone = 1;
                writeFile(outFile, str, strlen(str), 1);
            } else { // If mode is 2 we simply need to append to the file
                writeFile(outFile, fileNames[i], strlen(fileNames[i]), 1);
                writeFile(outFile, str, strlen(str), 1);
            }
            continue;
        }
        // If the fileNames[i] does exist, we need to write it to the appropriate
        // location
        if (mode == 0) {
            f_write(STDOUT_FILENO, buffer, fileSize);
            f_write(STDOUT_FILENO, "\n", 1);
        } else if (mode == 1) { // If mode is 1 and we are doing the first write
                                // to outFile, we need to overwrite it. Otherwise,
                                // we need to append to it
            if (firstWriteDone) 
                writeFile(outFile, buffer, fileSize, 1);
            else
                writeFile(outFile, buffer, fileSize, 0);
            
            firstWriteDone = 1;
            writeFile(outFile, "\n", 1, 1);
        } else { // If mode is 2 we simply need to append to the file
            writeFile(outFile, buffer, fileSize, 1);
            writeFile(outFile, "\n", 1, 1);
        }
    }

    return 1;
}


// Function to implement mv. Renames src to dest and deletes src. If dest does
// not exist, it is created. If it does exists, its contents are overwritten with
// those of src
// Arguments: 
//     src: Source file filename
//     dest: Destination file filename
// Returns: 
//     0 on success, -1 otherwise (eg: src does not exist) 
int mv(char *src, char *dest) {
    off_t offset = findFile(src);
    // If file does not exist, return -1
    if (offset == -1) return -1;
    // If src and dest are the same, do nothing
    if (strcmp(src, dest) == 0) return 0;

    // Copy the contents of src to dest
    cp(src, dest, 0);
    // Delete src
    deleteFile(src);

    return 0;
}

// Function to implement ls. Prints the information for each file in the filesystem
// on a new line. The columns printed for each file are first block number, 
// permissions, size, month, day, time, name.  
// Arguments: 
//     None
// Returns: 
//     None
void ls(void) {
    int fd = open(fsName, O_RDWR);
    int currBlock = 1; // The current block in the root directory that we are in
    int currLoc = 0; // The current location we are at in the current block
    int endReached = 0; // Variable to track whether we have reached the end of 
                        // the directory

    // Iterate through all root directory entries to print them
    while (currBlock != 0xffff) {
        while (currLoc < blockSize) {
            // Offset to beginning of current directory entry
            off_t offset = fatSize + (currBlock - 1) * blockSize + currLoc;

            // Check whether we have reached the end of the directory or if we are 
            // at a deleted entry
            lseek(fd, offset, SEEK_SET);
            char c;
            read(fd, &c, 1);
            if (c == 0) { // We have reached the end of the directory 
                endReached = 1;
                break;
            } else if (c == 1 || c == 2) { // We are at a deleted entry
                currLoc += DIR_ENTRY_SIZE;
                continue;
            }

            // Print first block number
            lseek(fd, offset + FIRST_BLOCK_OFFSET, SEEK_SET);
            uint16_t firstBlock;
            read(fd, &firstBlock, sizeof(uint16_t));
            printf("%d ", firstBlock);
            // Print the permissions
            lseek(fd, offset + PERMS_OFFSET, SEEK_SET);
            uint8_t perms;
            read(fd, &perms, sizeof(uint8_t));
            ((perms & 4) == 0) ? printf("-") : printf("r");
            ((perms & 2) == 0) ? printf("-") : printf("w");
            ((perms & 1) == 0) ? printf("-") : printf("x");
            printf(" ");
            // Print size
            lseek(fd, offset + SIZE_OFFSET, SEEK_SET);
            uint32_t size;
            read(fd, &size, sizeof(uint32_t));
            printf("%u ", size);
            // Print the date and time
            lseek(fd, offset + MTIME_OFFSET, SEEK_SET);
            time_t mtime;
            read(fd, &mtime, sizeof(time_t));
            struct tm tm = *localtime(&mtime);
            printf("%s %02d %02d:%02d:%02d ", months[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            // Print file name
            lseek(fd, offset, SEEK_SET);
            char name[NAME_SIZE];
            read(fd, name, NAME_SIZE);
            printf("%s\n", name);

            currLoc += DIR_ENTRY_SIZE;
        }
        if (endReached) break;
        // Get the next block in the root directory from the FAT
        lseek(fd, currBlock * 2, SEEK_SET);
        uint16_t nextBlock;
        read(fd, &nextBlock, sizeof(uint16_t));
        currBlock = nextBlock;
        currLoc = 0;
    }

    close(fd);
}


// Function to implement chmod. Changes permissions of the specified file.
// Arguments: 
//     fileName: The name of the file whose permissions to change
//     perms: The new permissions 
// Returns:
//     0 on success, -1 otherwise (eg: fileName does not exist)
int chmod(char *fileName, uint8_t perms) {
    off_t offset = findFile(fileName);
    // If fileName does not exist, return -1
    if (offset == -1) return -1;

    int fd = open(fsName, O_RDWR);

    // Seek to the appropriate location in the directory entry to rewrite 
    // permissions
    lseek(fd, offset + PERMS_OFFSET, SEEK_SET);
    write(fd, &perms, sizeof(uint8_t));
    close(fd);

    return 0;
}



// DOCUMENT ALL FUNCTIONS (ADD DESCRIPTION OF EVERYTHING THE FUNCTIONS DO TO THEIR
// DESCRIPTIONS)