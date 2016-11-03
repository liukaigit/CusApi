#include "cus-common.h"

int CusFileSetlock(int fd){
	struct flock lk;
    memset(&lk,0,sizeof(lk));
    lk.l_type = F_WRLCK;
    lk.l_whence = SEEK_SET;
    lk.l_start = 0;
    lk.l_len = 0;
    lk.l_pid = getpid();

    int r = fcntl(fd,F_SETLK,&lk);
    if(r < 0){
        printf("pidfile,unable to set lock.\n");
		return 0;
    }
	return 1;
}

void CusFileUnlock(int fd){
	struct flock lk;
	memset(&lk,0,sizeof(lk));
	lk.l_type = F_UNLCK;
	lk.l_whence = SEEK_SET;
	lk.l_start = 0;
	lk.l_len = 0;
	lk.l_pid = getpid();

	int r = fcntl(fd,F_SETLK,&lk);
	if(-1 == r){
		printf("fail to unlock pidfile.\n");
	}	
	
}