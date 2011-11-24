# $Id$
# $URL$

AM_CPPFLAGS = -I@top_srcdir@/include

bin_PROGRAMS = frog

frog_SOURCES = mbma_mod.cxx mblem_mod.cxx mwu_chunker_mod.cxx \
	Frog.cxx Frog-util.cxx Parser.cxx tagger_mod.cxx \
	Configuration.cxx


TESTS_ENVIRONMENT = $(SHELL) -x
TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
