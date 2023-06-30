#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>

int findFreeBlock(void);
int addBlock(int lastBlock);
int createNewFile(char *fileName, uint8_t type, uint8_t perm);
off_t findFile(char *fileName);
int touch(char **fileNames, int numFiles);
int deleteFile(char *fileName);
char *readFile(char *fileName, uint32_t *retFileSize);
uint32_t getFileSize(char *fileName);
int writeFile(char *fileName, char *buffer, uint32_t size, int mode);
int cp(char *src, char *dest, int mode);
int cat(char **fileNames, int numFiles, int mode, char *outFile);
int mv(char *src, char *dest);
void ls(void);
int chmod(char *fileName, uint8_t perms);

#endif