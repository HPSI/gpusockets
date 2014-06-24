CC=gcc
#CFLAGS=-Wall
LDLIBS=-lprotobuf-c
PROGS=server client

all: server client

server: server.o common.pb-c.o common.pb-c.h
	$(CC) $(CFLAGS) -o $@ $< common.pb-c.o $(LDLIBS)

client: client.o common.pb-c.o common.pb-c.h
	$(CC) $(CFLAGS) -o $@ $< common.pb-c.o $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY : clean
clean:
	rm *.o $(PROGS)
