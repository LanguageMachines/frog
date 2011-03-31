#!/usr/bin/env python

"""
Constraint satisfaction inference for dependency parsing.

usage: %prog [options] [file...]

-mDIST, --max-dist=DIST: maximum distance between head and dependent
-x, --exclude-non-scoring: do not generate instance for non-scoring
                           tokens

Constraints:
--dep=FILENAME: classifier output for the C_dep constraint
--dir=FILENAME: classifier output for the C_dir constraint
--mod=FILENAME: classifier output for the C_mod constraint

--out=FILENAME: output file name

Parsing algorithm:
--non-projective: approximate non-projective parsing

---
:Refinements:
-m: type='int', dest='maxDist'
-x: action='store_true', default=False, dest='skipNonScoring'
"""

import fileinput
import sys

from itertools import izip, imap, groupby, ifilter
from operator import itemgetter, attrgetter

from sentences import sentenceIterator

import cky
import csiparse2 as csiparse
import deptree

from common import *


def leftIncomplete(chart, s, t, sentence):
	r = chart[s, t, "l", False].r
	label = chart[s, t, "l", False].edgeLabel
	if r is not None:
		assert s > 0
		sentence[s - 1][DEPREL] = label
		sentence[s - 1][HEAD] = t
		rightComplete(chart, s, r, sentence)
		leftComplete(chart, r + 1, t, sentence)


def rightIncomplete(chart, s, t, sentence):
	r = chart[s, t, "r", False].r
	label = chart[s, t, "r", False].edgeLabel
	if r is not None:
		assert t > 0
		sentence[t - 1][DEPREL] = label
		sentence[t - 1][HEAD] = s
		rightComplete(chart, s, r, sentence)
		leftComplete(chart, r + 1, t, sentence)


def leftComplete(chart, s, t, sentence):
	r = chart[s, t, "l", True].r
	if r is not None:
		leftComplete(chart, s, r, sentence)
		leftIncomplete(chart, r, t, sentence)


def rightComplete(chart, s, t, sentence):
	r = chart[s, t, "r", True].r
	if r is not None:
		rightIncomplete(chart, s, r, sentence)
		rightComplete(chart, r, t, sentence)


def cyclic(sentence, head, dependent):
	if head == dependent:
		return True
	x = sentence[head - 1][HEAD]
	while x > 0:
		if x == dependent:
			return True
		else:
			x = sentence[x - 1][HEAD]

	return False


def evaluateTree(sentence, parser):
	score = 0
	
	for i in xrange(len(sentence)):
		inDeps = set(x[DEPREL] for x in sentence if x[HEAD] == i + 1)
		outDep = sentence[i][DEPREL]

		# C_mod
		for constraint in parser.inDepConstraints[i + 1]:
			if constraint.relType in inDeps:
				score += constraint.weight

		# C_dir
		for constraint in parser.outDepConstraints[i + 1]:
			if (0 < sentence[i][HEAD] < i + 1 and \
				constraint.direction == constraint.LEFT) or \
				(sentence[i][HEAD] > i + 1 and \
				 constraint.direction == constraint.RIGHT) or \
				(sentence[i][HEAD] == 0 and \
				 constraint.direction == constraint.ROOT):
				score += constraint.weight

		# C_dep
		if True: #sentence[i][HEAD] > 0:
			for constraint in parser.edgeConstraints[i + 1][sentence[i][HEAD]]:
				if constraint.relType == sentence[i][DEPREL]:
					score += constraint.weight

	return score


def scoreDiff(sentence, parser, dependent, newHead):
	result = 0
	
	oldHead = sentence[dependent - 1][HEAD]
	oldRel = sentence[dependent - 1][DEPREL]

	# C_dep diff
	if True: #oldHead >= 0:
		depRels = parser.edgeConstraints[dependent][oldHead]
		assert len(depRels) <= 1
		if depRels and depRels[0].relType == oldRel:
			result -= depRels[0].weight
	
	if True: #newHead >= 0:
		depRels = parser.edgeConstraints[dependent][newHead]
		assert len(depRels) <= 1
		if depRels:
			newRel = depRels[0].relType
			result += depRels[0].weight
		else:
			newRel = "unk" if newHead > 0 else "ROOT"

	# C_mod diff
	if oldHead > 0:
		dependents = [x
					  for x in sentence
					  if x[HEAD] == oldHead and x[DEPREL] == oldRel]
		if len(dependents) == 1:
			for constraint in parser.inDepConstraints[oldHead]:
				if constraint.relType == oldRel:
					result -= constraint.weight

	if newHead > 0:
		dependents = [x
					  for x in sentence
					  if x[HEAD] == newHead and x[DEPREL] == newRel]
		if len(dependents) == 0:
			for constraint in parser.inDepConstraints[newHead]:
				if constraint.relType == newRel:
					result += constraint.weight

	# C_dir diff
	if True: #(dependent - oldHead) * (dependent - newHead) < 0:
		for constraint in parser.outDepConstraints[dependent]:
			if (0 < oldHead < dependent and \
				constraint.direction == constraint.LEFT) or \
				(oldHead > dependent and \
				 constraint.direction == constraint.RIGHT) or \
				(oldHead == 0 and \
				 constraint.direction == constraint.ROOT):
				result -= constraint.weight

			if (0 < newHead < dependent and \
				constraint.direction == constraint.LEFT) or \
				(newHead > dependent and \
				 constraint.direction == constraint.RIGHT) or \
				(newHead == 0 and \
				 constraint.direction == constraint.ROOT):
				result += constraint.weight

	return result


