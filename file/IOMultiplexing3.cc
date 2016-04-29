#include <sys/select.h>
#include <fcntl.h>
#include <list>
#include <string.h>
#include <pthread.h>
#include "jojo.h"

#define DSIZE BUFSIZ

using namespace std;

enum class skstatus {
	reading = 1, pending = 2, finished = 3
};

enum class skevents {
	initEnt = 1, readEnt = 2, writeEnt= 3
};

//selection key, combining ifd and ofds
struct sk {
	uint32_t ifd;
	uint32_t ofd;
	skevents event;
	skstatus status; //1=reading, 2=pending,3=finished
	uint8_t buf[DSIZE]; //for buffer data
	uint32_t ntowrite; //how much data need to write at a time
	bool lastone;
	pthread_mutex_t rd_lock;
	pthread_mutex_t wr_lock;
	//total: readfds, writefds
	fd_set *pbkReadfds;
	fd_set *pbkWritefds;
};

/**
 * thread routine, read operation and write operation are here
 */
void * thrRoutine(void*arg){
	struct sk * psk = (struct sk *)arg;

	//read event
	pthread_mutex_lock(&psk->rd_lock);
	if(skevents::readEnt == psk->event && skstatus::reading == psk->status){
		if((psk->ntowrite = read(psk->ifd, psk->buf, sizeof(psk->buf))) < 0)
			JojoUtil::err_sys("read error in thread!");
//		fprintf(stderr, "read %d bytes from file: %d\n", psk->ntowrite, psk->ifd);

		if(psk->ntowrite < sizeof(psk->buf)){
			psk->lastone = true;
			//XXX close file, can't put it here! //select function will select it again!
//			close(psk->ifd);
		}
		FD_SET(psk->ofd, psk->pbkWritefds);

		psk->status = skstatus::pending;

//		fprintf(stderr, "end of thread! event=read event\n");
	}
	pthread_mutex_unlock(&psk->rd_lock);

	//write event
	pthread_mutex_lock(&psk->wr_lock);
	if(skevents::writeEnt == psk->event && skstatus::pending == psk->status){
		int n = 0;
		if((n = write(psk->ofd, psk->buf, psk->ntowrite)) < 0)
			JojoUtil::err_sys("write error in thread!");
//		fprintf(stderr, "write %d bytes to file: %d\n", n, psk->ofd);

		//default set status to read
		psk->status = skstatus::reading;
		if(true == psk->lastone){
			psk->status = skstatus::finished;
			//XXX close file
			close(psk->ifd);
			close(psk->ofd);
		}

		//go to read more data!!
		if(skstatus::reading == psk->status)
			FD_SET(psk->ifd, psk->pbkReadfds);

//		fprintf(stderr, "end of thread! event=write event\n");
	}
	pthread_mutex_unlock(&psk->wr_lock);

	return (void *)0;
}

/**
 * dispatch read and write event
 */
void dispatcher(fd_set &cpReadfds, fd_set &cpWritefds, std::list<struct sk *> &sklist){
	pthread_t ntid;
	//1. tell read
	for(auto &psk : sklist){
		//XXX we can create another two threads handling these events
//		pthread_mutex_lock(&psk->rd_lock);
//		if(FD_ISSET(psk->ifd, &cpReadfds) && skstatus::reading == psk->status){
		if(FD_ISSET(psk->ifd, &cpReadfds)){
//			FD_CLR(psk->ifd, &cpReadfds); //this won't control concurrent, 'cause we cpReadfds is different copy after each select return
			FD_CLR(psk->ifd, psk->pbkReadfds);
			psk->event = skevents::readEnt;
			if(pthread_create(&ntid, NULL, thrRoutine, (void*)psk) != 0)
				JojoUtil::err_sys("can't create thread!");

//			fprintf(stderr, "create rd event thread : %lu\n", (uint64_t)ntid);
		}
//		pthread_mutex_unlock(&psk->rd_lock);

		//need tell if there data need to write here
//		pthread_mutex_lock(&psk->wr_lock);
		if(FD_ISSET(psk->ofd, &cpWritefds)){
//			FD_CLR(psk->ofd, &cpWritefds); //same as above
			FD_CLR(psk->ofd, psk->pbkWritefds);
//		if(FD_ISSET(psk->ofd, &cpWritefds) && skstatus::pending == psk->status){
			psk->event = skevents::writeEnt;
			if(pthread_create(&ntid, NULL, thrRoutine, (void*)psk) != 0)
				JojoUtil::err_sys("can't create thread!");

//			fprintf(stderr, "create wr event thread : %lu\n", (uint64_t)ntid);
		}
//		pthread_mutex_unlock(&psk->wr_lock);

		//need register, don't forget //can't do it here!! need do it after I/O operation
//		FD_SET(psk->ifd, &readfds);
//		FD_SET(psk->ofd, &writefds);
	}
}

