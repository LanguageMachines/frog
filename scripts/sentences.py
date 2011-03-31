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


def nonDelimitedSentenceIterator(stream, sentenceStartTest):
	sentence = []
	for line in imap(str.split, stream):
		if sentenceStartTest(line) and sentence:
			yield sentence
			sentence = []

		sentence.append(line)

	if sentence:
		yield sentence


def makeWindow(tokens, focusIndex, leftSize, rightSize, labelFunction,
			   emptySlotLabel="__"):
	return max(0, leftSize - focusIndex) * [emptySlotLabel] + \
		   map(labelFunction,
			   tokens[max(0, focusIndex - leftSize):min(len(tokens), focusIndex + rightSize + 1)]) + \
		   max(0, focusIndex + rightSize - len(tokens) + 1) * [emptySlotLabel]
