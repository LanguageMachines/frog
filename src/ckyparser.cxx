#include <iostream>
#include <vector>
#include <map>
#include <string>

#include "ticcutils/PrettyPrint.h"
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
  using TiCC::operator<<;
  bestConstraints.clear();
  cerr << "BESTEDGE " << headIndex << " <> " << depIndex << endl;
  if ( headIndex == 0 ){
    bestScore = 0.0;
    for ( auto const& constraint : outDepConstraints[depIndex] ){
      cerr << "CHECK " << constraint << endl;
      if ( constraint->direct() == dirType::ROOT ){
	cerr << "head outdep matched " << constraint << endl;
	bestScore = constraint->wght();
	bestConstraints.insert( constraint );
      }
    }
    string label = "ROOT";
    for ( auto const& constraint : edgeConstraints[depIndex][0] ){
      cerr << "head edge matched " << constraint << endl;
      bestScore += constraint->wght();
      bestConstraints.insert( constraint );
      label = constraint->rel();
    }
    cerr << "best HEAD==>" << label << " " << bestScore << " " << bestConstraints << endl;
    return label;
  }
  bestScore = -0.5;
  string bestLabel = "None";
  for( auto const& edgeConstraint : edgeConstraints[depIndex][headIndex] ){
    double my_score = edgeConstraint->wght();
    string my_label = edgeConstraint->rel();
    set<Constraint *> my_constraints;
    my_constraints.insert( edgeConstraint );
    for( const auto& constraint : inDepConstraints[headIndex] ){
      if ( constraint->rel() == my_label &&
	   leftSubtree.satisfiedConstraints.find( constraint ) == leftSubtree.satisfiedConstraints.end() &&
	   rightSubtree.satisfiedConstraints.find( constraint ) == rightSubtree.satisfiedConstraints.end() ){
	cerr << "inDep matched: " << constraint << endl;
	my_score += constraint->wght();
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
	cerr << "outdep matched: " << constraint << endl;
	my_score += constraint->wght();
	my_constraints.insert(constraint);
      }
    }
    if ( my_score > bestScore ){
      bestScore = my_score;
      bestLabel = my_label;
      bestConstraints = my_constraints;
      cerr << "UPDATE BEst " << bestLabel << " " << bestScore << " " << bestConstraints << endl;
    }
  }
  cerr << "GRAND TOTAL " << bestLabel << " " << bestScore << " " << bestConstraints << endl;
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
      double bestScore = -10E45;
      int bestI = -1;
      string bestL = "__";
      set<Constraint*> bestConstraints;
      for( size_t r = s; r < t; ++r ){
	double edgeScore = -0.5;
	set<Constraint*> constraints;
	string label = bestEdge( C[s][r]["r"][true],
				 C[r+1][t]["l"][true],
				 t, s, constraints, edgeScore );
	cerr << "STEP 1 BEST EDGE==> " << label << " ( " << edgeScore << ")" << endl;
	double score = C[s][r]["r"][true].score() + C[r+1][t]["l"][true].score() + edgeScore;
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	  bestL = label;
	  bestConstraints = constraints;
	}
      }
      cerr << "STEP 1 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      C[s][t]["l"][false] = SubTree( bestScore, bestI, bestL );
      C[s][t]["l"][false].satisfiedConstraints.insert( C[s][bestI]["r"][true].satisfiedConstraints.begin(), C[s][bestI]["r"][true].satisfiedConstraints.end() );
      C[s][t]["l"][false].satisfiedConstraints.insert( C[bestI+1][t]["l"][true].satisfiedConstraints.begin(), C[bestI+1][t]["l"][true].satisfiedConstraints.end() );
      C[s][t]["l"][false].satisfiedConstraints.insert( bestConstraints.begin(), bestConstraints.end() );

      bestScore = -10E45;
      bestI = -1;
      bestL = "__";
      bestConstraints.clear();
      for ( size_t r = s; r < t; ++r ){
	double edgeScore = -0.5;
	set<Constraint*> constraints;
	string label = bestEdge( C[s][r]["r"][true],
				 C[r+1][t]["l"][true],
				 s, t, constraints, edgeScore );
	cerr << "STEP 2 BEST EDGE==> " << label << " ( " << edgeScore << ")" << endl;
	double score = C[s][r]["r"][true].score() + C[r+1][t]["l"][true].score() + edgeScore;
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	  bestL = label;
	  bestConstraints = constraints;
	}
      }

      cerr << "STEP 2 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      C[s][t]["r"][false] = SubTree( bestScore, bestI, bestL );
      C[s][t]["r"][false].satisfiedConstraints.insert( C[s][bestI]["r"][true].satisfiedConstraints.begin(), C[s][bestI]["r"][true].satisfiedConstraints.end() );
      C[s][t]["r"][false].satisfiedConstraints.insert( C[bestI+1][t]["l"][true].satisfiedConstraints.begin(), C[bestI+1][t]["l"][true].satisfiedConstraints.end() );
      C[s][t]["r"][false].satisfiedConstraints.insert( bestConstraints.begin(), bestConstraints.end() );

      bestI = -1;
      bestL = "NONE";
      bestScore = -10E45;
      for ( size_t r = s; r < t; ++r ){
	double score = C[s][r]["l"][true].score() + C[r][t]["l"][false].score();
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	}
      }
      cerr << "STEP 3 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      C[s][t]["l"][true] = SubTree( bestScore, bestI, bestL );
      C[s][t]["l"][true].satisfiedConstraints.insert( C[s][bestI]["l"][true].satisfiedConstraints.begin(), C[s][bestI]["l"][true].satisfiedConstraints.end() );
      C[s][t]["l"][true].satisfiedConstraints.insert( C[bestI][t]["l"][false].satisfiedConstraints.begin(), C[bestI][t]["l"][false].satisfiedConstraints.end() );

      bestI = -1;
      bestL = "NONE";
      bestScore = -10E45;
      for ( size_t r = s+1; r < t+1; ++r ){
	double score = C[s][r]["r"][false].score() + C[r][t]["r"][true].score();
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	}
      }

      cerr << "STEP 4 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      C[s][t]["r"][true] = SubTree( bestScore, bestI, bestL );
      C[s][t]["r"][true].satisfiedConstraints.insert( C[s][bestI]["r"][false].satisfiedConstraints.begin(), C[s][bestI]["r"][false].satisfiedConstraints.end() );
      C[s][t]["r"][true].satisfiedConstraints.insert( C[bestI][t]["r"][true].satisfiedConstraints.begin(), C[bestI][t]["r"][true].satisfiedConstraints.end() );


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

	// 				score = C[s, r, "r", True].score +
	// 						C[r + 1, t, "l", True].score +
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

	// 				score = C[s, r, "r", True].score +
	// 						C[r + 1, t, "l", True].score +
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
	// 				score = C[s, r, "l", True].score +
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
	// 				score = C[s, r, "r", False].score +
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
