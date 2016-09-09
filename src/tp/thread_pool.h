#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

typedef struct _thread_pool thread_pool_t;

thread_pool_t *thread_pool_create(int size);
void thread_pool_destroy(thread_pool_t *tp);
void thread_pool_add_work(thread_pool_t *tp, void *(*routine)(void *), void *arg);
void thread_pool_wait(thread_pool_t *tp);

#endif
