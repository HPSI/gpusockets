CC = gcc
CFLAGS = -Wall -ggdb
PROGS = server client libcudawrapper.so
#CUDA_PATH = /various/ananos-temp/cuda-5.0
LDLIBS = -lprotobuf-c -lcuda 
CUDA_PATH = /usr

all: server client libcudawrapper

proto: common.proto
	protoc-c --c_out=. $<

server: server.o protocol.o protocol.h process.o process.h common.pb-c.o common.pb-c.h
	$(CC) $(CFLAGS) -o $@ $< protocol.o process.o common.pb-c.o $(LDLIBS)

client: client.o protocol.o protocol.h process.o process.h common.pb-c.o common.pb-c.h
	$(CC) $(CFLAGS) -o $@ $< protocol.o process.o common.pb-c.o $(LDLIBS)

libcudawrapper: libcudawrapper.so.o common.pb-c.so.o common.pb-c.h
	$(CC) $(CFLAGS) -shared -o libcudawrapper.so libcudawrapper.so.o \
	   	common.pb-c.so.o $(LDLIBS) -ldl

process.o: process.c process.h cuda_errors.h
	$(CC) $(CFLAGS) -I$(CUDA_PATH)/include -c $<

%.o: %.c
	$(CC) $(CFLAGS) -I$(CUDA_PATH)/include -c $<

process.so.o: process.c process.h cuda_errors.h
	$(CC) $(CFLAGS) -I$(CUDA_PATH)/include -fPIC -o $@ -c $<

libcudawrapper.so.o: libcudawrapper.c
	$(CC) $(CFLAGS) -I$(CUDA_PATH)/include -fPIC -o $@ -c $<

%.so.o: %.c
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

.PHONY: clean
clean:
	rm -f *.o *.so $(PROGS)
