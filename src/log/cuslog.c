#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<time.h>
#include"log.h"

int CusInitLog(const char* logpath){
	int logfd = open(logpath,O_CREAT|O_WRONLY|O_APPEND,0644);
	return logfd;
}

int CusWriteLog(int logfd,const void *buf,size_t len){
	int ret = write(logfd,buf,len);
	return ret;
}

void CusFiniLog(int logfd){
	close(logfd);
}

void CusGetTimeStamp(char *rdate,char *rtime,char *rtimestamp){
	time_t t;
	struct tm *stamp;

	time(&t);
	stamp = localtime(&t);
	if(NULL != rdate)
		sprintf(rdate,"%d/%d/%d",stamp->tm_mday,stamp->tm_mon + 1,stamp->tm_year + 1900);
	if(NULL != rtime)
		sprintf(rtime,"%d:%d:%d",stamp->tm_hour,stamp->tm_min,stamp->tm_sec);
	if(NULL != rtimestamp)
		sprintf(rtimestamp,"%d/%d/%d -- %d:%d:%d",stamp->tm_mday,stamp->tm_mon + 1,stamp->tm_year + 1900,stamp->tm_hour,stamp->tm_min,stamp->tm_sec);
}
