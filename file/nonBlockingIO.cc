#include <fcntl.h>
#include <errno.h>
#include "jojo.h"

using namespace std;

uint8_t buf[1000000];

int main(int argc, char**argv){
	int ntowrite, nwrite;
	uint8_t * ptr;

	ntowrite = read(STDIN_FILENO, buf, sizeof(buf));
	fprintf(stderr, "read %d bytes\n", ntowrite);

	JojoUtil::set_fl(STDOUT_FILENO, O_NONBLOCK);
	ptr = buf;
	while(ntowrite > 0){
		nwrite = write(STDOUT_FILENO, ptr, ntowrite);
		fprintf(stderr, "nwrite=%d, errno = %d, err_msg = %s\n", nwrite, errno, strerror(errno));

		if(nwrite > 0){
			ptr += nwrite;
			ntowrite -= nwrite;
		}
	}
	JojoUtil::clr_fl(STDOUT_FILENO, O_NONBLOCK);

	exit(0);
}
