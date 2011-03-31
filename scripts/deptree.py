from itertools import izip


class Node:

	def __init__(self):
		self.outgoingRelation = None
		self.incomingRelations = set()


class DependencyTree:

	def __init__(self, numTokens):
		self.nodes = [Node() for i in xrange(numTokens)]
		self.relations = []

	def addRelation(self, source, dest, relType):
		rel = (self.nodes[source], self.nodes[dest], relType)
		self.relations.append(rel)
		self.nodes[source].outgoingRelation = (dest, relType)
		self.nodes[dest].incomingRelations.add((source, relType))

	def removeRelation(self, source):
		dest, relType = self.nodes[source].outgoingRelation
		rel = (self.nodes[source], self.nodes[dest], relType)
		self.relations.remove(rel)
		self.nodes[source].outgoingRelation = None
		self.nodes[dest].incomingRelations.remove((source, relType))

	def findCycle(self):
		visitedNodes = set()
		for node in self.nodes:
			if node not in visitedNodes:
				result = self.visitNode(node, visitedNodes)
				if result:
					return result

	def visitNode(self, node, visitedNodes):
		currentNodes = set()
		currentNode = node
		path = []

		while currentNode.outgoingRelation:
			currentNodes.add(currentNode)
			visitedNodes.add(currentNode)

			path.append(currentNode)
			
			dest, relType = currentNode.outgoingRelation
			dest = self.nodes[dest]
			if dest in currentNodes:
				return path[path.index(dest):]
			elif dest in visitedNodes:
				return None
			else:
				currentNode = dest

		return None


class Constraint:

	def __init__(self, weight):
		self.weight = weight

	def isSatisfied(self, tree):
		raise NotImplementedError

	def getValue(self, tree):
		if self.isSatisfied(tree):
			return self.weight
		else:
			return 0.0


class HasIncomingRel(Constraint):

	def __init__(self, tokenIndex, relType, weight):
		Constraint.__init__(self, weight)
		self.tokenIndex = tokenIndex
		self.relType = relType

	def isSatisfied(self, tree):
		return self.relType in [rel[1] for rel in tree.nodes[self.tokenIndex].incomingRelations]


class HasDependency(Constraint):

	def __init__(self, tokenIndex, headIndex, relType, weight):
		Constraint.__init__(self, weight)
		self.tokenIndex = tokenIndex
		self.headIndex = headIndex
		self.relType = relType

	def isSatisfied(self, tree):
		return tree.nodes[self.tokenIndex].outgoingRelation == (self.headIndex, self.relType)


class DependencyDirection(Constraint):

	ROOT, LEFT, RIGHT = range(3)

	def __init__(self, tokenIndex, direction, weight):
		Constraint.__init__(self, weight)
		self.tokenIndex = tokenIndex
		self.direction = direction

	def isSatisfied(self, tree):
		return tree.nodes[self.tokenIndex].outgoingRelation != None and int(tree.nodes[self.tokenIndex].outgoingRelation[0] - self.tokenIndex > 0) == self.direction


def writeDepTree(tree, sentence, stream=None):
	for node, token in izip(tree.nodes, sentence):
		line = list(token)

		if node.outgoingRelation:
			headIndex, relType = node.outgoingRelation
			line[6:8] = str(headIndex + 1), relType
		else:
			line[6:8] = "0", "ROOT"

		print >> stream, "\t".join(line)

	print >> stream
