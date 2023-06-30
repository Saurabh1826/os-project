#ifndef HEADERS_H
#define HEADERS_H

#include <stdint.h>

typedef struct dirEntry {
    char name[32];
    uint32_t size;
    uint16_t firstBlock;
    uint8_t type;
    uint8_t perm;
    time_t mtime;
    char blank[16]; // 16 extra reserved bytes
} dirEntry;

// Definitions for bitmap entries
#define FREE 0
#define OCCUPIED 1
// Definition for size of a directory entry
#define DIR_ENTRY_SIZE sizeof(struct dirEntry)
// Definition for size of the name field in a directory entry
#define NAME_SIZE 32
// Definitions for offsets to fields in a directory entry (from beginning of the entry)
#define MTIME_OFFSET 40
#define FIRST_BLOCK_OFFSET 36
#define SIZE_OFFSET 32
#define PERMS_OFFSET 39
// Definitions for name[0] of directory entry
#define DIR_END 0
#define DEL_FILE_1 1 // File deleted 
#define DEL_FILE_2 2 // File deleted but still in use
// File type definitions
#define UNKOWN_FILE 0
#define REG_FILE 1
#define DIR_FILE 2
#define SYM_LINK_FILE 4
// File permission definitions. They are in the format XXX, which are read, write, 
// execute, respectively. For example, YYN means read and write but no execute 
// permission
#define NNN 0
#define NYN 2
#define YNN 4
#define YNY 5
#define YYN 6
#define YYY 7

#endif