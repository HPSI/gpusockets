
#! /bin/sh

aclocal \
&& automake --gnu --add-missing \
&& autoreconf -ivf

