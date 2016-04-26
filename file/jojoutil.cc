#include <errno.h> /*for definition error*/
#include <stdarg.h>  /* ISO C variable arguments */
#include <fcntl.h>
#include "jojo.h"

namespace JojoUtil{

static void err_doit(int errnoflag, int error, const char *fmt, va_list ap);

void err_exit(int error, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	err_doit(1, error, fmt, ap);
	va_end(ap);
	exit(1);
}

void err_sys(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	err_doit(1, errno, fmt, ap);
	va_end(ap);
	exit(1);
}

static void err_doit(int errnoflag, int error, const char *fmt, va_list ap){
	char buf[MAXLINE];

	vsnprintf(buf, MAXLINE - 1, fmt, ap);
	if(errnoflag)
		snprintf(buf + strlen(buf), MAXLINE - 1 - strlen(buf), ": %s", strerror(error));
	strcat(buf, "\n");
	fflush(stdout); /* in case stdout and stderr are the same*/
	fputs(buf, stderr);
	fflush(NULL); /* flush all stdio output streams  */
}


//time functions
uint64_t nowMicros(){
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

void set_fl(int fd, int flags){
	int val;

	if((val = fcntl(fd, F_GETFL, 0)) < 0)
		err_sys("fcntl F_GETFL error");

	val |= flags; //turn on flags

	if(fcntl(fd, F_SETFL, val) < 0)
		err_sys("fcntl F_SETFL error");
}
void clr_fl(int fd, int flags){
	int val;

	if((val = fcntl(fd, F_GETFL, 0)) < 0)
		err_sys("fcntl F_GETFL error");

	val &= ~flags; //turn off the flags

	if(fcntl(fd, F_SETFL, val) < 0)
		err_sys("fcntl F_SETFL error");
}

}
