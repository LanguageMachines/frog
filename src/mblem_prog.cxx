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
#include "frog/ucto_tokenizer_mod.h"
#include "frog/cgn_tagger_mod.h"
#include "frog/mblem_mod.h"

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
  cout << endl << "mblem [options] testfile" << endl
       << "Options:n" << endl;
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -t <testfile>    Run mblem on this file\n"
       << "\t --notokenizer    Don't use a tokenizer, so assume all text is tokenized already.\n"
       << "\t --notagger       Don't use a tagger to disambiguate, so give ALL variants.\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version .   Show version info.\n"
       << "\t -d <debug level>    (for more verbosity)\n";
}

static Mblem myMblem(theErrLog);

bool parse_args( TiCC::CL_Options& Opts ) {
  cerr << "start " << Opts << endl;
  if ( Opts.is_present('V') || Opts.is_present("version" ) ){
    // we already did show what we wanted.
    exit( EXIT_SUCCESS );
  }
  if ( Opts.is_present ('h') ) {
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

  // debug opts
  string value;
  if ( Opts.extract( 'd', value ) ) {
    int debug = 0;
    if ( !TiCC::stringTo<int>( value, debug ) ){
      cerr << "-d value should be an integer" << endl;
      return false;
    }
    configuration.setatt( "debug", value, "mblem" );
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
  };
  useTagger = !Opts.is_present( "notagger" );
  useTokenizer = !Opts.is_present( "notokenizer" );
  return true;
}

bool init(){
  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  if ( !myMblem.init( configuration ) ){
    cerr << "MBLEM Initialization failed." << endl;
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
      cerr << "Tagger Initialization failed." << endl;
      return false;
    }
  }
  cerr << "Initialization done." << endl;
  return true;
}

void Test( istream& in ){
  string line;
  while ( getline( in, line ) ){
    vector<string> sentences;
    if ( useTokenizer ){
      sentences = tokenizer.tokenize( line );
    }
    else
      sentences.push_back( line );
    
    for ( auto const& s : sentences ){
      if ( useTagger ){
	vector<TagResult> tagv = tagger.tagLine( s );
	for ( size_t w=0; w < tagv.size(); ++w ){
	  UnicodeString uWord = folia::UTF8ToUnicode(tagv[w].word());
	  myMblem.Classify( uWord );
	  myMblem.filterTag( tagv[w].assignedTag() );
	  vector<pair<string,string> > res = myMblem.getResult();
	  string line = tagv[w].word() + " {" + tagv[w].assignedTag() + "}\t";
	  for ( const auto& p : res ){
	    line += p.first + "[" + p.second + "]/";
	  }
	  line.erase(line.length()-1);
	  cout << line << endl;
	}
      }
      else {
	vector<string> parts;
	TiCC::split( s, parts );
	for ( const auto& w : parts ){
	  UnicodeString uWord = folia::UTF8ToUnicode(w);
	  myMblem.Classify( uWord );
	  vector<pair<string,string> > res = myMblem.getResult();
	  string line = w + "\t";
	  for ( const auto& p : res ){
	    line += p.first + "[" + p.second + "]/";
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
  cerr << "mblem " << VERSION << " (c) ILK 1998 - 2015" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  TiCC::CL_Options Opts("c:t:hVd:", "version,notagger,notokenizer");
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
	Test( in );
      }
      else {
	cerr << "unable to open: " << TestFileName << endl;
	continue;
      }
    }
  }
  else {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
