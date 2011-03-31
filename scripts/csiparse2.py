#!/usr/bin/env python

"""
Generate instances for the pairwise dependency prediction task.

usage: %prog [options] [file...]

-mDIST, --max-dist=DIST: maximum distance between head and dependent

---
:Refinements:
-m: type='int', dest='maxDist'
"""

import fileinput

from itertools import imap, izip

from sentences import sentenceIterator

import common
import deptree


def list_rindex(lst, x):
	for i in xrange(len(lst) - 1, -1, -1):
		if lst[i] == x:
			return i
	raise ValueError("")


def parseDist(string):
	dist = dict((label.strip(), float(weight))
				for label, weight in map(str.split, string.strip().split(",")))
	normFactor = sum(dist.itervalues())
	for cls, weight in dist.iteritems():
		dist[cls] = weight / normFactor
	
	return dist


def parseInstanceLine(line):
	instance = line.split()
	startIndex = list_rindex(instance, "{")
	endIndex = instance.index("}", startIndex)
	dist = parseDist(" ".join(instance[startIndex:endIndex]).strip("{}"))
	return instance[:startIndex], dist, 0 #float(instance[endIndex+1])


def instanceIterator(stream):
	return imap(parseInstanceLine, stream)


def formulateWCSP(sentence, dirInstances,
				  relInstances, pairInstances,
				  options):
	domains = [None] + [[] for token in sentence]
	constraints = []

	for dependent in sentence:
		dependentId = int(dependent[0])
		headId = 0
		instance, distribution, distance = pairInstances.next()

		cls = instance[-1]
		conf = distribution[cls]

		if cls != "__":
			constraints.append(deptree.HasDependency(dependentId,
													 headId, cls,
													 conf))
		if cls != "__":
			domains[dependentId].append((headId, cls))

	for dependent, head in common.pairIterator(sentence, options):
		dependentId = int(dependent[0])
		headId = int(head[0])

		instance, distribution, distance = pairInstances.next()

		cls = instance[-1]
		conf = distribution[cls]
		#conf = sum(distribution[x] for x in distribution if x != "__")

		#if distance == 0:
		#	conf = 1000000
		#else:
		#	conf = 1.0 / distance

		#constraints.append(deptree.HasDependency(dependentId,
		#										 headId, cls,
		#										 conf))
		if cls != "__":
			constraints.append(deptree.HasDependency(dependentId,
													 headId, cls,
													 conf))
		if cls != "__":
			domains[dependentId].append((headId, cls))


	if dirInstances:
		for token, (instance, distribution, distance) in izip(sentence,
															  dirInstances):
			tokenId = int(token[0])
			cls = instance[-1]
			if True: #cls != "__":
				#constraints.append(deptree.DependencyDirection(
				#	tokenId,
				#	getattr(deptree.DependencyDirection, cls),
				#	distribution[cls]))

				for cls in distribution:
					constraints.append(deptree.DependencyDirection(
						tokenId,
						getattr(deptree.DependencyDirection, cls),
						distribution[cls]))

				#if "__" in distribution:
				#	constraints.append(deptree.DependencyDirection(
				#		tokenId, 100, distribution["__"]))
				#if "RIGHT" in distribution:
				#	cls = "RIGHT"
				#	constraints.append(deptree.DependencyDirection(
				#		tokenId,
				#		getattr(deptree.DependencyDirection, cls),
				#		distribution[cls]))
				#if "LEFT" in distribution:
				#	cls = "LEFT"
				#	constraints.append(deptree.DependencyDirection(
				#		tokenId,
				#		getattr(deptree.DependencyDirection, cls),
				#		distribution[cls]))
			#else:
			#	constraints.append(deptree.DependencyDirection(
			#		tokenId, deptree.DependencyDirection.LEFT,
			#		-distribution[cls]))
			#	constraints.append(deptree.DependencyDirection(
			#		tokenId, deptree.DependencyDirection.RIGHT,
			#		-distribution[cls]))

	if relInstances:
		for token, (instance, distribution, distance) in izip(sentence,
															  relInstances):
			tokenId = int(token[0])
			cls = instance[-1]

			distribution = dict((tuple(key.split("|")), value)
								for key, value
								in distribution.iteritems())

			if cls != "__":
				for rel in cls.split("|"):
					conf = sum(value
							   for key, value
							   in distribution.iteritems()
							   if rel in key)

					constraints.append(deptree.HasIncomingRel(tokenId,
															  rel,
															  conf))


	return domains, constraints


def main(options, args):
	dirOutput, relsOutput, pairsOutput = args[:3]

	dirStream = open(dirOutput)
	relsStream = open(relsOutput)
	pairsStream = open(pairsOutput)

	dirIterator = instanceIterator(dirStream)
	relsIterator = instanceIterator(relsStream)
	pairsIterator = instanceIterator(pairsStream)
	
	for sentence in sentenceIterator(fileinput.input(args[3:])):
		csp = formulateWCSP(sentence,
							dirIterator,
							relsIterator,
							pairsIterator,
							options)


	dirStream.close()
	relsStream.close()
	pairsStream.close()


if __name__ == "__main__":
	import cmdline
	main(*cmdline.parse())
