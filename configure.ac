PACKAGE=gpusockets
VERSION=0.1.0
AC_INIT(gpusockets, 0.1.0)
AC_CONFIG_SRCDIR([src])

AM_INIT_AUTOMAKE

AC_ARG_WITH([protobuf-c-libdir],
    [AS_HELP_STRING([--with-protobuf-c-libdir=LIB_DIR],
        [location of the protocol buffers libraries, defaults to /usr/lib])],
    [PROTOBUF_C_LIBDIR="$withval"],
    [PROTOBUF_C_LIBDIR='/usr/lib'])

#AX_LIB_PROTOBUF_C([0.14])

AC_CHECK_LIB([protobuf-c], [main], [], [AC_MSG_ERROR([cannot find protobuf library])])
AC_SUBST(PROTOBUF_C_LIBDIR)
AC_SUBST(PROTOBUF_C_CFLAGS)
AC_SUBST(PROTOBUF_C_VERSION)
LDFLAGS="$LDFLAGS -L$PROTOBUF_C_LIBDIR"


AC_ARG_WITH([protoc-c],
    [AS_HELP_STRING([--with-protoc-c=PATH],
        [location of the protoc protocol buffer compiler binary, defaults to protoc])],
    [PROTOC_C="$withval"],
    [PROTOC_C='protoc-c'])
AC_SUBST([PROTOC_C])

AC_PROG_CXX

AM_SILENT_RULES([no])

AC_ARG_WITH([cuda],
   [AS_HELP_STRING(
        [--with-cuda=PATH],
        [prefix where CUDA is installed @<:@default=auto@:>@])],
   [CUDA_INSTALL_PATH=$withval],
   [with_cuda=auto])
AC_SUBST([CUDA_INSTALL_PATH])

AC_OUTPUT(Makefile src/Makefile)
