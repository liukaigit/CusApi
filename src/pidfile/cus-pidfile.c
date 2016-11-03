#include "cus-common.h"
#include "cus-filelock.h"

int CusPidFileCreateAndSetlock(const char *pidfile,int pidfd){
    int ret = 1;
    if(NULL == pidfile){
        printf("pidfile is NULL!\n");
        ret = 0;
        goto error;
    }
    
    pidfd = open(pidfile,O_CREAT|O_TRUNC|O_NOFOLLOW|O_WRONLY,0644);
    if(-1 == pidfd){
        printf("unable to set pidfile '%s':%s\n",pidfile,strerror(errno));
        ret = 0;
        goto error;
    }

    if(0 == CusFileSetlock(pidfd)){
		printf("Fail to set wrlock.\n");
		ret = 0;
		goto error;
	}
		
	
    char pidbuf[16] = {0};
    size_t len = snprintf(pidbuf,sizeof(pidbuf),"%d",getpid());
    if(len <= 0){ 
        printf("PID error!\n");
        ret = 0;
        goto error;
    }
    
    ssize_t re = write(pidfd,pidbuf,len);
    if(-1 == re){
        printf("Fail to write pidfile:%s\n",strerror(errno));
        ret = 0;
        goto error;
    }

    error:
    return ret;
}

void CusPidFileRemoveAndUnlock(const char *pidfile,int pidfd){
   CusFileUnlock(pidfd);
   close(pidfd);
   if(NULL != pidfile)
		unlink(pidfile);
}

/*
 *write a pidfile (used at the startup)
 *retval 1 if success
 *retval 0 on failure
 * */
int CusPidFileCreate(const char *pidfile){
    int ret = 1;
    if(NULL == pidfile){
        ret = 0;
        printf("pidfile is NULL!\n");
        goto error;
    }
        
    char pidbuf[16] = {0};
    size_t len = snprintf(pidbuf,sizeof(pidbuf),"%d",getpid());
    if(len <= 0){
        printf("PID error!\n");
        ret = 0;
        goto error;
    }

    int pidfd = open(pidfile,O_CREAT|O_TRUNC|O_NOFOLLOW|O_WRONLY,0644);
    if(-1 == pidfd){
        printf("unable to set pidfile '%s':%s\n",pidfile,strerror(errno));
        ret = 0;
        goto error;
    }

    ssize_t r = write(pidfd,pidbuf,len);
    if(-1 == r){
        printf("unable to write pidfile:%s\n",strerror(errno));
        ret = 0;
        goto error;
    }

    close(pidfd);

    error:
    return ret;
}

/*delete the pidfile from the file system
 * we ignore the result,the user may have removed the pidfile already.
 * */
void CusPidFileRemove(const char *pidfile){ 
    if(NULL != pidfile)
        unlink(pidfile);
}