def approxNonProjective(sentence, parser):
	#currentScore = 0 #evaluateTree(sentence, parser)
	currentScore = evaluateTree(sentence, parser)

	while True:
		bestScore = float("-inf")
		bestHead = None
		bestDependent = None

		for j in xrange(1, len(sentence) + 1):
			for i in xrange(0, len(sentence) + 1):
				assert type(sentence[j - 1][HEAD]) == int
				if not sentence[j - 1][HEAD] == i and \
					   not cyclic(sentence, i, j):

					score = currentScore + scoreDiff(sentence, parser, j, i)
					########
					#newSentence = [t[:] for t in sentence]
					#newSentence[j - 1][HEAD] = i
					#if True: #i > 0:
					#	depRels = parser.edgeConstraints[j][i]
					#	assert len(depRels) <= 1
					#	if depRels:
					#		newSentence[j - 1][DEPREL] = depRels[0].relType
					#	else:
					#		newSentence[j - 1][DEPREL] = "unk" if i > 0 else "ROOT"
					#assert abs(evaluateTree(newSentence, parser) - score) < 0.0001, ("%s %s" % (j, i))
					#######

					if score > bestScore:
						bestScore = score
						bestHead = i
						bestDependent = j

		if bestScore > currentScore:
#			sys.stderr.write("+")
			currentScore = bestScore
			sentence[bestDependent - 1][HEAD] = bestHead
			if True: #bestHead > 0:
				depRels = parser.edgeConstraints[bestDependent][bestHead]
				assert len(depRels) <= 1
				if depRels:
					sentence[bestDependent - 1][DEPREL] = depRels[0].relType
				else:
					sentence[bestDependent - 1][DEPREL] = "unk" if bestHead > 0 else "ROOT"
			#else:
			#	sentence[bestDependent - 1][DEPREL] = "ROOT"
		else:
			return


def main(args):
	import cmdline
	main_(*cmdline.parse(__doc__, args))


def main_(options, args):
	assert options.dep
	assert options.out
	pairsStream = open(options.dep)
	pairsIterator = csiparse.instanceIterator(pairsStream)
#	print >> sys.stderr, "C_dep constraints enabled"

	if options.dir:
		dirStream = open(options.dir)
		dirIterator = csiparse.instanceIterator(dirStream)
#		print >> sys.stderr, "C_dir constraints enabled"
	else:
		dirIterator = None

	if options.mod:
		relsStream = open(options.mod)
		relsIterator = csiparse.instanceIterator(relsStream)
#		print >> sys.stderr, "C_mod constraints enabled"
	else:
		relsIterator = None
	
	
	for sentence in sentenceIterator(fileinput.input(args)):
		outfile = open(options.out, "w" )

		domains, constraints = csiparse.formulateWCSP(sentence,
													  dirIterator,
													  relsIterator,
													  pairsIterator,
													  options)

		parser = cky.CKYParser(len(sentence))
		for constraint in constraints:
			parser.addConstraint(constraint)

		chart = parser.parse()

		#item = chart[0, self.numTokens - 1, "r", True]

		#for token in sentence:
		#	token[DEPREL] = "__"
		#	token[HEAD] = "__"
		for token in sentence:
			if len(token) <= DEPREL:
				token.extend([None, None])  

		#print chart[0, len(sentence) - 1, "r", True].r + 1
		rightComplete(chart, 0, len(sentence) - 1 + 1, sentence)

		if options.non_projective:
			approxNonProjective(sentence, parser)

		for token in sentence:
			outfile.write( " ".join(map(str, token)) )
			outfile.write("\n")
		
#		sys.stderr.write(".")


if __name__ == "__main__":
	import cmdline
	main(*cmdline.parse())
