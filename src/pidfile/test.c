#include "cus-common.h"
#include "cus-pidfile.h"
#include "cus-filelock.h"

int main(int argc,char *argv[]){
	if(argc < 2){
        printf("Usage:%s pidfile_path.\n",argv[0]);
        return -1;
    }    
	int pidfd = 0;

    if(0 == CusPidFileCreateAndSetlock(argv[1],pidfd)){
        printf("Fail to create pidfile.\n");
        return -1;
    }
    printf("create pidfile\n");

    sleep(20);

    CusPidFileRemoveAndUnlock(argv[1],pidfd);
    printf("remove pidfile.\n");
	
    return 0;    
}
