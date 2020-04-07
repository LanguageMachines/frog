man1_MANS = frog.1 mbma.1 mblem.1 ner.1
EXTRA_DIST = frog.1 mbma.1 mblem.1 ner.1

# https://stackoverflow.com/questions/10682603/generating-and-installing-doxygen-documentation-with-autotools

if HAVE_DOXYGEN
directory = $(top_srcdir)/docs/html

man_MANS = $(directory)

$(directory): doxyfile.stamp

doxyfile.stamp: Doxygen.cfg
	$(DOXYGEN) $^
	echo Timestamp > $@

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp
clean-local:
	-rm -rf $(top_srcdir)/docs/html

endif
