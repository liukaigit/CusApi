#ifndef __KMP_H__
#define __KMP_H__

void KmpPreStrNext(const char *str,int next[]);
char *KmpMatch(const char*src,const char*dst,int next[]);

#endif
