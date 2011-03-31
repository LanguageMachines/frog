#!/usr/bin/env python

import fileinput

from itertools import izip
from operator import itemgetter

from sentences import sentenceIterator, makeWindow

from common import *


def main(args):
	for sentence in sentenceIterator(fileinput.input()):
		rels = [set() for t in sentence]
		for token in sentence:
			if token[HEAD] != "0":
				head = int(token[HEAD]) - 1
				rels[head].add(token[DEPREL])

		for i, token in enumerate(sentence):
			features = []
		
			# window of words
			features.extend(makeWindow(sentence, i, 2, 2, itemgetter(FORM)))

			features.append(token[FEATS])
			
			# window of pos tags
			features.extend(makeWindow(sentence, i, 2, 2, itemgetter(POSTAG)))

			# pos conjunctions
			features.append("^".join(makeWindow(sentence, i, 1, 0, itemgetter(POSTAG))))
			features.append("^".join(makeWindow(sentence, i, 0, 1, itemgetter(POSTAG))))

			features.append("^".join(makeWindow(sentence, i, 2, 0, itemgetter(POSTAG))))
			features.append("^".join(makeWindow(sentence, i, 0, 2, itemgetter(POSTAG))))

			## distance to comma
			#left = map(itemgetter(FORM), sentence[:i])
			#right = map(itemgetter(FORM), sentence[i+1:])
			#left.reverse()
			#if "," in left:
			#	features.append(str(left.index(",")))
			#else:
			#	features.append("-")

			#if "," in right:
			#	features.append(str(right.index(",")))
			#else:
			#	features.append("-")

			## bag-of-POS
			#left = map(itemgetter(CPOSTAG), sentence[:i])
			#left.reverse()
			#right = map(itemgetter(CPOSTAG), sentence[i+1:])
			#for tag in ["CC", "CD", "DT", "EX", "FW", "IN", "JJ", "MD",
			#            "NN", "PD", "PO", "PR", "RB", "RP", "SY", "TO",
			#            "UH", "VB", "WD", "WP", "WR"]:
			#	features.append(str(left.index(tag)) if tag in left else "-")
			#	features.append(str(right.index(tag)) if tag in right else "-")

			print " ".join(features), "|".join(sorted(rels[i])) or "__"

		print


if __name__ == "__main__":
	import sys
	main(sys.argv[1:])
