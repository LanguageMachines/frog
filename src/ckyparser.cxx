#include <iostream>
#include <vector>
#include <map>
#include <string>

#include "frog/ckyparser.h"

using namespace std;

ostream& operator<<( ostream& os, const Constraint* c ){
  if ( c ){
    c->put( os );
  }
  else
    os << "None";
  os << endl;
  return os;
}

ostream& operator<<( ostream& os, const Constraint& c ){
  return os << &c;
}

void HasIncomingRel::put( ostream& os ) const {
  Constraint::put( os );
  os << " REL " << relType;
}


void HasDependency::put( ostream& os ) const {
  Constraint::put( os );
  os << " rel=" << relType << " head=" << headType;
}
void DependencyDirection::put( ostream & os ) const {
  Constraint::put( os );
  os << " direct=" << " " << direction;
}


CKYParser::CKYParser( size_t num ): numTokens(num)
{
  inDepConstraints.resize( numTokens + 1 );
  outDepConstraints.resize( numTokens + 1 );
  edgeConstraints.resize( numTokens + 1 );
  for ( auto& it : edgeConstraints ){
    it.resize( numTokens + 1 );
  }
}


void CKYParser::addConstraint( Constraint *c ){
  if ( c->type() == Constraint::Incoming ){
    inDepConstraints[c->tIndex()].push_back( c );
    cerr << "added INCOMING[" << c->tIndex() << "]" << endl;
  }
  else if ( c->type() ==  Constraint::Dependency ){
    edgeConstraints[c->tIndex()][c->hIndex()].push_back( c );
    cerr << "added DEPENDENCY[" << c->tIndex() << "," << c->hIndex() << "]" << endl;
  }
  else if ( c->type() == Constraint::Direction ){
    outDepConstraints[c->tIndex()].push_back( c );
    cerr << "added DIRECTION[" << c->tIndex() << "]" << endl;
  }
}

string CKYParser::bestEdge( SubTree& leftSubtree, SubTree& rightSubtree, 
			    size_t headIndex, size_t depIndex,
			    set<Constraint*>& bestConstraints,
			    double& bestScore ){
  bestConstraints.clear();
  if ( headIndex == 0 ){
    bestScore = 0.0;
    string label = "ROOT";
    for ( auto const& constraint : outDepConstraints[depIndex] ){
      if ( constraint->direct() == dirType::ROOT ){
	bestScore = constraint->wght();
	bestConstraints.insert( constraint );
      }
    }
    for ( auto const& constraint : edgeConstraints[depIndex][0] ){
      bestScore += constraint->wght();
      bestConstraints.insert( constraint );
      label = constraint->rel();
    }
    return label;
  }
  bestScore = -0.5;
  string bestLabel = "None";
  for( auto const& edgeConstraint : edgeConstraints[depIndex][headIndex] ){
    double my_score = edgeConstraint->wght();
    string my_label = edgeConstraint->rel();
    cerr << "MY score" << my_score << endl;
    set<Constraint *> my_constraints;
    my_constraints.insert( edgeConstraint );
    for( const auto& constraint : inDepConstraints[headIndex] ){
      if ( constraint->rel() == my_label &&
	   leftSubtree.satisfiedConstraints.find( constraint ) == leftSubtree.satisfiedConstraints.end() &&
	   rightSubtree.satisfiedConstraints.find( constraint ) == rightSubtree.satisfiedConstraints.end() ){
	my_score += constraint->wght();
	cerr << "add MY score" << my_score << endl;
	my_constraints.insert(constraint);
      }
    }
    for( const auto& constraint : outDepConstraints[depIndex] ){
      if ( ( ( constraint->direct() == LEFT &&
	       headIndex < depIndex )
	     ||
	     ( constraint->direct() == RIGHT &&
	       headIndex > depIndex ) )
	   && leftSubtree.satisfiedConstraints.find( constraint ) == leftSubtree.satisfiedConstraints.end() 
	   && rightSubtree.satisfiedConstraints.find( constraint ) == rightSubtree.satisfiedConstraints.end() ){
	my_score += constraint->wght();
	cerr << "add more MY score" << my_score << endl;
	my_constraints.insert(constraint);
      }
    }
    if ( my_score > bestScore ){
      bestScore = my_score;
      bestLabel = my_label;
      bestConstraints = my_constraints;
    }
  }
  return bestLabel;
}

