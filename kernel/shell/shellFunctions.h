#ifndef SHELL_FUNCTIONS_H
#define SHELL_FUNCTIONS_H

#include <stdarg.h>

void shellCat(char *args[]);
void shellSleep(char *args[]);
void shellBusy(char *args[]);
void shellEcho(char *args[]);
void shellLs(char *args[]);
void shellTouch(char *args[]);
void shellMv(char *args[]);
void shellCp(char *args[]);
void shellRm(char *args[]);
void shellChmod(char *args[]);
void shellPs(char *args[]);
void shellKill(char *args[]);
void shellZombify(char *args[]);
void zombieChild(char *args[]);
void shellOrphanify(char *args[]);
void orphanChild(char *args[]);

#endif