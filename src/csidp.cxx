/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2018
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

#define LOG *TiCC::Log(log)
#define DBG *TiCC::Dbg(log)

unordered_map<string,double> split_dist( const vector< pair<string,double>>& dist ){
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
					 TiCC::LogStream *log ){
  vector<const Constraint*> constraints;
  vector<timbl_result>::const_iterator pit = p_res.begin();
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

  for ( size_t dependent_id = 1;
	dependent_id <= sent_len;
	++dependent_id ) {
    for ( size_t headId = 1;
	  headId <= sent_len;
	  ++headId ){
      size_t diff = ( headId > dependent_id ) ? headId - dependent_id : dependent_id - headId;
      if ( diff != 0 && diff <= maxDist ){
	string line;
	if ( pit == p_res.end() ){
	  LOG << "OEPS p_res leeg? " << endl;
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

vector<parsrel> parse( const vector<timbl_result>& p_res,
		       const vector<timbl_result>& r_res,
		       const vector<timbl_result>& d_res,
		       size_t parse_size,
		       int maxDist,
		       TiCC::LogStream *log ){
  vector<const Constraint*> constraints
    = formulateWCSP( d_res, r_res, p_res, parse_size, maxDist, log );
  CKYParser parser( parse_size, constraints, log );
  parser.parse();
  vector<parsrel> result( parse_size );
  parser.rightComplete(0, parse_size, result );
  for ( const auto& constraint : constraints ){
    delete constraint;
  }
  return result;
}
