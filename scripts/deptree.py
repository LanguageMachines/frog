
class Constraint:

	def __init__(self, weight):
		self.weight = weight

class HasIncomingRel(Constraint):

	def __init__(self, tokenIndex, relType, weight):
		Constraint.__init__(self, weight)
		self.tokenIndex = tokenIndex
		self.relType = relType

class HasDependency(Constraint):

	def __init__(self, tokenIndex, headIndex, relType, weight):
		Constraint.__init__(self, weight)
		self.tokenIndex = tokenIndex
		self.headIndex = headIndex
		self.relType = relType

class DependencyDirection(Constraint):

	ROOT, LEFT, RIGHT = range(3)

	def __init__(self, tokenIndex, direction, weight):
		Constraint.__init__(self, weight)
		self.tokenIndex = tokenIndex
		self.direction = direction

