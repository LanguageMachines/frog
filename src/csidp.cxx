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

void formulateWCSP( const vector<string>& sentences,
		    istream& dirs, istream& rels,
		    istream& pairs ) {
  multimap<int,pair<int,string>> domains;
  vector<string> constraints;
  for ( const auto& sentence : sentences ){
    vector<string> dependents;
    TiCC::split( sentence, dependents );
    int dependent_id = TiCC::stringTo<int>( dependents[0] );
    int headId = 0;
    cerr << "sentence: " << sentence << " ID=" << dependent_id << endl;
    
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
    cerr << "instance: " << instance << endl;
    cerr << "distribution: " << distribution << endl;
    cerr << "distance: " << distance << endl;
    string top_class = get_class( instance );
    map<string,double> dist;
    split_dist( distribution, dist );
    double conf = dist[top_class];
    cerr << "class=" << top_class << " met conf " << conf << endl;
    if ( top_class != "__" ){
      cerr << "ADD domain[" << dependent_id << "]=<" << headId << "," << top_class << ">" << endl;
      domains.insert( make_pair(dependent_id, make_pair(headId, top_class) ) );
    }
  }
  for ( const auto& sentence : sentences ){
    vector<string> dependents;
    TiCC::split( sentence, dependents );
    int dependent_id = TiCC::stringTo<int>( dependents[0] );
    cerr << "pair-sentence: " << sentence << " ID=" << dependent_id << endl;
    
    for ( const auto& head : sentences ){
      if ( head != sentence ){
	cerr << "pair-head: " << head << endl;
	vector<string> parts;
	TiCC::split( head, parts );
	int headId = TiCC::stringTo<int>( parts[0] );
	string line;
	getline( pairs, line );
	vector<string> timbl_result;
	TiCC::split_at_first_of( line, timbl_result, "{}" );
	string instance = timbl_result[0];
	string distribution = timbl_result[1];
	string distance = timbl_result[2];
	cerr << "instance: " << instance << endl;
	cerr << "distribution: " << distribution << endl;
	cerr << "distance: " << distance << endl;
	string top_class = get_class( instance );
	map<string,double> dist;
	split_dist( distribution, dist );
	double conf = dist[top_class];
	cerr << "class=" << top_class << " met conf " << conf << endl;
	if ( top_class != "__" ){
	  cerr << "ADD domain[" << dependent_id << "]=<" << headId << "," << top_class << ">" << endl;
	  domains.insert( make_pair( dependent_id, make_pair(headId, top_class)));
	}
      }
    }
  }
}

void parse( const string& pair_file, const string& rel_file,
	    const string& dir_file, int maxDist,
	    const string& in_file, const string& out_file ){
  ifstream pairs( pair_file );
  cerr << "opened pairs: " << pair_file << endl;
  ifstream rels( rel_file );
  cerr << "opened rels: " << rel_file << endl;
  ifstream dirs( dir_file );
  cerr << "opened dir: " << dir_file << endl;
  ifstream is( in_file );
  cerr << "opened inputfile: " << in_file << endl;
  string line;
  vector<string> sentences;
  while( getline( is, line ) ){
    sentences.push_back( line );
  }
  formulateWCSP( sentences, dirs, rels, pairs );
}

