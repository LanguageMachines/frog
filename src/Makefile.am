# $Id$
# $URL$

AM_CPPFLAGS = -I@top_srcdir@/include

bin_SCRIPTS = frog-update.sh
bin_PROGRAMS = frog

frog_SOURCES = mbma_mod.cxx mblem_mod.cxx mwu_chunker_mod.cxx \
	Frog.cxx Frog-util.cxx Parser.cxx \
	cgn_tagger_mod.cxx iob_tagger_mod.cxx \
	Configuration.cxx ucto_tokenizer_mod.cxx


TESTS_ENVIRONMENT = $(SHELL) -x
TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
