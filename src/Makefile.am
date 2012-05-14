# $Id$
# $URL$

AM_CPPFLAGS = -I@top_srcdir@/include

bin_SCRIPTS = frog-dp-update
bin_PROGRAMS = frog

frog_SOURCES = mwu_chunker_mod.cxx \
	Frog.cxx Parser.cxx \
	cgn_tagger_mod.cxx iob_tagger_mod.cxx ner_tagger_mod.cxx \
	ucto_tokenizer_mod.cxx

LDADD = libfrog.la
lib_LTLIBRARIES = libfrog.la
libfrog_la_SOURCES = mbma_mod.cxx mblem_mod.cxx Configuration.cxx \
	 Frog-util.cxx

TESTS_ENVIRONMENT = $(SHELL) -x
TESTS = tst.sh

EXTRA_DIST = tst.sh frog-dp-update
CLEANFILES = tst.out
