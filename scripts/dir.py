#!/usr/bin/env python

import fileinput

from itertools import izip
from operator import itemgetter

from sentences import sentenceIterator, makeWindow

from common import *


def main(args):
	for sentence in sentenceIterator(fileinput.input()):
		for i, token in enumerate(sentence):
			features = []

			#if i == 0:
			#	features.append("FIRST")
			#elif i + 1 == len(sentence):
			#	features.append("LAST")
			#else:
			#	features.append("MIDDLE")

			# window of words
			features.extend(makeWindow(sentence, i, 2, 2, itemgetter(FORM)))

			# window of pos tags
			features.extend(makeWindow(sentence, i, 2, 2, itemgetter(POSTAG)))

			# window of word-tag conjunctions
			features.extend(makeWindow(sentence, i, 2, 2, lambda x: "%s^%s" % (x[FORM], x[POSTAG])))

			# pos conjunctions
			features.append("^".join(makeWindow(sentence, i, 1, 0, itemgetter(POSTAG))))
			features.append("^".join(makeWindow(sentence, i, 0, 1, itemgetter(POSTAG))))

			#features.append("^".join(makeWindow(sentence, i, 2, 0, itemgetter(POSTAG))))
			#features.append("^".join(makeWindow(sentence, i, 0, 2, itemgetter(POSTAG))))


			#features.append(token[FEATS])
			features.extend(makeWindow(sentence, i, 1, 1, itemgetter(FEATS)))
			
			if token[HEAD] == "0":
				dir = "ROOT"
			else:
				dir = ["LEFT", "RIGHT"][int(token[ID]) < int(token[HEAD])]

			print " ".join(features), dir

		print


if __name__ == "__main__":
	import sys
	main(sys.argv[1:])
