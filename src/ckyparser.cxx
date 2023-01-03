/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2023
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of frog:

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for
  several languages

  frog is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  frog is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/frog/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/

#include "frog/ckyparser.h"

#include <iostream>
#include <vector>
#include <string>

#include "ticcutils/PrettyPrint.h"
#include "ticcutils/LogStream.h"

using namespace std;

#define LOG *TiCC::Log(ckyLog)
#define DBG *TiCC::Dbg(ckyLog)

ostream& operator<<( ostream& os, const Constraint* c ){
  /// output a Constraint (debug only)
  if ( c ){
    c->put( os );
  }
  else {
    os << "None";
  }
  return os;
}

ostream& operator<<( ostream& os, const Constraint& c ){
  /// output a Constraint (debug only)
  return os << &c;
}

using TiCC::operator<<;

void HasIncomingRel::put( ostream& os ) const {
  /// output a HasIncomingRel (debug only)
  Constraint::put( os );
  os << " incoming rel=" << relType;
}


void HasDependency::put( ostream& os ) const {
  /// output a HasDependency (debug only)
  Constraint::put( os );
  os << " dependency rel=" << relType << " head=" << headType;
}

void DependencyDirection::put( ostream & os ) const {
  /// output a DependencyDirection (debug only)
  Constraint::put( os );
  os << " direction=" << " " << direction;
}


CKYParser::CKYParser( size_t num,
		      const vector<const Constraint*>& constraints,
		      TiCC::LogStream* log ):
  numTokens(num)
{
  /// initalialize a CKYparser
  /*!
    \param num The number of tokens to parse
    \param constraints A Constraints vector
    \param log a LogStream for (debug) messages.
   */
  inDepConstraints.resize( numTokens + 1 );  // 1 dimensional array
  outDepConstraints.resize( numTokens + 1 ); // 1 dimensional array
  edgeConstraints.resize( numTokens + 1 );   // 2 dimensional array
  for ( auto& it : edgeConstraints ){
    it.resize( numTokens + 1 ); // second dimension
  }
  chart.resize( numTokens +1 ); // 2 dimensional array
  for ( auto& it : chart ){
    it.resize( numTokens + 1 ); // second dimension
  }
  for ( const auto& constraint : constraints ){
    addConstraint( constraint );
  }
  ckyLog = new TiCC::LogStream( log, "cky:" );
}


void CKYParser::addConstraint( const Constraint *c ){
  /// add a Constraint to our parser
  /*!
    \param c the Constraint to add.

    Depending on the Constraint Type we add \e c to one of our stacks
   */
  switch ( c->type() ){
  case Constraint::Incoming:
    inDepConstraints[c->tIndex()].push_back( c );
    break;
  case Constraint::Dependency:
    edgeConstraints[c->tIndex()][c->hIndex()].push_back( c );
    break;
  case Constraint::Direction:
    outDepConstraints[c->tIndex()].push_back( c );
    break;
  default:
    LOG << "UNSUPPORTED constraint type" << endl;
    abort();
  }
}

string CKYParser::bestEdge( const SubTree& leftSubtree,
			    const SubTree& rightSubtree,
			    size_t headIndex,
			    size_t depIndex,
			    set<const Constraint*>& bestConstraints,
			    double& bestScore ){
  /// search the best edge
  /// I dare not to comment....
  bestConstraints.clear();
  DBG << "BESTEDGE " << headIndex << " <> " << depIndex << endl;
  if ( headIndex == 0 ){
    bestScore = 0.0;
    for ( auto const& constraint : outDepConstraints[depIndex] ){
      DBG << "CHECK " << constraint << endl;
      if ( constraint->direct() == dirType::ROOT ){
	DBG << "head outdep matched " << constraint << endl;
	bestScore = constraint->wght();
	bestConstraints.insert( constraint );
      }
    }
    string label = "ROOT";
    for ( auto const& constraint : edgeConstraints[depIndex][0] ){
      DBG << "head edge matched " << constraint << endl;
      bestScore += constraint->wght();
      bestConstraints.insert( constraint );
      label = constraint->rel();
    }
    DBG << "best HEAD==>" << label << " " << bestScore << " " << bestConstraints << endl;
    return label;
  }
  bestScore = -0.5;
  string bestLabel = "None";
  for( auto const& edgeConstraint : edgeConstraints[depIndex][headIndex] ){
    double my_score = edgeConstraint->wght();
    string my_label = edgeConstraint->rel();
    set<const Constraint *> my_constraints;
    my_constraints.insert( edgeConstraint );
    for( const auto& constraint : inDepConstraints[headIndex] ){
      if ( constraint->rel() == my_label &&
	   leftSubtree.satisfiedConstraints.find( constraint ) == leftSubtree.satisfiedConstraints.end() &&
	   rightSubtree.satisfiedConstraints.find( constraint ) == rightSubtree.satisfiedConstraints.end() ){
	DBG << "inDep matched: " << constraint << endl;
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
	DBG << "outdep matched: " << constraint << endl;
	my_score += constraint->wght();
	my_constraints.insert(constraint);
      }
    }
    if ( my_score > bestScore ){
      bestScore = my_score;
      bestLabel = my_label;
      bestConstraints = std::move(my_constraints);
      DBG << "UPDATE BEst " << bestLabel << " " << bestScore << " " << bestConstraints << endl;
    }
  }
  DBG << "GRAND TOTAL " << bestLabel << " " << bestScore << " " << bestConstraints << endl;
  return bestLabel;
}

