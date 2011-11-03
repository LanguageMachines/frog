# $Id$
# $URL$

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src include scripts m4 config docs tests

EXTRA_DIST = bootstrap AUTHORS TODO NEWS

ChangeLog: $(top_srcdir)/NEWS
	svn update; svn2cl --include-rev
