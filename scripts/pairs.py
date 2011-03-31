#!/usr/bin/env python

"""
Generate instances for the pairwise dependency prediction task.

usage: %prog [options] [file...]

-mDIST, --max-dist=DIST: maximum distance between head and dependent
-x, --exclude-non-scoring: do not generate instance for non-scoring
                           tokens
-t, --test: test data mode: the input does not contain HEAD and DEPREL
            columns
-s, --separate-sentences: separate instances from different sentences by
                          an empty line

Features:
--bigram: add a head-dependent bigram feature
--feats-bigram: add a head-dependent bigram of the FEATS column

---
:Refinements:
-m: type='int', dest='maxDist'
-x: action='store_true', default=False, dest='skipNonScoring'
-t: action='store_true', default=False
-s: action='store_true', default=False, dest='separateSentences'
--bigram: action='store_true', default=False
--feats-bigram: action='store_true', default=False, dest='featsBigram'
"""

import fileinput

from itertools import izip
from operator import itemgetter

from sentences import sentenceIterator, makeWindow

import common

from common import *  # COLUMN INDEXES ONLY


def main(options, args):
	for sentence in sentenceIterator(fileinput.input(args)):
		for dependent in sentence:
			dist = "ROOTDEP"
			features = []

			dependentId = int(dependent[ID]) - 1

			# window of words
			features.extend(makeWindow(sentence, dependentId,
									   1, 1, itemgetter(FORM)))
#									   2, 2, itemgetter(FORM)))
			#features.extend(makeWindow(sentence, headId,
			#						   1, 1, itemgetter(FORM)))

			features.extend(["ROOT", "ROOT", "ROOT"])
			
#									   2, 2, itemgetter(FORM)))

			# window of pos tags
			features.extend(makeWindow(sentence, dependentId,
									   1, 1, itemgetter(POSTAG)))
#									   2, 2, itemgetter(POSTAG)))
			#features.extend(makeWindow(sentence, headId,
			#						   1, 1, itemgetter(POSTAG)))
			features.extend(["ROOT", "ROOT", "ROOT"])
#									   2, 2, itemgetter(POSTAG)))

			#for id in [dependentId, headId]:
			#	window = makeWindow(sentence, id,
			#	                    2, 2, itemgetter(POSTAG))
			#	features.append("%s^%s" % tuple(window[:2]))
			#	features.append("%s^%s" % tuple(window[-2:]))

			features.append("%s^%s" % (dependent[POSTAG], "ROOT"))

			# relative position, distance
			#features.append(
			#	["LEFT", "RIGHT"][dependentId < headId])
			features.append("ROOT")
			#features.append(str(abs(dependentId - headId)))
			features.append("ROOT")

			if options.bigram:
				features.append("^".join(["ROOT", dependent[FORM]]))

			if options.featsBigram:
				features.append("^".join(["ROOT", dependent[FEATS]]))

			#posTags = map(itemgetter(CPOSTAG), sentence[min(dependentId, headId):max(dependentId, headId)])
			#for tag in ["CC", "CD", "DT", "EX", "FW", "IN", "JJ", "MD",
			#            "NN", "PD", "PO", "PR", "RB", "RP", "SY", "TO",
			#            "UH", "VB", "WD", "WP", "WR"]:
			#	features.append(str(sum(1 for t in posTags if t == tag)))
			
			if not options.test:
				if dependent[HEAD] == "0":
					rel = dependent[DEPREL]
				else:
					rel = "__"
			else:
				rel = "?"

			print " ".join(features), rel
		
		for dependent, head in common.pairIterator(sentence, options):
			dist = abs(int(dependent[ID]) - int(head[ID]))
			features = []

			dependentId = int(dependent[ID]) - 1
			headId = int(head[ID]) - 1

			# window of words
			features.extend(makeWindow(sentence, dependentId,
									   1, 1, itemgetter(FORM)))
#									   2, 2, itemgetter(FORM)))
			features.extend(makeWindow(sentence, headId,
									   1, 1, itemgetter(FORM)))
#									   2, 2, itemgetter(FORM)))

			# window of pos tags
			features.extend(makeWindow(sentence, dependentId,
									   1, 1, itemgetter(POSTAG)))
#									   2, 2, itemgetter(POSTAG)))
			features.extend(makeWindow(sentence, headId,
									   1, 1, itemgetter(POSTAG)))
#									   2, 2, itemgetter(POSTAG)))

			#for id in [dependentId, headId]:
			#	window = makeWindow(sentence, id,
			#	                    2, 2, itemgetter(POSTAG))
			#	features.append("%s^%s" % tuple(window[:2]))
			#	features.append("%s^%s" % tuple(window[-2:]))

			features.append("%s^%s" % (dependent[POSTAG], head[POSTAG]))

			# relative position, distance
			features.append(
				["LEFT", "RIGHT"][dependentId < headId])
			features.append(str(abs(dependentId - headId)))

			if options.bigram:
				features.append("^".join([head[FORM], dependent[FORM]]))

			if options.featsBigram:
				features.append("^".join([head[FEATS], dependent[FEATS]]))

			#posTags = map(itemgetter(CPOSTAG), sentence[min(dependentId, headId):max(dependentId, headId)])
			#for tag in ["CC", "CD", "DT", "EX", "FW", "IN", "JJ", "MD",
			#            "NN", "PD", "PO", "PR", "RB", "RP", "SY", "TO",
			#            "UH", "VB", "WD", "WP", "WR"]:
			#	features.append(str(sum(1 for t in posTags if t == tag)))
			
			if not options.test:
				if dependent[HEAD] == head[ID]:
					rel = dependent[DEPREL]
				else:
					rel = "__"
			else:
				rel = "?"

			print " ".join(features), rel

		if options.separateSentences:
			print


if __name__ == "__main__":
	import cmdline
	main(*cmdline.parse())
