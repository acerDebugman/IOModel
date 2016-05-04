#include <malloc.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <list>
#include <map>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include "jojo.h"

#define BSZ BUFSIZ
#define MAX_CONN 30

using namespace std;

enum class skstatus {
	unused = 0, write_pending = 1, read_pending = 2, finished = 3
};

enum class skevents {
	initEnt = 1, readEnt = 2, writeEnt= 3
};

//reactor patter
struct reactor{
	//resources

	//dispatcher

	//synchronous event demultiplexer

	//request handler
};

//selection key, combining ifd and ofds
struct sk {
	uint32_t ifd;
	uint32_t ofd;
//	skevents event;
	skstatus status; //1=reading, 2=pending,3=finished
	uint8_t buf[BSZ]; //for buffer data
	uint32_t nread; //how much data need to write at a time
	uint32_t nwrite;
	bool lastone;
	int offset;
	int nbytes;
	struct stat *pstinfo;
	struct pollfd *pipolfd;
	struct pollfd *popolfd;

	pthread_mutex_t rd_lock;
	pthread_mutex_t wr_lock;
};

/**
 * thread routine, read operation and write operation are here
 */
void * thrRoutine(void*arg){
	struct sk * psk = (struct sk *)arg;

	//read event
	pthread_mutex_lock(&psk->rd_lock);
	if(skstatus::read_pending == psk->status){
		if((psk->nread = read(psk->ifd, psk->buf, psk->nbytes)) < 0)
			JojoUtil::err_sys("read error in thread!"); //actuallly if we meet errors at here, we should undo something

		if(psk->nread < psk->nbytes && !psk->lastone)
			JojoUtil::err_sys("short read error!");

//		fprintf(stderr, "read %d bytes from file: %d\n", psk->nread, psk->ifd);

		//change status and register to the set
		psk->nbytes = psk->nread;
		psk->status = skstatus::write_pending;
		psk->popolfd->events |= POLLOUT; //register write operations
	}
	pthread_mutex_unlock(&psk->rd_lock);

	//write event
	pthread_mutex_lock(&psk->wr_lock);
	if(skstatus::write_pending == psk->status){
		if((psk->nwrite = write(psk->ofd, psk->buf, psk->nbytes)) < 0)
			JojoUtil::err_sys("write error in thread!");

//		fprintf(stderr, "write %d bytes to file: %d, lastone=%d\n", psk->nwrite, psk->ofd, psk->lastone);

		if(psk->nwrite < psk->nbytes)
			JojoUtil::err_sys("short write error!");

		//default set status to read
		psk->status = skstatus::unused;
		if(true == psk->lastone)
			psk->status = skstatus::finished;

		//go to read more data!!
		if(skstatus::unused == psk->status)
			psk->pipolfd->events |= POLLIN;

//		fprintf(stderr, "psk->ifd:%d, ipfd-skstatus:%d, psk-ofd:%d, opfd-status:%d\n",
//				psk->pipolfd->fd, psk->pipolfd->events, psk->popolfd->fd, psk->popolfd->events);
//		fprintf(stderr, "end of thread! event=write event\n");
	}
	pthread_mutex_unlock(&psk->wr_lock);

	return (void *)0;
}

/**
 * dispatch read and write event
 */
void dispatcher(struct pollfd *ppfd, multimap<int, struct sk*> &fdsmap){
	pthread_t ntid;

	for(int i = 0; i < MAX_CONN; i++){
		struct pollfd * ppol = (ppfd + i);
		if(-1 == ppol->fd)
			continue ;

//		fprintf(stderr, "before check, i=%d, ppol->fd=%d, PIN=%d, POT=%d, rPOLLIN=%d, rPOLLOUT=%d\n", i,
//				ppol->fd, ppol->events , ppol->events , ppol->revents , ppol->revents );

		if(ppol->revents & POLLIN){
			ppol->events &= ~POLLIN; //clear the identity

//			fprintf(stderr, "poll here, ppol->fd=%d, PIN=%d\n", ppol->fd, ppol->events);

			struct sk *psk = fdsmap.find(ppol->fd)->second;
			if(skstatus::unused != psk->status)
				continue;

			psk->offset += BSZ;
			//if the offset over the boundary
			if(psk->offset >= psk->pstinfo->st_size + BSZ)
				continue ;

			psk->status = skstatus::read_pending;
			psk->nbytes = BSZ;
			if(psk->offset >= psk->pstinfo->st_size)
				psk->lastone = true;

			if(pthread_create(&ntid, NULL, thrRoutine, (void*)psk) < 0)
				JojoUtil::err_sys("create thread error!");
		}
		if(ppol->revents & POLLOUT){
			struct sk * psk = fdsmap.find(ppol->fd)->second;
//			fprintf(stderr, "in write ppol->events=%d, psk->popolfd->events=%d\n", ppol->events, psk->popolfd->events);
			ppol->events &= ~POLLOUT;
			psk->popolfd->events &= ~POLLOUT;
			if(skstatus::write_pending != psk->status)
				continue ;

			if(pthread_create(&ntid, NULL, thrRoutine, (void*)psk) < 0)
				JojoUtil::err_sys("create thread error!");
		}
	}
}

