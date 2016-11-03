#include<stdio.h>
#include<string.h>
#include<stdint.h>
#include"util-fwhashlist.h"

int main(void){
    FwHashListTable *ht = FwHashListTableInit(2,2,FwHashListTableGenericHash,NULL,NULL);
    printf("--FwHashListTableInit--\n");

    char ip[] = "192.168.11.110#14.215.177.37#80";
    uint16_t iplen = strlen(ip);
    char net[] = "192.168.11.0/24#80";
    uint16_t netlen = strlen(net);
    FwHashListTableAddIp(ht,ip,iplen);
    FwHashListTableAddNet(ht,net,netlen);

    FwHashListTableBucketIp *ipptr = NULL;
    ipptr = FwHashListTableLookupIp(ht,ip,iplen);
    if(ipptr != NULL)
        printf("ipptr->data = %s\n",ipptr->data);

    FwHashListTableBucketNet *netptr = NULL;
    netptr = FwHashListTableLookupNet(ht,net,netlen);
    if(netptr != NULL)
        printf("netptr->data = %s\n",netptr->data);

    FwHashListTableFree(ht);
    printf("--FwHashListTableFree--\n");
    return 0;
}
