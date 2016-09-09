all:libtoolhub.a

OBJS = getstr.o kmp.o
SRCS = getstr.c kmp.c

libtoolhub.a:$(OBJS)
	ar crv ../lib/libtoolhub.a $(OBJS)

$(OBJS):$(SRCS)
	gcc -c $^ 

clean:
	rm *.o a.out -rf
