all:testlog

testlog:testlog.c
	gcc -o testlog testlog.c ../cuslog.c -lpthread -I ../

clean:
	rm *.0 testlog info.log -rf
