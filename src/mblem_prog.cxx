/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2024
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

#include "frog/mblem_mod.h"

#include <string>
#include <iostream>
#include <fstream>
#include <vector>

#include "config.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/folia.h"
#include "frog/ucto_tokenizer_mod.h"
#include "frog/cgn_tagger_mod.h"

using namespace std;
using namespace icu;
using namespace Timbl;
using namespace Tagger;
using TiCC::operator<<;

TiCC::LogStream my_default_log( cerr ); // fall-back
TiCC::LogStream *theErrLog = &my_default_log;  // fill the externals

vector<string> fileNames;
bool usewordlist = false;
bool useTagger = true;
bool useTokenizer = true;
string output_name;

TiCC::Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/nld/";
static string configFileName = configDir + "frog.cfg";

static UctoTokenizer tokenizer(theErrLog);
static CGNTagger tagger(theErrLog);

void usage( ) {
  cout << endl << "mblem [options] testfile" << endl
       << "Options:n" << endl;
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -t <testfile>    Run mblem on this file\n"
       << "\t --wordlist <wordfile>    Run on a wordlist. Produces a lemma list\n"
       << "\t -o <outputfile>    write results in 'outputfile'\n"

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

  if ( Opts.extract( 'o', value ) ){
    ofstream os( value );
    if ( !os ){
      cerr << "output stream " << value << " is not writeable" << endl;
      return false;
    }
    output_name = value;
  }
  useTagger = !Opts.is_present( "notagger" );
  useTokenizer = !Opts.is_present( "notokenizer" );
  usewordlist = Opts.is_present( "wordlist" );
  if ( usewordlist ){
    useTagger = false;
    useTokenizer = false;
  }
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

void Test( istream& in, ostream& os ){
  UnicodeString line;
  while ( TiCC::getline( in, line ) ){
    if ( line.isEmpty() ) {
      os << endl;
      continue;
    }
    vector<UnicodeString> sentences;
    if ( useTokenizer ){
      sentences = tokenizer.tokenize( line );
    }
    else {
      sentences.push_back( line );
    }
    for ( auto& us : sentences ){
      if ( useTagger ){
	vector<TagResult> tagrv = tagger.tagLine( us );
	for ( const auto& tr : tagrv ){
	  myMblem.Classify( tr.word() );
	  myMblem.filterTag( tr.assigned_tag() );
	  vector<pair<UnicodeString,UnicodeString> > res = myMblem.getResult();
	  UnicodeString out_line = tr.word() + " {" + tr.assigned_tag() + "}\t";
	  for ( const auto& [lemma,tag] : res ){
	    out_line += lemma + "[" + tag + "]/";
	  }
	  out_line.remove(out_line.length()-1);
	  out_line += "\n";
	  os << out_line;
	}
      }
      else {
	if ( usewordlist ){
	  if ( us[0] == '"' ){
	    us.remove(0,1);
	  }
	  if ( us.endsWith( "\"" ) ){
	    us.remove( us.length()-1 );
	  }
	  vector<UnicodeString> parts = TiCC::split( us );
	  if ( parts.size() > 1 ){
	    cerr << "skipping multiword " << us << endl;
	  }
	  else {
	    myMblem.Classify( parts[0] );
	    vector<pair<UnicodeString,UnicodeString> > res = myMblem.getResult();
	    UnicodeString out_line = parts[0] + ",";
	    set<UnicodeString> out_set;
	    for ( const auto& [lemma,tag] : res ){
	      if ( lemma != parts[0] || out_set.empty() ){
		out_set.insert( lemma );
	      }
	    }
	    for ( const auto& st : out_set ){
	      out_line += st + ",";
	    }
	    os << out_line;
	  }
	}
	else {
	  vector<UnicodeString> parts = TiCC::split( us );
	  for ( const auto& p : parts ){
	    myMblem.Classify( p );
	    vector<pair<UnicodeString,UnicodeString> > res = myMblem.getResult();
	    UnicodeString out_line = p + "\t";
	    for ( const auto& [lemma,tag] : res ){
	      out_line += lemma + "[" + tag + "]/";
	    }
	    out_line.remove(out_line.length()-1);
	    out_line += "\n";
	    os << out_line;
	  }
	}
      }
      if ( !usewordlist ){
	os << "<utt>\n\n";
      }
    }
    os << "\n";
  }
  os.flush();
  return;
}


int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  cerr << "mblem " << VERSION << " (c) CLTS, ILK 2014 - 2024." << endl;
  cerr << "CLST  - Centre for Language and Speech Technology,"
       << "Radboud University" << endl
       << "ILK   - Induction of Linguistic Knowledge Research Group,"
       << "Tilburg University" << endl;
  TiCC::CL_Options Opts("c:t:hVd:o:", "version,notagger,notokenizer,wordlist");
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
    ostream *os;
    if ( !output_name.empty() ){
      os = new ofstream( output_name );
    }
    else {
      os = &cout;
    }
    for ( const auto& TestFileName : fileNames ){
      ifstream in(TestFileName);
      if ( in.good() ){
	Test( in, *os );
      }
      else {
	cerr << "unable to open: " << TestFileName << endl;
	continue;
      }
    }
    if ( os != &cout ){
      // cppcheck-suppress autovarInvalidDeallocation
      delete os;
    }
  }
  else {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
