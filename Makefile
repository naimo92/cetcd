flags := -Wall -I ./include

$(shell mkdir -p ./out)

all:sds_test base64_test thread_pool_test cetcd_test

sds_test:sds.o sds_test.o
	gcc -o sds_test sds.o sds_test.o $(flags)
	mv sds_test ./out

base64_test:base64.o base64_test.o
	gcc -o base64_test base64.o base64_test.o $(flags)
	mv base64_test ./out

thread_pool_test:thread_pool.o thread_pool_test.o
	gcc -o thread_pool_test thread_pool.o thread_pool_test.o -lpthread $(flags)
	mv thread_pool_test ./out

cetcd_test:sds.o base64.o thread_pool.o cetcd.o cetcd_test.o
	gcc -o cetcd_test sds.o base64.o thread_pool.o cetcd.o cetcd_test.o -lcurl -lyajl -lpthread $(flags)
	mv cetcd_test ./out


sds.o:sds.c
	gcc -c sds.c $(flags)

base64.o:base64.c
	gcc -c base64.c $(flags)

thread_pool.o:thread_pool.c
	gcc -c thread_pool.c $(flags)

cetcd.o:cetcd.c
	gcc -c cetcd.c $(flags)

sds_test.o:sds_test.c
	gcc -c sds_test.c $(flags)

base64_test.o:base64_test.c
	gcc -c base64_test.c $(flags)

thread_pool_test.o:thread_pool_test.c
	gcc -c thread_pool_test.c $(flags)

cetcd_test.o:cetcd_test.c
	gcc -c cetcd_test.c $(flags)
	
.PHONY:clean
clean:
	rm -rf *.o
