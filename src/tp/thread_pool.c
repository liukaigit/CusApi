#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "thread_pool.h"

typedef struct _work {
	void 	     		*(*routine)(void*);
	void 			*arg;
	struct _work 		*next;
} work_t;

typedef struct _work_queue {
	int 			size;
	work_t 			*head;
	work_t 			*tail;
} work_queue_t;

typedef struct _thread {
	thread_pool_t 		*tp;
	int 			tid;		/* human id */
	pthread_t 		handle;
} thread_t;

struct _thread_pool {
	int 			thread_size;	/* the size of threads array */
	thread_t 		*threads;	/* the array of thread_t */
	work_queue_t 		*wq;
	pthread_mutex_t 	mutex;
	pthread_cond_t 		cond;
	int			shutdown;	/*0-false,1-true*/
};


static void *_dispatch_work(void *arg);

static void _destroy_threads(thread_pool_t *tp);

static work_queue_t *_work_queue_create();
static void _work_queue_destroy(work_queue_t *wq);
static void _work_queue_push(work_queue_t *wq, void *(*routine)(void *), void *arg);
static work_t *_work_queue_pop(work_queue_t *wq);
static int _work_queue_empty(work_queue_t *wq);

/*
 * create and init thread pool
 */
thread_pool_t *
thread_pool_create(int size) {
	thread_pool_t *tp = NULL;
	
	tp = (thread_pool_t *)malloc(sizeof(thread_pool_t));
	if (tp == NULL) {
		perror("malloc thread_pool_t");
		goto QUIT;
	}
	memset(tp, 0, sizeof(thread_pool_t));

	/* mutex */
	if (pthread_mutex_init(&tp->mutex, NULL) != 0) {
		perror("pthread_mutex_init wq_mutex");
		goto QUIT;
	}
	
	if (pthread_cond_init(&tp->cond, NULL) != 0) {
		perror("pthread_cond_init wq_cond");
		pthread_mutex_destroy(&tp->mutex);
		goto QUIT;
	}
	
	/* work queue */
	tp->wq = _work_queue_create();
	if (tp->wq == NULL) {
		goto ERROR;
	}

	/* threads */
	tp->threads = (thread_t *)malloc(sizeof(thread_t) * size);
	if (tp->threads == NULL) {
		perror("malloc thread_t");
		goto ERROR;
	}
	memset(tp->threads, 0, sizeof(thread_t) * size);
	
	int i;
	thread_t *th;
	pthread_t *phandle;
	for (i = 0; i < size; i++) {
		th = tp->threads + tp->thread_size; 
		phandle = &th->handle;
		if (pthread_create(phandle, NULL, _dispatch_work, (void *)th)) {
			perror("create thread");
			goto ERROR;
		}
		th->tid = tp->thread_size; 
		th->tp = tp;
		
		tp->thread_size++;
#ifdef _TP_DEBUG
		printf("thread #%d,%p has been created\n", th->tid, th);
#endif
	}
	
	if (tp->thread_size == 0) {
		fprintf(stderr, "Error: no any thread has been created\n");
		goto ERROR;
	}

	return tp;
	
ERROR:
	_destroy_threads(tp);
	_work_queue_destroy(tp->wq);
	pthread_mutex_destroy(&tp->mutex);
	pthread_cond_destroy(&tp->cond);
QUIT:
	fprintf(stderr, "thread_pool_create() failed\n");
	return NULL;
}

/*
 * destroy thread pool and free all memory
 */
void
thread_pool_destroy(thread_pool_t *tp) {
	if (tp == NULL) return;
	
	_destroy_threads(tp); 
	_work_queue_destroy(tp->wq);
	pthread_mutex_destroy(&tp->mutex);
	pthread_cond_destroy(&tp->cond);
	free(tp);
}

/*
 * wait util all work have been handled.
 * Warning: if all threads exit and some works still exist, 
 *         thread_pool_wait() will *not* exit 
 */
void
thread_pool_wait(thread_pool_t *tp) {
	/* wait all works done */
	while(1) {
		//if there have any work, then wait to handle
		pthread_mutex_lock(&tp->mutex);
		if (_work_queue_empty(tp->wq)) {
			pthread_mutex_unlock(&tp->mutex);
			break;
		}
		pthread_mutex_unlock(&tp->mutex);
		usleep(300000);//0.3s
	}
}

