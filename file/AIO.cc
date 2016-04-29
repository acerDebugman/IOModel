#include <ctype.h>
#include <errno.h>
#include <aio.h>
#include <fcntl.h>

#include "jojo.h"

using namespace std;

void th_rountine(sigval_t val){
	struct aiocb * pcb = (struct aiocb*)val.sival_ptr;
	fprintf(stderr, "the rs is :\n %s\n", (char*)pcb->aio_buf);

	exit(0);
}

int main(int argc, char** argv){
	struct aiocb acb;
	int ifd, ofd;
	uint8_t buf[BUFSIZ];
	struct sigevent sigent;
	int err;
	int nread;

	if(argc != 2)
		JojoUtil::err_exit(2, "usage: a.out file");

	if((ifd = open(argv[1], O_RDONLY)) < 0)
		JojoUtil::err_sys("open file error!");

	sigent.sigev_notify = SIGEV_THREAD;
	sigent.sigev_value.sival_int = 1;
	sigent.sigev_value.sival_ptr = (void*)&acb;
	sigent._sigev_un._sigev_thread._function = th_rountine;

	acb.aio_fildes = ifd;
	acb.aio_buf = buf;
	acb.aio_offset = 0;
	acb.aio_nbytes = BUFSIZ;
	memcpy(&acb.aio_sigevent, &sigent, sizeof(sigent));

	//how can proof it's asynchronous ?
	if(aio_read(&acb) < 0)
		JojoUtil::err_sys("aio read error!");


	//aio_error is not synchronous function
	if((err = aio_error(&acb)) != EINPROGRESS) //if asynchronous is stil busy, then sleep one second
		sleep(1);
	//if I comment the while loop, then I can proof aio_read() is a asynchronous function
	while(err == EINPROGRESS){
		if((err = aio_error(&acb)) != EINPROGRESS) //if asynchronous is stil busy, then sleep one second
			sleep(1);
	}

	if(err != 0){
		if(err == -1)
			JojoUtil::err_sys("aio_error meet error!");
		else
			JojoUtil::err_exit(err, "read fail!");
	}

	if((nread = aio_return(&acb)) < 0)
		JojoUtil::err_sys("aio return failed!");

	fprintf(stderr, "read %d bytes from file\n", nread);
	fprintf(stderr, "is there anything out first ?\n %s\n", (char*)acb.aio_buf);

	//wait it has done, because aio_read return immediately
	sleep(1);

	return 0;
}
