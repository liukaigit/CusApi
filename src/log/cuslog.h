#ifndef __LOG_H_INCLUDE__
#define __LOG_H_INCLUDE__

#include<pthread.h>
#include<sys/types.h>

/*
 *man 2 write:The adjustment of the file offset and the write operation are performed as an atomic step.
 *So,no longer use the lock operation.
 */

/*Call the "open" system function,open the "logpath", return the logfile descriptor.*/
int CusInitLog(const char* logpath);

/*Write log.*/
int CusWriteLog(int logfd,const void *buf,size_t len);

/*close the logfile descriptor.*/
void CusFiniLog(int logfd);

/*Get the time stamp
 * rdate<real date>:12/8/2016
 * rtime<real time>:24:20:12
 * rtimestamp<real timestamp>:12/8/2016 -- 24:20:18
 * */
void CusGetTimeStamp(char *rdate,char *rtime,char *rtimestamp);

#endif