void CKYParser::parse(){
  vector<vector<map<string,map<bool,SubTree>>>> C;
  C.resize( numTokens +1 );
  for ( auto& it : C ){
    it.resize( numTokens + 1 );
    for ( auto& it2 : it ){
      it2["r"][true] = SubTree(0.0, -1, "" );
      it2["r"][false] = SubTree(0.0, -1, "" );
      it2["l"][true] = SubTree(0.0, -1, "" );
      it2["l"][false] = SubTree(0.0, -1, "" );
    }
  }
  for ( size_t k=1; k < numTokens + 2; ++k ){
    for( size_t s=0; s < numTokens +1 - k; ++s ){
      size_t t = s + k;
      int bestI = -1;
      string bestT = "__";
      set<Constraint*> bestConstraints;
      for( size_t r = s; r < t; ++r ){
	double score = -0.5;
	string label = bestEdge( C[s][r]["r"][true],
				 C[r+1][t]["l"][true],
				 t, s, bestConstraints, score );
	cerr << "BEST EDGE==> " << label << " ( " << score << ")" << endl;
      }
    }
  }
}
		
	// 	for k in xrange(1, self.numTokens + 1 + 1):
	// 		for s in xrange(self.numTokens - k + 1):
	// 			t = s + k

	// 			best = -1, "__" #####None
	// 			bestScore = float("-inf") #-1
	// 			bestConstraints = None
	// 			for r in xrange(s, t):
	// 				label, edgeScore, constraints = self.bestEdge(
	// 					C[s, r, "r", True],
	// 					C[r + 1, t, "l", True],
	// 					t, s)

	// 				score = C[s, r, "r", True].score + \
	// 						C[r + 1, t, "l", True].score + \
	// 						edgeScore
					
	// 				if score > bestScore:
	// 					bestScore = score
	// 					best = r, label
	// 					bestConstraints = constraints

	// 			C[s, t, "l", False] = SubTree(bestScore, *best)
	// 			C[s, t, "l", False].satisfiedConstraints.update(
	// 				C[s, best[0], "r", True].satisfiedConstraints)
	// 			C[s, t, "l", False].satisfiedConstraints.update(
	// 				C[best[0] + 1, t, "l", True].satisfiedConstraints)
	// 			C[s, t, "l", False].satisfiedConstraints.update(
	// 				bestConstraints)
				

	// 			best = -1, "__" #######None
	// 			bestScore = float("-inf") #-1
	// 			bestConstraints = None
	// 			for r in xrange(s, t):
	// 				label, edgeScore, constraints = self.bestEdge(
	// 					C[s, r, "r", True],
	// 					C[r + 1, t, "l", True],
	// 					s, t)
					
	// 				score = C[s, r, "r", True].score + \
	// 						C[r + 1, t, "l", True].score + \
	// 						edgeScore
					
	// 				if score > bestScore:
	// 					bestScore = score
	// 					best = r, label
	// 					bestConstraints = constraints

	// 			C[s, t, "r", False] = SubTree(bestScore, *best)
	// 			C[s, t, "r", False].satisfiedConstraints.update(
	// 				C[s, best[0], "r", True].satisfiedConstraints)
	// 			C[s, t, "r", False].satisfiedConstraints.update(
	// 				C[best[0] + 1, t, "l", True].satisfiedConstraints)
	// 			C[s, t, "r", False].satisfiedConstraints.update(
	// 				bestConstraints)
				

	// 			best = -1 ######None
	// 			bestScore = float("-inf") #-1
	// 			for r in xrange(s, t):
	// 				score = C[s, r, "l", True].score + \
	// 						C[r, t, "l", False].score
	// 				if score > bestScore:
	// 					bestScore = score
	// 					best = r

	// 			C[s, t, "l", True] = SubTree(bestScore, best, None)
	// 			C[s, t, "l", True].satisfiedConstraints.update(
	// 				C[s, best, "l", True].satisfiedConstraints)
	// 			C[s, t, "l", True].satisfiedConstraints.update(
	// 				C[best, t, "l", False].satisfiedConstraints)
				

	// 			best = -1 ####None
	// 			bestScore = float("-inf") #-1
	// 			for r in xrange(s + 1, t + 1):
	// 				score = C[s, r, "r", False].score + \
	// 						C[r, t, "r", True].score
	// 				if score > bestScore:
	// 					bestScore = score
	// 					best = r

	// 			C[s, t, "r", True] = SubTree(bestScore, best, None)
	// 			C[s, t, "r", True].satisfiedConstraints.update(
	// 				C[s, best, "r", False].satisfiedConstraints)
	// 			C[s, t, "r", True].satisfiedConstraints.update(
	// 				C[best, t, "r", True].satisfiedConstraints)
				

	// 	return C
