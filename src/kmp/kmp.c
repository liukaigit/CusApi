#include <stdio.h>
#include <string.h>

void KmpPreStrNext(const char *str,int next[]){
    int index = 0,k = 0;
    int len = strlen(str);
    next[0] = 0;    
    for(index = 1,k = 0;index < len;index++){
    
    while(k > 0 && str[index] != str[k]){
        k = next[k-1];
    }

    if(str[index] == str[k]){
        k++;
    }
    next[index] = k;
    }
}

char *KmpMatch(const char*src,const char*dst,int next[]){
    int nsrc = 0,ndst = 0;
    int index = 0,k = 0;

    nsrc = strlen(src);
    ndst = strlen(dst);

    for(index = 1,k = 0;index < nsrc;index++){
        while(k > 0 && src[index] != dst[k]){
            k = next[k-1];
        }
        if(src[index] == dst[k]){
            k++;
        }
        if(k == ndst){
            return (src + index - ndst + 1);
        }
    }
    return NULL;
}

