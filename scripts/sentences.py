####
#
# Pylet - Python Language Engineering Toolkit
# Written by Sander Canisius <S.V.M.Canisius@uvt.nl>
#

from itertools import takewhile, ifilter, imap


uttDelimiterTest = lambda line: line.startswith("<utt>")
emptyLineDelimiterTest = lambda line: not line.strip()


class filterFn:

	def __init__(self, delimiterTest):
		self.delimiterTest = delimiterTest
		self.keep = False

	def __call__(self, line):
		if self.delimiterTest(line):
			result = self.keep
			self.keep = False
		else:
			result = True
			self.keep = True
		
		return result


def sentenceIterator(stream, delimiterTest=emptyLineDelimiterTest):
	lineTest = lambda line: not delimiterTest(line)

	# FIXME: the following code makes sure that extra lines of
	# whitespace do not cause the iterator to stop before reaching
	# the actual end of the stream. Most likely, this can be
	# implemented more efficiently.
	stream = ifilter(filterFn(delimiterTest), stream)

	sentence = map(str.split, takewhile(lineTest, stream))
	while sentence:
		yield sentence
		sentence = map(str.split, takewhile(lineTest, stream))


