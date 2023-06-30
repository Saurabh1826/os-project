#ifndef MKFS_H
#define MKFS_H

int mkfs(char *fs_Name, int blocksInFat, int blockSizeConfig, char *bitmapPointer);

#endif