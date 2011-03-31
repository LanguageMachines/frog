# $Id$
# $URL$

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src include scripts m4 config docs

EXTRA_DIST = bootstrap AUTHORS TODO NEWS test.txt

ChangeLog: $(top_srcdir)/NEWS
	svn update; svn2cl --include-rev
