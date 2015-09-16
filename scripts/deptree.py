import sys

class Constraint:

	def __init__(self, weight, tokenIndex ):
		self.weight = weight
		self.tokenIndex = tokenIndex

        def __str__(self):
                return str(self.tokenIndex) + " " + str(self.weight)
        
class HasIncomingRel(Constraint):

	def __init__(self, tokenIndex, relType, weight):
		Constraint.__init__(self, weight, tokenIndex)
		self.relType = relType

        def __str__(self):
                return str(self.tokenIndex) + " " + str(self.weight) + " REL=" + str(self.relType)
        
class HasDependency(Constraint):

	def __init__(self, tokenIndex, headIndex, relType, weight):
		Constraint.__init__(self, weight, tokenIndex )
		self.headIndex = headIndex
		self.relType = relType
                
        def __str__(self):
                return str(self.tokenIndex) + " " + str(self.weight) + " REL=" + str(self.relType) + " HEAD=" + str(self.headIndex)
                
class DependencyDirection(Constraint):

	ROOT, LEFT, RIGHT = range(3)

	def __init__(self, tokenIndex, direction, weight):
		Constraint.__init__(self, weight,tokenIndex)
		self.direction = direction

        def __str__(self):
                return str(self.tokenIndex) + " " + str(self.weight) + " DIR=" + str(self.direction)
