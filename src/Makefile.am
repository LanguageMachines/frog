AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(datadir)\" -std=c++11 -W -Wall -pedantic -g -O3
bin_PROGRAMS = frog mbma mblem ner

frog_SOURCES = Frog.cxx
mbma_SOURCES = mbma_prog.cxx
mblem_SOURCES = mblem_prog.cxx
ner_SOURCES = ner_prog.cxx

LDADD = libfrog.la
lib_LTLIBRARIES = libfrog.la
libfrog_la_LDFLAGS = -version-info 2:0:0

libfrog_la_SOURCES = FrogAPI.cxx FrogData.cxx \
	mbma_rule.cxx mbma_mod.cxx mbma_brackets.cxx clex.cxx \
	mblem_mod.cxx csidp.cxx ckyparser.cxx \
	Frog-util.cxx mwu_chunker_mod.cxx Parser.cxx AlpinoParser.cxx \
	tagger_base.cxx cgn_tagger_mod.cxx \
	iob_tagger_mod.cxx \
	ner_tagger_mod.cxx \
	ucto_tokenizer_mod.cxx


TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
