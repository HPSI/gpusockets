bin_PROGRAMS = server libcudawrapper.so #test-cuda test-client


AM_CFLAGS = -I$(PROTOBUF_C_CFLAGS) -L$(PROTOBUF_C_LIBDIR) -L$(CUDA) -I@builddir@ -I/usr/include/google -L$(CUDA_INSTALL_PATH)/lib -I$(CUDA_INSTALL_PATH)/include $(TIMERS_CFLAGS) $(DEBUG_CFLAGS)


BUILT_SOURCES = @srcdir@/common.pb-c.c @srcdir@/common.pb-c.h

server_SOURCES = server.c process.c process.h common.h common.c protocol.c protocol.h list.h cuda_errors.h timer.h
server_SOURCES += common.pb-c.c common.pb-c.h

libcudawrapper_so_CFLAGS = -fPIC -shared $(TIMERS_CFLAGS) $(DEBUG_CFLAGS)
libcudawrapper_so_CFLAGS +=  -L$(CUDA_INSTALL_PATH)/lib -I$(CUDA_INSTALL_PATH)/include
libcudawrapper_so_SOURCES = libcudawrapper.c process.c process.h common.h common.c protocol.c protocol.h list.h cuda_errors.h client.h client.c
libcudawrapper_so_SOURCES += common.pb-c.c common.pb-c.h

common.pb-c.c: @srcdir@/common.proto
	 $(PROTOC_C) --proto_path=@srcdir@ --c_out=. @srcdir@/common.proto

common.pb-c.h: @srcdir@/common.pb-c.c
	@if test ! -f $@; then \
	  rm -f $<; \
	  $(MAKE) $(AM_MAKEFLAGS) $<; \
	else :; fi

CLEANFILES = @builddir@/common.pb-c.c @builddir@/common.pb-c.h

server_LDADD = $(PROTOBUF_C_LIBS) $(CUDA_LIBS) -lcuda $(PTHREAD_LIBS) $(TIMERS_LIBS)
libcudawrapper_so_LDADD = $(PROTOBUF_C_LIBS) $(CUDA_LIBS) -lcuda -ldl $(TIMERS_LIBS)

EXTRA_DIST = common.proto

AM_CFLAGS += -I@builddir@


