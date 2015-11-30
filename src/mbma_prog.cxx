/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
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
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/foliautils.h"
#include "libfolia/folia.h"
#include "frog/ucto_tokenizer_mod.h"
#include "frog/cgn_tagger_mod.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace Timbl;

LogStream my_default_log( cerr, "", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals

vector<string> fileNames;
bool useTagger = true;
bool useTokenizer = true;

Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/";
static string configFileName = configDir + "frog.cfg";

static UctoTokenizer tokenizer(theErrLog);
static CGNTagger tagger(theErrLog);

void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -t <testfile>          Run mbma on this file\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version .   Show version info.\n"
       << "\t --daring . Deliver structured output. Default false\n"
       << "\t -d <debug level>    (for more verbosity)\n";
}

static Mbma myMbma(theErrLog);

bool parse_args( TiCC::CL_Options& Opts ) {
  if ( Opts.is_present( 'V' ) || Opts.is_present("version") ){
    // we already did show what we wanted.
    exit( EXIT_SUCCESS );
  }
  if ( Opts.is_present('h') ){
    usage();
    exit( EXIT_SUCCESS );
  };
  // is a config file specified?
  Opts.extract( 'c', configFileName );
  if ( configuration.fill( configFileName ) ){
    cerr << "config read from: " << configFileName << endl;
  }
  else {
    cerr << "failed to read configuration from! '" << configFileName << "'" << endl;
    cerr << "did you correctly install the frogdata package?" << endl;
    return false;
  }
  string value;
  // debug opts
  if ( Opts.extract('d', value ) ){
    int debug = 0;
    if ( !TiCC::stringTo<int>( value, debug ) ){
      cerr << "-d value should be an integer" << endl;
      return false;
    }
    configuration.setatt( "debug", value, "mbma" );
  };

  if ( Opts.extract( 't', value ) ){
    ifstream is( value );
    if ( !is ){
      cerr << "input stream " << value << " is not readable" << endl;
      return false;
    }
    fileNames.push_back( value );
  }
  else {
    fileNames = Opts.getMassOpts();
  }
  useTagger = !Opts.extract( "notagger" );
  useTokenizer = !Opts.extract( "notokenizer" );
  if ( Opts.extract( "daring" ) ){
    configuration.setatt( "daring", "1", "mbma" );
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
  if ( useTokenizer ){
    if ( !tokenizer.init( configuration ) ){
      cerr << "UCTO Initialization failed." << endl;
      return false;
    }
  }
  if ( useTagger ){
    if ( !tagger.init( configuration ) ){
      cerr << "CGN Initialization failed." << endl;
      return false;
    }
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
    vector<string> sentences;
    if ( useTokenizer ){
      sentences = tokenizer.tokenize( line );
    }
    else {
      sentences.push_back( line );
    }
    for ( auto const& s : sentences ){
      if ( useTagger ){
	vector<TagResult> tagv = tagger.tagLine( s );
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
	  myMbma.filterHeadTag( head );
	  myMbma.filterSubTags( v );
	  vector<vector<string> > res = myMbma.getResult(true);
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
      }
      else {
	vector<string> parts;
	TiCC::split( s, parts );
	for( auto const& w : parts ){
	  UnicodeString uWord = folia::UTF8ToUnicode(w);
	  uWord.toLower();
	  myMbma.Classify( uWord );
	  vector<vector<string> > res = myMbma.getResult(true);
	  string line = w + "\t";
	  for ( auto const& r : res ){
	    line += "[";
	    for ( auto const& m: r){
	      line += m + ",";
	    }
	    line.erase(line.length()-1);
	    line += "]/";
	  }
	  line.erase(line.length()-1);
	  cout << line << endl;
	}
      }
      cout << "<utt>" << endl << endl;
    }
    cout << endl;
  }
  return;
}


int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  cerr << "mbma " << VERSION << " (c) ILK 1998 - 2015" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  TiCC::CL_Options Opts("Vt:d:hc:","daring,version,notagger,notokenizer");
  try {
    Opts.init(argc, argv);
  }
  catch ( const exception& e ){
    cerr << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  cerr << "based on [" << Timbl::VersionName() << "]" << endl;
  cerr << "configdir: " << configDir << endl;
  if ( parse_args(Opts) ){
    if (  !init() ){
      cerr << "terminated." << endl;
      return EXIT_FAILURE;
    }
    for ( size_t i=0; i < fileNames.size(); ++i ){
      string TestFileName = fileNames[i];
      ifstream in(TestFileName);
      if ( in.good() ){
	cerr << "Processing: " << TestFileName << endl;
	Test( in );
      }
      else {
	cerr << "unable to open: " << TestFileName << endl;
	return EXIT_FAILURE;
      }
    }
  }
  else {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
