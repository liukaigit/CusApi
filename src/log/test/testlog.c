#include<stdio.h>
#include<log.h>
#include<pthread.h>
#include<string.h>

static int info_logfd = 0;

void *task(void *data){
    printf("Thread ----> %u   starting.\n",pthread_self());
    int cnt = 100;
    char buf[255] = {0};
    char timestamp[30] = {0};
    CusGetTimeStamp(NULL,NULL,timestamp);//这个函数可以获取三个不同的字段，
                                      //第一日期4/5/2015，第二时间14:34:23，第三日期加时间4/5/2015 -- 14:34:23，不需要就给NULL
    sprintf(buf,"%s %u is writing.\n",timestamp,pthread_self());
    while(cnt--){
       CusWriteLog(info_logfd,buf,strlen(buf));//刚才傻叉一样strlen后面加了个1，导致‘\0’被写入文件出现乱码
        usleep(1);//这个可以不要；添加就是为了好观察多线程同时写日志的效果；
    }
    printf("Thread ---> %u   stopping.\n",pthread_self());
    return NULL;    
}

int main(int argc,char *argv[]){
    //初始化获取日志文件句柄
    info_logfd = CusInitLog("./info.log"); //返回的是文件句柄
    if(-1 == info_logfd)
        return -1;

    //多线程同时操作日志句柄写日志内容，封装的接口中write为原子操作，
    //所以不需要加线程锁，不会出现一个句柄的内容被打断的现象；
    pthread_t td[5];
    int i = 0,ret = 0;
    for(;i < 5;i++) {
        ret = pthread_create(&td[i],NULL,task,NULL);   //成功返回0；
        if(ret)
            continue;
        printf("Thread %d is working.\n");
    }  

    for(i = 0;i < 5;i++){
        pthread_join(td[i],NULL);    //join在等待回收线程资源所以不用detach了；
    }

    //关闭日志
    CusFiniLog(info_logfd);
    return 0;    
}

