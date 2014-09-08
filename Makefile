CC = gcc
CFLAGS = -Wall -ggdb
PROGS = server test-client test-cuda
#CUDA_PATH = /various/ananos-temp/cuda-5.0
LDLIBS = -lprotobuf-c -lcuda 
CUDA_PATH = /usr

all: libcudawrapper $(PROGS)

proto: common.proto
	protoc-c --c_out=. $<

server: server.o protocol.o protocol.h process.o process.h common.pb-c.o common.pb-c.h common.o common.h
	$(CC) $(CFLAGS) -o $@ $< protocol.o process.o common.pb-c.o common.o \
		$(LDLIBS)

test-client: test-client.o protocol.o protocol.h process.o process.h common.pb-c.o common.pb-c.h common.o common.h
	$(CC) $(CFLAGS) -o $@ $< protocol.o process.o common.pb-c.o common.o \
		$(LDLIBS)

libcudawrapper: libcudawrapper.so.o client.so.o client.h protocol.so.o protocol.h process.so.o process.h common.pb-c.so.o common.pb-c.h common.so.o common.h
	$(CC) $(CFLAGS) -shared -o libcudawrapper.so libcudawrapper.so.o \
	   	client.so.o protocol.so.o process.so.o common.pb-c.so.o common.so.o \
		$(LDLIBS) -ldl

test-cuda: test-cuda.o
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)


%.o: %.c
	$(CC) $(CFLAGS) -I$(CUDA_PATH)/include -c $<

process.o: process.c process.h cuda_errors.h list.h common.pb-c.h common.h

client.o: client.c client.h protocol.h process.h common.pb-c.h common.h

%.so.o: %.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

libcudawrapper.so.o: libcudawrapper.c client.h protocol.h common.pb-c.h common.h
	$(CC) $(CFLAGS) -I$(CUDA_PATH)/include -fPIC -o $@ -c $<

process.so.o: process.c process.h cuda_errors.h list.h common.pb-c.h common.h

client.so.o: client.c client.h protocol.h process.h common.pb-c.h common.h


.PHONY: clean
clean:
	rm -f *.o *.so $(PROGS)
