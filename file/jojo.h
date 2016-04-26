#ifndef _JOJO_H
#define _JOJO_H

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <iostream>

#define MAXLINE 4096

/*
 * Default file access permissions for new files.
 */
#define	FILE_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

//define types, only used in windows
//typedef unsigned long long uint64_t;

namespace JojoUtil {
void err_exit(int error, const char *fmt, ...);
void err_sys(const char *fmt, ...);

//for time
uint64_t nowMicros();

//for set and clean file status flags
void set_fl(int fd, int flags);
void clr_fl(int fd, int flags);
}

#endif
