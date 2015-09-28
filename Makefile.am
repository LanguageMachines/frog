# $Id$
# $URL$

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src include m4 docs tests

EXTRA_DIST = bootstrap.sh AUTHORS TODO NEWS

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = frog.pc

ChangeLog: $(top_srcdir)/NEWS
	svn update; svn2cl --include-rev
