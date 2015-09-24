#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "frog/csidp.h"

using namespace std;

string get_class( const string& instance ){
  vector<string> classes;
  TiCC::split( instance, classes );
  return classes[classes.size()-1];
}


void split_dist( const string& distribution, map<string,double>& result ){
  result.clear();
  vector<string> dist_parts;
  TiCC::split_at( distribution, dist_parts, "," );
  for( const auto& it : dist_parts ){
    vector<string> parts;
    TiCC::split( it, parts );
    result[parts[0]] = TiCC::stringTo<double>( parts[1] );
  }
}

void split_dist( const string& distribution, multimap<string,double>& result ){
  result.clear();
  vector<string> dist_parts;
  TiCC::split_at( distribution, dist_parts, "," );
  for( const auto& it : dist_parts ){
    vector<string> parts;
    TiCC::split( it, parts );
    double d = TiCC::stringTo<double>( parts[1] );
    vector<string> tags;
    TiCC::split_at( parts[0], tags, "|" );
    for( const auto& t : tags ){
      result.insert( make_pair(t, d ) );
    }
  }
}

void formulateWCSP( const vector<string>& sentences,
		    const vector<timbl_result>& d_res,
		    istream& rels,
		    const vector<timbl_result>& r_res,
		    const vector<timbl_result>& p_res,
		    vector<Constraint*>& constraints,
		    int maxDist ){
  constraints.clear();
  vector<timbl_result>::const_iterator pit = p_res.begin();
  for ( const auto& sentence : sentences ){
    vector<string> dependents;
    TiCC::split( sentence, dependents );
    int dependent_id = TiCC::stringTo<int>( dependents[0] );
    int headId = 0;
    //    cerr << "sentence: " << sentence << " ID=" << dependent_id << endl;

    string top_class = pit->cls();
    double conf = pit->confidence();
    ++pit;
    //    cerr << "class=" << top_class << " met conf " << conf << endl;
    if ( top_class != "__" ){
      constraints.push_back(new HasDependency(dependent_id,headId,top_class,conf));
    }
  }
  for ( const auto& sentence : sentences ){
    vector<string> dependents;
    TiCC::split( sentence, dependents );
    int dependent_id = TiCC::stringTo<int>( dependents[0] );
    //    cerr << "pair-sentence: " << sentence << " ID=" << dependent_id << endl;

    for ( const auto& head : sentences ){
      if ( head != sentence ){
	vector<string> parts;
	TiCC::split( head, parts );
	int headId = TiCC::stringTo<int>( parts[0] );
	if ( abs(headId-dependent_id) > maxDist ){
	  continue;
	}
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
  for ( const auto& head : sentences ){
    vector<string> tokens;
    TiCC::split( head, tokens );
    int token_id = TiCC::stringTo<int>( tokens[0] );
    for ( auto const& d : dit->dist() ){
      constraints.push_back( new DependencyDirection( token_id, d.first, d.second ) );
    }
    ++dit;

    for ( const auto& token : sentences ){
      vector<string> tokens;
      TiCC::split( token, tokens );
      int token_id = TiCC::stringTo<int>( tokens[0] );
      string line;
      if ( !getline( rels, line ) ){
	break;
      }
      vector<string> timbl_result;
      TiCC::split_at_first_of( line, timbl_result, "{}" );
      string instance = timbl_result[0];
      string distribution = timbl_result[1];
      string distance = timbl_result[2];
      // cerr << "instance: " << instance << endl;
      // cerr << "distribution: " << distribution << endl;
      // cerr << "distance: " << distance << endl;
      string top_class = get_class( instance );
      if ( top_class != "__" ){
	multimap<string,double> mdist;
	split_dist( distribution, mdist );
	// using TiCC::operator<<;
	// cerr << "Multi dist = " << mdist << endl;
	vector<string> clss;
	TiCC::split_at( top_class, clss, "|" );
	for( const auto& rel : clss ){
	  double conf = 0.0;
	  for( const auto& d : mdist ){
	    if ( d.first == rel ){
	      conf += d.second;
	    }
	  }
	  constraints.push_back( new HasIncomingRel( token_id, rel, conf ) );
	}
      }
    }
  }
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

vector<parsrel> parse( const string& pair_file, 
		       const vector<timbl_result>& p_res,
		       const string& rel_file,
		       const vector<timbl_result>& r_res,
		       const string& dir_file, 
		       const vector<timbl_result>& d_res,
		       int maxDist,
		       const string& in_file ){
  ifstream rels( rel_file );
  //  cerr << "opened rels: " << rel_file << endl;
  ifstream is( in_file );
  //  cerr << "opened inputfile: " << in_file << endl;
  string line;
  vector<string> sentences;
  while( getline( is, line ) ){
    sentences.push_back( line );
  }
  vector<Constraint*> constraints;
  formulateWCSP( sentences, d_res, rels, r_res, p_res, constraints, maxDist );
  CKYParser parser( sentences.size() );
  for ( const auto& constraint : constraints ){
    parser.addConstraint( constraint );
  }
  parser.parse();
  vector<parsrel> result( sentences.size() );
  parser.rightComplete(0, sentences.size(), result );
  return result;
}
