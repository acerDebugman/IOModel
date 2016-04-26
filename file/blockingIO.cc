#include <fcntl.h>
#include "jojo.h"

using namespace std;

int main(int argc, char** argv){
	int n; //for file size
	int ifd, ofd; //file descriptor
	char buf[BUFSIZ];

	if(argc != 3)
		JojoUtil::err_exit(1, "usage: testBio from.txt to.txt");

	if((ifd = open(argv[1], O_RDONLY)) < 0)
		JojoUtil::err_sys("open input file error! ");
//	if((ofd = open(argv[2], O_WRONLY | O_APPEND)) < 0)
//	if((ofd = open(argv[2], O_WRONLY)) < 0)
	if((ofd = open(argv[2], O_RDWR, FILE_MODE)) < 0)
		JojoUtil::err_sys("open output file error !");

	uint64_t start_micro = JojoUtil::nowMicros();

	while((n = read(ifd, buf, BUFSIZ)) > 0)
		if(write(ofd, buf, n) < 0)
			JojoUtil::err_sys("write error!");

	uint64_t end_micro = JojoUtil::nowMicros();
	unsigned int us = end_micro - start_micro;
	fprintf(stdout, "time cost : %9u \n", us);

	if(n < 0)
		JojoUtil::err_sys("read error!");

	exit(0);
}
