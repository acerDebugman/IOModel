#include <sys/select.h>
#include <fcntl.h>
#include "jojo.h"

#define DSIZ 1024

using namespace std;

int main(int argc, char** argv){
	int ifd, ofd;
	int ntowrite;
	int maxfds1;
	fd_set readfds, writefds;
	uint8_t buf[DSIZ];
	bool isEnd = false;
	bool lastOne = false;

	if(argc != 3)
		JojoUtil::err_exit(1, "usage: iom fromfile tofile");

	if((ifd = open(argv[1], O_RDONLY)) < 0)
		JojoUtil::err_sys("open file error! ");

	if(((ofd = open(argv[2], O_WRONLY)) < 0))
		JojoUtil::err_sys("open file error!");

	fprintf(stderr, "file name: %s, %s, %d, %d\n",argv[1], argv[2], ifd, ofd);

	FD_SET(ifd, &readfds);
	FD_SET(ofd, &writefds);
	maxfds1 = ofd + 1;
	//dispatcher
	int count = 0;
	while(count++ < 50 && !isEnd){
		if(select(maxfds1, &readfds, &writefds, NULL, NULL) < 0)
			JojoUtil::err_sys("select encounter error!");

		//handler
		if(FD_ISSET(ifd, &readfds)){
			if((ntowrite = read(ifd, buf, DSIZ)) < 0)
				JojoUtil::err_sys("read encounter error!");

			fprintf(stderr, "read count: %d\n", ntowrite);

			if(ntowrite < DSIZ){
				lastOne = true;
				fprintf(stderr, "********************read end: %d\n", ntowrite);
			}
		}

		//handler
		if(FD_ISSET(ofd, &writefds)){
			if(lastOne)
				isEnd = true;

			if(ntowrite > 0){
				if(write(ofd, buf, ntowrite) < 0)
					JojoUtil::err_sys("write encounter error! ");

				//reset to zero
				ntowrite = 0;
			}
		}
		//register
		FD_SET(ifd, &readfds);
		FD_SET(ofd, &writefds);
	}
	return 0;
}
