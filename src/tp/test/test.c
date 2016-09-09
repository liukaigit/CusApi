#include <stdio.h>
#include <unistd.h>
#include "thread_pool.h"

void *work_func(void *arg) {
	int no;
	no = *((int*)arg);
	printf("#%d work is running\n", no);
	
	return NULL;
}


int main(int argc, char **argv) {
	thread_pool_t *tp;

	tp = thread_pool_create(5);
	if (tp == NULL) return -1;

	int i;
	for (i = 1; i < 20; i++) {
		thread_pool_add_work(tp, work_func, (void *)&i);
	}
	thread_pool_wait(tp);
	thread_pool_destroy(tp);
	return 0;

}
