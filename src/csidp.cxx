/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2021
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

#include "frog/csidp.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/LogStream.h"


using namespace std;
using TiCC::operator<<;

#define LOG *TiCC::Log(dbg_log)
#define DBG *TiCC::Dbg(dbg_log)

unordered_map<string,double> split_dist( const vector< pair<string,double>>& dist ){
  /// split a vector of ambi-tags to double pairs into a map of tag[double]
  /*!
    \param dist a vector of <string,double> pairs, where each string is a list
    of '|' separated tags.
    \return a mapping from tags to doubles

    so for instance when the input is like {<"a|b",0.5>,<"c|d|e",0.6>}
    the result would be { {"a",0.5},{"b",0.5},{"c",0.6},{"d",0.6},{"e",0.6}}
   */
  unordered_map<string,double> result;
  for( const auto& it : dist ){
    double d = it.second;
    vector<string> tags = TiCC::split_at( it.first, "|" );
    for( const auto& t : tags ){
      result[t] += d;
    }
  }
  return result;
}

vector<const Constraint*> formulateWCSP( const vector<timbl_result>& d_res,
					 const vector<timbl_result>& r_res,
					 const vector<timbl_result>& p_res,
					 size_t sent_len,
					 size_t maxDist,
					 TiCC::LogStream *dbg_log ){
  /// create a list of Parse Constraint records based on the 3 Timbl outputs
  /*!
    \param d_res results of the Timbl dist classifier
    \param r_res results of the Timbl relations classifier
    \param p_res results of the Timbl pairs classifier
    \param sent_len the sentence length
    \param maxDist the maximum distance we still handle
    \param dbg_log a LogStream for debugging
   */
  vector<const Constraint*> constraints;
  vector<timbl_result>::const_iterator pit = p_res.begin();
  //  LOG << "formulate WSCP, step 1" << endl;
  for ( size_t dependent_id = 1;
	dependent_id <= sent_len;
	++dependent_id ){
    string top_class = pit->cls();
    double conf = pit->confidence();
    ++pit;
    DBG << "class=" << top_class << " met conf " << conf << endl;
    if ( top_class != "__" ){
      constraints.push_back( new HasDependency( dependent_id, 0 ,top_class, conf ) );
    }
  }

  //  LOG << "formulate WSCP, step 2" << endl;
  for ( size_t dependent_id = 1;
	dependent_id <= sent_len;
	++dependent_id ) {
    for ( size_t headId = 1;
	  headId <= sent_len;
	  ++headId ){
      size_t diff = ( headId > dependent_id ) ? headId - dependent_id : dependent_id - headId;
      if ( diff != 0 && diff <= maxDist ){
	if ( pit == p_res.end() ){
	  DBG << "OEPS p_res leeg? " << endl;
	  break;
	}
	string top_class = pit->cls();
	double conf = pit->confidence();
	++pit;
	DBG << "class=" << top_class << " met conf " << conf << endl;
	if ( top_class != "__" ){
	  constraints.push_back( new HasDependency(dependent_id,headId,top_class,conf));
	}
      }
    }
  }

  //  LOG << "formulate WSCP, step 3" << endl;
  vector<timbl_result>::const_iterator dit = d_res.begin();
  vector<timbl_result>::const_iterator rit = r_res.begin();
  for ( size_t token_id = 1;
	token_id <= sent_len;
	++token_id ) {
    for ( auto const& d : dit->dist() ){
      constraints.push_back( new DependencyDirection( token_id, d.first, d.second ) );
    }
    ++dit;

    for ( size_t rel_id = 1;
	  rel_id <= sent_len;
	  ++rel_id ) {
      if ( rit == r_res.end() ){
	break;
      }
      string top_class = rit->cls();
      if ( top_class != "__" ){
	unordered_map<string,double> splits = split_dist( rit->dist() );
	vector<string> clss = TiCC::split_at( top_class, "|" );
	for( const auto& rel : clss ){
	  constraints.push_back( new HasIncomingRel( rel_id, rel, splits[rel] ) );
	}
      }
      ++rit;
    }
  }
  //  LOG << "formulate WSCP, Done" << endl;
  return constraints;
}

timbl_result::timbl_result( const string& cls,
			    double conf,
			    const Timbl::ValueDistribution* vd ):
  _cls(cls), _confidence(conf) {
  Timbl::ValueDistribution::dist_iterator it = vd->begin();
  while ( it != vd->end() ){
    _dist.push_back( make_pair(it->second->Value()->Name(),it->second->Weight()) );
    ++it;
  }
}

timbl_result::timbl_result( const string& cls,
			    double conf,
			    const vector<std::pair<string,double>>& vd ):
  _cls(cls), _confidence(conf), _dist(vd) {
}

vector<parsrel> parse( const vector<timbl_result>& p_res,
		       const vector<timbl_result>& r_res,
		       const vector<timbl_result>& d_res,
		       size_t sent_len,
		       int maxDist,
		       TiCC::LogStream *dbg_log ){
  /// run de CKY parser using these data
  /*!
    \param p_res the Timbl pairs outcome
    \param r_res the Timbl rels outcome
    \param d_res the Timbl dir outcome
    \param sent_len the maximum sentence lenght
    \param maxDist the maximum distance between dependents we allow
    \param dbg_log the stream used for debugging
    \return a vector of parsrel structures
  */
  vector<const Constraint*> constraints
    = formulateWCSP( d_res, r_res, p_res, sent_len, maxDist, dbg_log );
  DBG << "constraints: " << endl;
  DBG << constraints << endl;
  CKYParser parser( sent_len, constraints, dbg_log );
  parser.parse();
  vector<parsrel> result( sent_len );
  parser.rightComplete(0, sent_len, result );
  for ( const auto& constraint : constraints ){
    delete constraint;
  }
  return result;
}