struct sk * newsk(){
	return (struct sk*)malloc(sizeof(struct sk));
}

void initsk(struct sk *psk, int &ifd, int &ofd, fd_set &readfds, fd_set &writefds){
	if(pthread_mutex_init(&psk->rd_lock, NULL) != 0){
		free(psk);
		JojoUtil::err_sys("init mutex lock error!");
	}
	if(pthread_mutex_init(&psk->wr_lock, NULL) != 0){
		free(psk);
		JojoUtil::err_sys("init mutex lock error!");
	}

	psk->ifd = ifd;
	psk->ofd = ofd;
	psk->status = skstatus::reading;
	psk->event = skevents::initEnt;
	psk->ntowrite = 0;
	psk->lastone = false;
	psk->pbkReadfds = &readfds;
	psk->pbkWritefds = &writefds;
}

int main(int argc, char** argv){
	fd_set readfds, writefds;
	bool isEnd = false;
	char fname[20];
	int ifd, ofd, maxfd;
	list<struct sk*> sklist;
	fd_set backup_read;
	fd_set backup_write;

	if(argc < 2)
		JojoUtil::err_exit(1, "usage: a.out fromfile1 fromfile2 [fromfileN]");

	//init part
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	for(int i = 1; i < argc; i++){
		ifd = 0; ofd = 0;
		if((ifd = open(argv[i], O_RDONLY)) < 0)
			JojoUtil::err_sys("open file error!");

//		fprintf(stderr, "open file: %s, fd=%d\n", argv[i], ifd);

		sprintf(fname, "tofile%d.txt", i);
		if((ofd = creat(fname, FILE_MODE)) < 0)
			JojoUtil::err_sys("create file error!");
		if(ifd == 0 || ofd == 0)
			JojoUtil::err_exit(1, "open/create file error!");

		//init struct sk
		struct sk * psk = newsk();
		initsk(psk, ifd, ofd, backup_read, backup_write);
		sklist.push_front(psk);
		//register
		FD_SET(ifd, &readfds);
//		FD_SET(ofd, &writefds);
		maxfd = ofd > ifd ? ofd : ifd;
	}
	//do the IO Multiplexing job

	timeval wait_time;
	int selectRet = 0;

	uint64_t begin_micro = JojoUtil::nowMicros();
	while(!isEnd){
		//NB: timeval will be reset after select return
		wait_time.tv_sec = 0;
		wait_time.tv_usec = 1000;
		if((selectRet = select(maxfd + 1, &readfds, &writefds, NULL, &wait_time)) < 0)
			JojoUtil::err_sys("select function error!");
		fd_set tmp_reads = readfds;
		fd_set tmp_write = writefds;
		if(0 == selectRet){
			readfds = backup_read;
			writefds = backup_write;
		}else{
			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
		}
		//dispatch IO request
		dispatcher(tmp_reads, tmp_write, sklist);
		//check if all done
		for(auto &x : sklist){
			if(x->status != skstatus::finished)
				break;
			isEnd = true;
		}
	}
	uint64_t end_micro = JojoUtil::nowMicros();
	fprintf(stderr, "\ntime total cost: %u, block-size:%u\n", (unsigned int)(end_micro - begin_micro), DSIZE);

	return 0;
}
