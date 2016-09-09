all: test
test: test.c ../thread_pool.c
	gcc -g -Wall -D_TP_DEBUG test.c ../thread_pool.c -I ../  -lpthread -o $@
clean:
	rm -rf test *.o
