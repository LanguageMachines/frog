#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "frog/csidp.h"

using namespace std;

void split_dist( const vector< pair<string,double>>& dist,
		 map<string,double>& result ){
  result.clear();
  for( const auto& it : dist ){
    double d = it.second;
    vector<string> tags;
    TiCC::split_at( it.first, tags, "|" );
    for( const auto& t : tags ){
      result[t] += d;
    }
  }
}

vector<Constraint*> formulateWCSP( const vector<timbl_result>& d_res,
				   const vector<timbl_result>& r_res,
				   const vector<timbl_result>& p_res,
				   size_t sent_len,
				   size_t maxDist ){
  vector<Constraint*> constraints;
  vector<timbl_result>::const_iterator pit = p_res.begin();
  for ( size_t dependent_id = 1;
	dependent_id <= sent_len;
	++dependent_id ){
    int headId = 0;
    string top_class = pit->cls();
    double conf = pit->confidence();
    ++pit;
    //    cerr << "class=" << top_class << " met conf " << conf << endl;
    if ( top_class != "__" ){
      constraints.push_back(new HasDependency(dependent_id,headId,top_class,conf));
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
	  cerr << "OEPS p_res leeg? " << endl;
	  break;
	}
	string top_class = pit->cls();
	double conf = pit->confidence();
	++pit;
	//	cerr << "class=" << top_class << " met conf " << conf << endl;
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
	map<string,double> splits;
	split_dist( rit->dist(), splits );
	vector<string> clss;
	TiCC::split_at( top_class, clss, "|" );
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
		       int maxDist ){
  vector<Constraint*> constraints
    = formulateWCSP( d_res, r_res, p_res, parse_size, maxDist );
  CKYParser parser( parse_size );
  for ( const auto& constraint : constraints ){
    parser.addConstraint( constraint );
  }
  parser.parse();
  vector<parsrel> result( parse_size );
  parser.rightComplete(0, parse_size, result );
  return result;
}
