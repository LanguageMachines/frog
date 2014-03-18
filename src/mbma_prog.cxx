/*
  $Id:$
  $URL:$

  Copyright (c) 2006 - 2014
  Tilburg University

  A Morphological-Analyzer for Dutch

  This file is part of frog

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
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/


#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

#include "config.h"
#include "timbl/TimblAPI.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "libfolia/folia.h"
#include "frog/ucto_tokenizer_mod.h"
#include "frog/cgn_tagger_mod.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace Timbl;

LogStream my_default_log( cerr, "", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals
int tpDebug = 0; //0 for none, more for more output

string TestFileName;
string ProgName;
bool doAll = false;

Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/";
static string configFileName = configDir + "frog.cfg";

static UctoTokenizer tokenizer;
static CGNTagger tagger;

void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -t <testfile>          Run mbma on this file\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version .   Show version info.\n"
       << "\t --daring=<true|false>. Default false\n"
       << "\t -d <debug level>    (for more verbosity)\n";
}

static Mbma myMbma;

bool parse_args( TimblOpts& Opts ) {
  string value;
  bool mood;
  if ( Opts.Find('V', value, mood ) ||
       Opts.Find("version", value, mood ) ){
    // we already did show what we wanted.
    exit( EXIT_SUCCESS );
  }
  if ( Opts.Find ('h', value, mood)) {
    usage();
    exit( EXIT_SUCCESS );
  };
  // is a config file specified?
  if ( Opts.Find( 'c',  value, mood ) ) {
    configFileName = value;
    Opts.Delete( 'c' );
  };

  if ( configuration.fill( configFileName ) ){
    cerr << "config read from: " << configFileName << endl;
  }
  else {
    cerr << "failed to read configuration from! '" << configFileName << "'" << endl;
    cerr << "did you correctly install the frogdata package?" << endl;
    return false;
  }

  // debug opts
  if ( Opts.Find ('d', value, mood)) {
    int debug = 0;
    if ( !TiCC::stringTo<int>( value, debug ) ){
      cerr << "-d value should be an integer" << endl;
      return false;
    }
    configuration.setatt( "debug", value, "mbma" );
    Opts.Delete('d');
  };

  if ( Opts.Find( 't', value, mood )) {
    TestFileName = value;
    ifstream is( value.c_str() );
    if ( !is ){
      cerr << "input stream " << value << " is not readable" << endl;
      return false;
    }
    Opts.Delete('t');
  };
  if ( Opts.Find( 'a', value, mood )) {
    doAll = true;
    Opts.Delete('a');
  };
  if ( Opts.Find( "daring", value, mood )) {
    if ( value.empty() )
      value = "1";
    configuration.setatt( "daring", value, "mbma" );
    Opts.Delete("daring");
  };
  return true;
}

bool init(){
  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  if ( !myMbma.init( configuration ) ){
    cerr << "MBMA Initialization failed." << endl;
    return false;
  }
  if ( !tokenizer.init( configuration, "", false ) ){
    cerr << "UCTO Initialization failed." << endl;
    return false;
  }
  if ( !tagger.init( configuration ) ){
    cerr << "CGN Initialization failed." << endl;
    return false;
  }
  cerr << "Initialization done." << endl;
  return true;
}

void Test( istream& in ){
  string line;
  while ( getline( in, line ) ){
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    cerr << "processing: " << line << endl;
    vector<string> sentences = tokenizer.tokenize( line );
    for ( size_t s=0; s < sentences.size(); ++s ){
      vector<TagResult> tagv = tagger.tagLine(sentences[s]);
      for ( size_t w=0; w < tagv.size(); ++w ){
	UnicodeString uWord = folia::UTF8ToUnicode(tagv[w].word());
	vector<string> v;
	size_t num = TiCC::split_at_first_of( tagv[w].assignedTag(),
					      v, "(,)" );
	if ( num < 1 ){
	  throw runtime_error( "error: tag not in right format " );
	}
	string head = v[0];
	if ( head != "SPEC" )
	  uWord.toLower();
	myMbma.Classify( uWord );
	if ( !doAll ){
	  v.erase(v.begin());
	  myMbma.filterTag( head, v );
	}
	vector<vector<string> > res = myMbma.getResult();
	cout << tagv[w].word() << " {" << tagv[w].assignedTag() << "}\t";
	if ( res.size() == 0 ){
	  cout << "[" << uWord << "]";
	}
	else {
	  for ( size_t i=0; i < res.size(); ++i ){
	    cout << res[i];
	    if ( i < res.size()-1 )
	      cout << "/";
	  }
	}
	cout << endl;
      }
      cout << "<utt>" << endl << endl;
    }
    cout << endl;
  }
  return;
}


int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  cerr << "mbma " << VERSION << " (c) ILK 1998 - 2014" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  ProgName = argv[0];
  cerr << "based on [" << Timbl::VersionName() << "]" << endl;
  cerr << "configdir: " << configDir << endl;
  try {
    TimblOpts Opts(argc, argv);
    if ( parse_args(Opts) ){
      if (  !init() ){
	cerr << "terminated." << endl;
	return EXIT_FAILURE;
      }
      ifstream in(TestFileName.c_str() );
      if ( in.good() ){
	cerr << "Processing: " << TestFileName << endl;
	Test( in );
      }
      else {
	cerr << "unable to open: " << TestFileName << endl;
	return EXIT_FAILURE;
      }
    }
    else {
      return EXIT_FAILURE;
    }
  }
  catch ( const exception& e ){
    cerr << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
