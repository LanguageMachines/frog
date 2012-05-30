# $Id$
# $URL$

AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(SYSCONFDIR)\" -DPYTHONDIR=\"$(PYTHONDIR)\"
bin_PROGRAMS = frog mbma

frog_SOURCES = Frog.cxx
mbma_SOURCES = mbma_prog.cxx

LDADD = libfrog.la
lib_LTLIBRARIES = libfrog.la
libfrog_la_SOURCES = mbma_mod.cxx mblem_mod.cxx Configuration.cxx \
	Frog-util.cxx mwu_chunker_mod.cxx Parser.cxx \
	cgn_tagger_mod.cxx iob_tagger_mod.cxx ner_tagger_mod.cxx \
	ucto_tokenizer_mod.cxx


TESTS_ENVIRONMENT = $(SHELL) -x
TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