struct sk * newsk(){
	return (struct sk*)malloc(sizeof(struct sk));
}

void initsk(struct sk *psk, int &ifd, int &ofd, struct stat &stinfo, struct pollfd *pipfd, struct pollfd *popfd){
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
	psk->status = skstatus::unused;
	psk->nread = 0;
	psk->nwrite = 0;
	struct stat * pst = (struct stat*)malloc(sizeof(struct stat));
	pst = (struct stat*)malloc(sizeof(struct stat));
	memcpy(pst, &stinfo, sizeof(struct stat));
	psk->pstinfo = pst;
	psk->lastone = false;
	psk->offset = 0;
	psk->pipolfd = pipfd;
	psk->popolfd = popfd;
}

int main(int argc, char** argv){
	struct pollfd *ppfds;
	bool isEnd = false;
	char fname[20];
	int ifd, ofd;
	//key: ifd, val: struct sk* and key: ofd, val: struct sk*
	multimap<int, struct sk*> fdsmap;
//	nfds_t nfds = MAX_CONN;
	struct stat stinfo;

	if(argc < 2)
		JojoUtil::err_exit(1, "usage: a.out fromfile1 fromfile2 [fromfileN]");

	//initial all pollfd
	ppfds = (struct pollfd*)malloc(sizeof(struct pollfd) * MAX_CONN);
	memset(ppfds, 0, sizeof(struct pollfd) * MAX_CONN);
	for(int i = 0; i < MAX_CONN; i++){
		(ppfds + i)->fd = -1;
		(ppfds + i)->events = 0;
		(ppfds + i)->revents = 0;
	}

	//init part
	int j = 0;
	for(int i = 1; i < argc && j + 1 < MAX_CONN; i++){
		ifd = 0; ofd = 0;
		if((ifd = open(argv[i], O_RDONLY)) < 0)
			JojoUtil::err_sys("open file error!");

		if(fstat(ifd, &stinfo) < 0)
			JojoUtil::err_sys("get file stat info error!");

		sprintf(fname, "tofile%d.txt", i);
		if((ofd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, FILE_MODE)) < 0)
			JojoUtil::err_sys("create file error!");
		if(ifd == 0 || ofd == 0)
			JojoUtil::err_exit(1, "open/create file error!");

		struct pollfd * pipolfd = ppfds + j;
		struct pollfd * popolfd = ppfds + j + 1;
		pipolfd->fd = ifd;
		pipolfd->events |= POLLIN;
		popolfd->fd = ofd;

		struct sk *psk = (struct sk *)malloc(sizeof(struct sk));
		initsk(psk, ifd, ofd, stinfo, pipolfd, popolfd);
		fdsmap.insert(std::pair<int, struct sk*>(ifd, psk));
		fdsmap.insert(std::pair<int, struct sk*>(ofd, psk));

		j += 2;

//		fprintf(stderr, "ifd=%d, ofd=%d, piPIN=%d, ifd_size=%d\n", ifd, ofd, pipolfd->events & POLLIN, stinfo.st_size);
	}

	//test
//	for(int i = 0; i < MAX_CONN; i++){
//		fprintf(stderr, "pollfd=%d,nfds=%lu, events=%d, &val=%d, addr=%x\n",
//				(ppfds + i * sizeof(struct pollfd))->fd, nfds, (ppfds + i * sizeof(struct pollfd))->events,
//				(ppfds + i * sizeof(struct pollfd))->events & POLLOUT,
//				(ppfds + i * sizeof(struct pollfd)));
//	}


	//do the IO Multiplexing job
	int pollret = 0;
	uint64_t begin_micro = JojoUtil::nowMicros();
	while(!isEnd){
		if((pollret = poll(ppfds, MAX_CONN, 0)) < 0)
			JojoUtil::err_sys("select function error!");

//		for(int y = 0; y < 8; y++){
//			struct pollfd * ppfd = (ppfds + y);
//			fprintf(stderr, "after poll fd=%d, POLLIN=%d, POLLOUT=%d, addr=%0x\n",
//					ppfd->fd, ppfd->revents , ppfd->revents, ppfd);
//		}

		//dispatch IO request
		dispatcher(ppfds, fdsmap);
		//check if all done
		isEnd = true;
		for(auto &x : fdsmap){
			if(skstatus::finished != x.second->status){
				isEnd = false;
				break;
			}
		}

//		fprintf(stdout, "*****circle: %d\n", isEnd);
	}

	uint64_t end_micro = JojoUtil::nowMicros();
	fprintf(stderr, "\ntime total cost: %u, block-size:%u\n", (unsigned int)(end_micro - begin_micro), BSZ);

	//close all files here
	for(auto &x : fdsmap){
		close(x.first);
	}

	return 0;
}