/*
 * add work to thread pool
 */
void
thread_pool_add_work(thread_pool_t *tp, void *(*routine)(void *), void *arg) {
	assert(tp != NULL);

	if (routine == NULL) {
		fprintf(stderr, "routine can't be *NULL*\n");
		return;
	}
	
	pthread_mutex_lock(&tp->mutex);
	_work_queue_push(tp->wq, routine, arg);
#ifdef _TP_DEBUG
	printf("add work(routine:%p, arg:%p)\n", routine, arg);
#endif
	pthread_cond_signal(&tp->cond);
	pthread_mutex_unlock(&tp->mutex);
}

static void
_destroy_threads(thread_pool_t *tp) {
	if (tp == NULL) return;

	/* wait for all work completed */
	if (tp->threads) {
		pthread_mutex_lock(&tp->mutex);
		tp->shutdown = 1;
		pthread_cond_broadcast(&tp->cond);
		pthread_mutex_unlock(&tp->mutex);

		int i;
		thread_t *th;
		for (i = 0; i < tp->thread_size; i++) {
			th = tp->threads + i;
			pthread_join(th->handle, NULL);
#ifdef _TP_DEBUG
			printf("thread #%d,%p has exited\n", th->tid, th);
#endif
		}
		free(tp->threads);
	} 
}

static void *
_dispatch_work(void *arg) {
	thread_t *th;
	thread_pool_t *tp;
	struct timespec ts;
	work_t *w;
	
	th = (thread_t *)arg;
	tp = th->tp;

	while(1) {
		pthread_mutex_lock(&tp->mutex);

		if (tp->shutdown) {
			pthread_mutex_unlock(&tp->mutex);
			goto EXIT;
		}

		if (_work_queue_empty(tp->wq)) {
			ts.tv_sec = 0;
			ts.tv_nsec = 300000000;/*0.3s*/
			pthread_cond_timedwait(&tp->cond, &tp->mutex, &ts);
			pthread_mutex_unlock(&tp->mutex);
			continue;
		}
		
		w = _work_queue_pop(tp->wq);
		pthread_mutex_unlock(&tp->mutex);
		
		if (w == NULL) continue;
#ifdef _TP_DEBUG
		printf("work:%p(routine:%p, arg:%p) is running on thread #%d,%p\n",
		       w, w->routine, w->arg, th->tid, th);
#endif
		if (w->routine) {
			w->routine(w->arg); 
		}
		free(w);
	}
EXIT:
	return (void *)th;
}


/* ========================================================= */
/*                    work queue                             */
/* ========================================================= */
static work_queue_t *
_work_queue_create() {
	work_queue_t *wq = NULL;

	wq = (work_queue_t *)malloc(sizeof(work_queue_t));
	if (wq == NULL) {
		perror("malloc work_queue_t");
		return NULL;
	}
	
	wq->size = 0;
	wq->head = NULL;
	wq->tail = NULL;

	return wq;
}

static void
_work_queue_destroy(work_queue_t *wq) {
	if (wq == NULL) return;

	/* pop and free all works */
	work_t *w;
    int i = 0;
	for (; i < wq->size; i++) {
		w = _work_queue_pop(wq);
		if (w == NULL) continue;
		free(w);
	}
	
	free(wq);
}

static void
_work_queue_push(work_queue_t *wq, void *(*routine)(void *), void *arg) {
	assert(wq != NULL);
	
	work_t *w;
	
	if (routine == NULL) return;
  
	w = (work_t *)malloc(sizeof(work_t));
	if (w == NULL) {
		perror("malloc work_t");
		return;
	}

	w->routine = routine;
	w->arg = arg;
	w->next = NULL;

	if (wq->size == 0) {
		wq->head = wq->tail = w;
	} else {
		wq->tail->next = w;
		wq->tail = w;
	}
  
	wq->size++;
}

static work_t *
_work_queue_pop(work_queue_t *wq) {
	assert(wq != NULL);
	
	if (wq->size == 0) return NULL;

	work_t *w = NULL;

	w = wq->head;
	wq->head = w->next;
	if (wq->size == 1) {
		wq->tail = wq->head = NULL;
	}

	wq->size--;
	
	return w;
}

static int
_work_queue_empty(work_queue_t *wq) {
	assert(wq != NULL);
	return wq->size == 0;
}
