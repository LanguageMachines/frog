import deptree


class SubTree:

	def __init__(self, score, r, edgeLabel):
		self.score = score
		self.r = r
		self.edgeLabel = edgeLabel
		self.satisfiedConstraints = set()


class CKYParser:

	def __init__(self, numTokens):
		self.numTokens = numTokens
		self.inDepConstraints = [[] for i in xrange(numTokens + 1)]
		self.outDepConstraints = [[] for i in xrange(numTokens + 1)]
		self.edgeConstraints = [[[] for i in xrange(numTokens + 1)]
								for i in xrange(numTokens + 1)]

	def addConstraint(self, c):
		if isinstance(c, deptree.HasIncomingRel):
			self.inDepConstraints[c.tokenIndex].append(c)
		elif isinstance(c, deptree.HasDependency):
			self.edgeConstraints[c.tokenIndex][c.headIndex].append(c)
		elif isinstance(c, deptree.DependencyDirection):
			self.outDepConstraints[c.tokenIndex].append(c)

	def bestEdge(self, leftSubTree, rightSubTree, headIndex, depIndex):
		if headIndex == 0:
			score = 0.0
			constraints = set()
			for constraint in self.outDepConstraints[depIndex]:
				if constraint.direction == constraint.ROOT:
					score = constraint.weight
					constraints.add(constraint)
			assert len(self.edgeConstraints[depIndex][0]) <= 1
			label = "ROOT"
			for constraint in self.edgeConstraints[depIndex][0]:
				score += constraint.weight
				constraints.add(constraint)
				label = constraint.relType
			return label, score, constraints

		#headIndex -= 1
		#depIndex -= 1
		
		best = None
		bestScore = -0.5 #-1
		bestConstraints = set()
		for edgeConstraint in self.edgeConstraints[depIndex][headIndex]:
			score = edgeConstraint.weight
			label = edgeConstraint.relType
			constraints = set([edgeConstraint])

			for constraint in self.inDepConstraints[headIndex]:
				if constraint.relType == label and (constraint not in leftSubTree.satisfiedConstraints and constraint not in rightSubTree.satisfiedConstraints):
					score += constraint.weight
					constraints.add(constraint)

			for constraint in self.outDepConstraints[depIndex]:
				if ((constraint.direction == constraint.LEFT and headIndex < depIndex) or (constraint.direction == constraint.RIGHT and headIndex > depIndex)) and (constraint not in leftSubTree.satisfiedConstraints and constraint not in rightSubTree.satisfiedConstraints):
					score += constraint.weight
					constraints.add(constraint)

			if score > bestScore:
				bestScore = score
				best = label
				bestConstraints = constraints

		return best, bestScore, bestConstraints

	def parse(self):
		C = {}
		for s in xrange(self.numTokens + 1):
			for d in ["r", "l"]:
				for c in [True, False]:
					C[s, s, d, c] = SubTree(0.0, None, None)
		
		for k in xrange(1, self.numTokens + 1 + 1):
			for s in xrange(self.numTokens - k + 1):
				t = s + k

				best = -1, "__" #####None
				bestScore = float("-inf") #-1
				bestConstraints = None
				for r in xrange(s, t):
					label, edgeScore, constraints = self.bestEdge(
						C[s, r, "r", True],
						C[r + 1, t, "l", True],
						t, s)

					score = C[s, r, "r", True].score + \
							C[r + 1, t, "l", True].score + \
							edgeScore
					
					if score > bestScore:
						bestScore = score
						best = r, label
						bestConstraints = constraints

				C[s, t, "l", False] = SubTree(bestScore, *best)
				C[s, t, "l", False].satisfiedConstraints.update(
					C[s, best[0], "r", True].satisfiedConstraints)
				C[s, t, "l", False].satisfiedConstraints.update(
					C[best[0] + 1, t, "l", True].satisfiedConstraints)
				C[s, t, "l", False].satisfiedConstraints.update(
					bestConstraints)
				

				best = -1, "__" #######None
				bestScore = float("-inf") #-1
				bestConstraints = None
				for r in xrange(s, t):
					label, edgeScore, constraints = self.bestEdge(
						C[s, r, "r", True],
						C[r + 1, t, "l", True],
						s, t)
					
					score = C[s, r, "r", True].score + \
							C[r + 1, t, "l", True].score + \
							edgeScore
					
					if score > bestScore:
						bestScore = score
						best = r, label
						bestConstraints = constraints

				C[s, t, "r", False] = SubTree(bestScore, *best)
				C[s, t, "r", False].satisfiedConstraints.update(
					C[s, best[0], "r", True].satisfiedConstraints)
				C[s, t, "r", False].satisfiedConstraints.update(
					C[best[0] + 1, t, "l", True].satisfiedConstraints)
				C[s, t, "r", False].satisfiedConstraints.update(
					bestConstraints)
				

				best = -1 ######None
				bestScore = float("-inf") #-1
				for r in xrange(s, t):
					score = C[s, r, "l", True].score + \
							C[r, t, "l", False].score
					if score > bestScore:
						bestScore = score
						best = r

				C[s, t, "l", True] = SubTree(bestScore, best, None)
				C[s, t, "l", True].satisfiedConstraints.update(
					C[s, best, "l", True].satisfiedConstraints)
				C[s, t, "l", True].satisfiedConstraints.update(
					C[best, t, "l", False].satisfiedConstraints)
				

				best = -1 ####None
				bestScore = float("-inf") #-1
				for r in xrange(s + 1, t + 1):
					score = C[s, r, "r", False].score + \
							C[r, t, "r", True].score
					if score > bestScore:
						bestScore = score
						best = r

				C[s, t, "r", True] = SubTree(bestScore, best, None)
				C[s, t, "r", True].satisfiedConstraints.update(
					C[s, best, "r", False].satisfiedConstraints)
				C[s, t, "r", True].satisfiedConstraints.update(
					C[best, t, "r", True].satisfiedConstraints)
				

		return C
