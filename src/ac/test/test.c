#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>
#include"ac.h"

static int SCACTest01(void)
{
    int result = 0;
    MpmCtx mpm_ctx;
    PatternMatcherQueue pmq;     /*用来存储匹配结果*/
	PmqSetup(&pmq,10);

    memset(&mpm_ctx, 0, sizeof(MpmCtx));
    SCACInitCtx(&mpm_ctx);      /*初始化状态机结构体*/
    
    /* 1 match */
    SCACAddPatternCS(&mpm_ctx, (uint8_t *)"abcd", 4, 0, 0, 0, 0, 0);/*添加匹配字符串*/
	SCACAddPatternCS(&mpm_ctx, (uint8_t *)"klmn", 4, 0, 0, 1, 0, 0);
	SCACAddPatternCS(&mpm_ctx, (uint8_t *)"pqrs", 4, 0, 0, 2, 0, 0);
	SCACAddPatternCS(&mpm_ctx, (uint8_t *)"mnopqrs", 7, 0, 0, 3, 0, 0);
    
    SCACPreparePatterns(&mpm_ctx);  /*准备状态机*/

    char *buf = "bcmnopqrsdefghjiklmnopqrstuvwxyiklmnopqrstuvwxyabcdefghjiklmnopqrstuvwxyz";
    printf("buf = %s\n",buf);

    uint32_t cnt = SCACSearch(&mpm_ctx,/* &mpm_thread_ctx, */&pmq,     /*匹配*/
                               (uint8_t *)buf, strlen(buf));

	printf("cnt = %d\n",cnt);
    if (cnt == 1)
        result = 1;
    else
        printf("1 != %d\n",cnt);

	printf("pmq->%d\n",pmq.pattern_id_array[0]);    /*拿结果，匹配上的字符串下标*/

    SCACDestroyCtx(&mpm_ctx);      
    PmqCleanup(&pmq);
    return result;
}

int main(){
	//MpmACRegister();
    SCACTest01();  
    return 0;    
}
