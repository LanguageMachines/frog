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
		    istream& dirs, istream& rels,
		    istream& pairs,
		    vector<Constraint*>& constraints,
		    int maxDist ){
  constraints.clear();
  for ( const auto& sentence : sentences ){
    vector<string> dependents;
    TiCC::split( sentence, dependents );
    int dependent_id = TiCC::stringTo<int>( dependents[0] );
    int headId = 0;
    //    cerr << "sentence: " << sentence << " ID=" << dependent_id << endl;

    string line;
    getline( pairs, line );
    vector<string> timbl_result;
    TiCC::split_at_first_of( line, timbl_result, "{}" );
    // using TiCC::operator<<;
    // for ( const auto& hm : timbl_result ){
    //   cerr << "timbl_resultf = " << hm << endl;
    // }
    string instance = timbl_result[0];
    string distribution = timbl_result[1];
    string distance = timbl_result[2];
    // cerr << "instance: " << instance << endl;
    // cerr << "distribution: " << distribution << endl;
    // cerr << "distance: " << distance << endl;
    string top_class = get_class( instance );
    map<string,double> dist;
    split_dist( distribution, dist );
    double conf = dist[top_class];
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
	if ( !getline( pairs, line ) ){
	  cerr << "OEPS pairs leeg? " << endl;
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
	map<string,double> dist;
	split_dist( distribution, dist );
	double conf = dist[top_class];
	//	cerr << "class=" << top_class << " met conf " << conf << endl;
	if ( top_class != "__" ){
	  constraints.push_back( new HasDependency(dependent_id,headId,top_class,conf));
	}
      }
    }
  }
  for ( const auto& head : sentences ){
    vector<string> tokens;
    TiCC::split( head, tokens );
    int token_id = TiCC::stringTo<int>( tokens[0] );
    string line;
    if ( !getline( dirs, line ) ){
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
    map<string,double> dist;
    split_dist( distribution, dist );
    for( const auto& d : dist ){
      constraints.push_back( new DependencyDirection( token_id, d.first, d.second ) );
    }

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

void parse( const string& pair_file, const string& rel_file,
	    const string& dir_file, int maxDist,
	    const string& in_file, const string& out_file ){
  ifstream pairs( pair_file );
  //  cerr << "opened pairs: " << pair_file << endl;
  ifstream rels( rel_file );
  //  cerr << "opened rels: " << rel_file << endl;
  ifstream dirs( dir_file );
  //  cerr << "opened dir: " << dir_file << endl;
  ifstream is( in_file );
  //  cerr << "opened inputfile: " << in_file << endl;
  string line;
  vector<string> sentences;
  while( getline( is, line ) ){
    sentences.push_back( line );
  }
  vector<Constraint*> constraints;
  formulateWCSP( sentences, dirs, rels, pairs, constraints, maxDist );
  CKYParser parser( sentences.size() );
  for ( const auto& constraint : constraints ){
    parser.addConstraint( constraint );
  }
  parser.parse();
  vector<parsrel> result( sentences.size() );
  parser.rightComplete(0, sentences.size(), result );
  ofstream os( out_file );
  size_t i = 1;
  for ( const auto& it : result ){
    os << i << " . . . . . " << it.head << " " << it.deprel << endl;
    ++i;
  }
}
