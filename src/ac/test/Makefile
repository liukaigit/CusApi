all:test

test:test.o util-mpm.o util-mpm-ac.o
	gcc -o test test.o util-mpm.o util-mpm-ac.o -I ../

test.o:test.c
	gcc -c test.c -I ../

util-mpm.o:../util-mpm.c
	gcc -c ../util-mpm.c -I ../

util-mpm-ac.o:../util-mpm-ac.c
	gcc -c ../util-mpm-ac.c -I ../

clean:
	rm *.o test -rf
