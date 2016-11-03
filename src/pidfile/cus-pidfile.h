#ifndef __CUS_PIDFILE_H__
#define __CUS_PIDFILE_H__

int CusFileSetlock(int );
void CusFileUnlock(int );
//void CusFileGetlock(int );

int CusPidFileCreateAndSetlock(const char *,int );
void CusPidFileRemoveAndUnlock(const char *,int );

int CusPidFileCreate(const char *);
void CusPidFileRemove(const char *);

#endif  /*end of the __CUS_PIDFILE_H__*/