void CKYParser::parse(){
  /// run the parser
  /// I dare not to comment
  for ( size_t k=1; k < numTokens + 2; ++k ){
    for( size_t s=0; s < numTokens + 1 - k; ++s ){
      size_t t = s + k;
      double bestScore = -10E45;
      int bestI = -1;
      string bestL = "__";
      set<const Constraint*> bestConstraints;
      for( size_t r = s; r < t; ++r ){
	double edgeScore = -0.5;
	set<const Constraint*> constraints;
	string label = bestEdge( chart[s][r].r_True,
				 chart[r+1][t].l_True,
				 t, s, constraints, edgeScore );
	DBG << "STEP 1 BEST EDGE==> " << label << " ( " << edgeScore << ")" << endl;
	double score = chart[s][r].r_True.score() + chart[r+1][t].l_True.score() + edgeScore;
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	  bestL = label;
	  bestConstraints = std::move(constraints);
	}
      }
      DBG << "STEP 1 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      chart[s][t].l_False = SubTree( bestScore, bestI, bestL );
      chart[s][t].l_False.satisfiedConstraints.insert( chart[s][bestI].r_True.satisfiedConstraints.begin(), chart[s][bestI].r_True.satisfiedConstraints.end() );
      chart[s][t].l_False.satisfiedConstraints.insert( chart[bestI+1][t].l_True.satisfiedConstraints.begin(), chart[bestI+1][t].l_True.satisfiedConstraints.end() );
      chart[s][t].l_False.satisfiedConstraints.insert( bestConstraints.begin(), bestConstraints.end() );

      bestScore = -10E45;
      bestI = -1;
      bestL = "__";
      bestConstraints.clear();
      for ( size_t r = s; r < t; ++r ){
	double edgeScore = -0.5;
	set<const Constraint*> constraints;
	string label = bestEdge( chart[s][r].r_True,
				 chart[r+1][t].l_True,
				 s, t, constraints, edgeScore );
	DBG << "STEP 2 BEST EDGE==> " << label << " ( " << edgeScore << ")" << endl;
	double score = chart[s][r].r_True.score() + chart[r+1][t].l_True.score() + edgeScore;
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	  bestL = label;
	  bestConstraints = std::move(constraints);
	}
      }

      DBG << "STEP 2 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      chart[s][t].r_False = SubTree( bestScore, bestI, bestL );
      chart[s][t].r_False.satisfiedConstraints.insert( chart[s][bestI].r_True.satisfiedConstraints.begin(), chart[s][bestI].r_True.satisfiedConstraints.end() );
      chart[s][t].r_False.satisfiedConstraints.insert( chart[bestI+1][t].l_True.satisfiedConstraints.begin(), chart[bestI+1][t].l_True.satisfiedConstraints.end() );
      chart[s][t].r_False.satisfiedConstraints.insert( bestConstraints.begin(), bestConstraints.end() );

      bestI = -1;
      bestL = "";
      bestScore = -10E45;
      for ( size_t r = s; r < t; ++r ){
	double score = chart[s][r].l_True.score() + chart[r][t].l_False.score();
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	}
      }
      DBG << "STEP 3 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      chart[s][t].l_True = SubTree( bestScore, bestI, bestL );
      chart[s][t].l_True.satisfiedConstraints.insert( chart[s][bestI].l_True.satisfiedConstraints.begin(), chart[s][bestI].l_True.satisfiedConstraints.end() );
      chart[s][t].l_True.satisfiedConstraints.insert( chart[bestI][t].l_False.satisfiedConstraints.begin(), chart[bestI][t].l_False.satisfiedConstraints.end() );

      bestI = -1;
      bestL = "";
      bestScore = -10E45;
      for ( size_t r = s+1; r < t+1; ++r ){
	double score = chart[s][r].r_False.score() + chart[r][t].r_True.score();
	if ( score > bestScore ){
	  bestScore = score;
	  bestI = r;
	}
      }

      DBG << "STEP 4 ADD: " << bestScore <<"-" << bestI << "-" << bestL << endl;
      chart[s][t].r_True = SubTree( bestScore, bestI, bestL );
      chart[s][t].r_True.satisfiedConstraints.insert( chart[s][bestI].r_False.satisfiedConstraints.begin(), chart[s][bestI].r_False.satisfiedConstraints.end() );
      chart[s][t].r_True.satisfiedConstraints.insert( chart[bestI][t].r_True.satisfiedConstraints.begin(), chart[bestI][t].r_True.satisfiedConstraints.end() );

    }
  }
}

void CKYParser::leftIncomplete( int s, int t, vector<parsrel>& pr ){
  int r = chart[s][t].l_False.r();
  string label = chart[s][t].l_False.edgeLabel();
  if ( r >=0 ){
    pr[s - 1].deprel = label;
    pr[s - 1].head = t;
    rightComplete( s, r, pr );
    leftComplete( r + 1, t, pr );
  }
}

void CKYParser::rightIncomplete( int s, int t, vector<parsrel>& pr ){
  int r = chart[s][t].r_False.r();
  string label = chart[s][t].r_False.edgeLabel();
  if ( r >= 0 ) {
    pr[t - 1].deprel = label;
    pr[t - 1].head = s;
    rightComplete( s, r, pr );
    leftComplete( r + 1, t, pr );
  }
}


void CKYParser::leftComplete( int s, int t, vector<parsrel>& pr ){
  int r = chart[s][t].l_True.r();
  if ( r >= 0 ){
    leftComplete( s, r, pr );
    leftIncomplete( r, t, pr );
  }
}

void CKYParser::rightComplete( int s, int t, vector<parsrel>& pr ){
  int r = chart[s][t].r_True.r();
  if ( r >= 0 ){
    rightIncomplete( s, r, pr );
    rightComplete( r, t, pr );
  }
}
