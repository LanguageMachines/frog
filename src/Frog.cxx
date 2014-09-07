/* ex: set tabstop=8 expandtab: */
/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2014
  Tilburg University

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for Dutch

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

// Python.h seems to best included first. It tramples upon defines like:
// _XOPEN_SOURCE, _POSIX_C_SOURCE" etc.
#include "Python.h"

#include <cstdlib>
#include <cstdio>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <omp.h>

#include "config.h"

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else
#    define NO_READLINE
#  endif /* !defined(HAVE_READLINE_H) */
#else
#  define NO_READLINE
#endif /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  endif /* defined(HAVE_READLINE_HISTORY_H) */
#endif /* HAVE_READLINE_HISTORY */


#include "timbl/TimblAPI.h"
#include "timblserver/FdStream.h"
#include "timblserver/ServerBase.h"

// individual module headers

#include "frog/Frog.h" //internal interface, included by all modules
#include "frog/FrogAPI.h" //public API interface
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"

using namespace std;
using namespace folia;
using namespace TiCC;

LogStream my_default_log( cerr, "frog-", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals

string testDirName;
string outputFileName;
bool wantOUT;
string XMLoutFileName;
string outputDirName;
string xmlDirName;
set<string> fileNames;
string ProgName;
int debugFlag = 0; //0 for none, more for more output

FrogOptions options;

/* assumptions:
   each components gets its own configfile per cmdline options
   another possibility is the U: en K: style option seperation

   each component has a read_settings_file in own namespace,
        which sets variables global to that namespace
	to be used in the init() for that namespace

   Further,
   each component provides a Test(File) which writes output to
        File.<componentname>_out
   and a Classify(Instance) which produces relevant output
        as a string return
	or somehwere else,
   to be determined later,
   after pre- and postprocessing raw classification data

*/

Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/";
static string configFileName = configDir + "frog.cfg";

void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -e <encoding>          specify encoding of the input (default UTF-8)\n"
       << "\t -t <testfile>          Run frog on this file\n"
       << "\t -x <testfile>          Run frog on this FoLiA XML file. Or the files form 'testdir'\n"
       << "\t --textclass=<cls>      use the specified class to search for text in the the FoLia docs.\n"
       << "\t --testdir=<directory>  All files in this dir will be tested\n"
       << "\t --uttmarker=<mark>     utterances are separated by 'mark' symbols"
       << "\t                        (default none)\n"
       << "\t -n                     Assume input file to hold one sentence per line\n"
       << "\t --max-parser-tokens=<n> inhibit parsing when a sentence contains over 'n' tokens. (default: no limit)\n"
       << "\t -Q                   Enable quote detection in tokeniser.\n"
       << "\t============= MODULE SELECTION ==========================================\n"
       << "\t --skip=[mptncla]    Skip Tokenizer (t), Lemmatizer (l), Morphological Analyzer (a), Chunker (c), Multi-Word Units (m), Named Entity Recognition (n), or Parser (p) \n"
       << "\t============= CONFIGURATION OPTIONS =====================================\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t============= OUTPUT OPTIONS ============================================\n"
       << "\t -o <outputfile>	    Output columned output to file, instead of default stdout\n"
       << "\t -X <xmlfile>          Output also to an XML file in FoLiA format\n"
       << "\t --id=<docid>          Document ID, used in FoLiA output. (Default 'untitled')\n"
       << "\t --outputdir=<dir>     Output to dir, instead of default stdout\n"
       << "\t --xmldir=<dir>        Use 'dir' to output FoliA XML to.\n"
       << "\t --tmpdir=<directory>  (location to store intermediate files. Default /tmp )\n"
       << "\t --keep-parser-files=[yes|no] keep intermediate parser files. (last sentence only).\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version .   Show version info.\n"
       << "\t -d <debug level>    (for more verbosity)\n"
       << "\t --debug=<module><level>,<module><level>... (eg --debug=l5,n3) \n"
       << "\t\t Set debug value for Tokenizer (t), Lemmatizer (l), Morphological Analyzer (a), Chunker (c), Multi-Word Units (m), Named Entity Recognition (n), or Parser (p) \n"
       << "\t -S <port>              Run as server instead of reading from testfile\n"
#ifdef HAVE_OPENMP
       << "\t --threads=<n>       Use a maximun of 'n' threads. Default: all we can get. \n"
#endif
       << "\t                     (but always 1 for server mode)\n";
}

//**** stuff to process commandline options *********************************



bool parse_args( TiCC::CL_Options& Opts ) {
  string value;
  bool mood;
  if ( Opts.is_present('V', value, mood ) ||
       Opts.is_present("version", value ) ){
    // we already did show what we wanted.
    exit( EXIT_SUCCESS );
  }
  if ( Opts.is_present ('h', value, mood)) {
    usage();
    exit( EXIT_SUCCESS );
  };
  // is a config file specified?
  if ( Opts.is_present( 'c',  value, mood ) ) {
    configFileName = value;
    Opts.remove( 'c' );
  };

  if ( configuration.fill( configFileName ) ){
    *Log(theErrLog) << "config read from: " << configFileName << endl;
  }
  else {
    cerr << "failed to read configuration from '" << configFileName << "' !!" << endl;
    cerr << "Did you correctly install the frogdata package?" << endl;
    return false;
  }

  // debug opts
  if ( Opts.is_present ('d', value, mood)) {
    if ( !stringTo<int>( value, debugFlag ) ){
      *Log(theErrLog) << "-d value should be an integer" << endl;
      return false;
    }
    configuration.setatt( "debug", value );
    Opts.remove('d');
  }
  else {
    configuration.setatt( "debug", "0" );
  }
  if ( Opts.is_present( "debug", value ) ) {
    if ( value.empty() ){
      *Log(theErrLog) << "missing a value for --debug (did you forget the '='?)" << endl;
      return false;
    }
    value = TiCC::lowercase( value );
    vector<string> vec;
    TiCC::split_at( value, vec, "," );
    for ( size_t i=0; i < vec.size(); ++i ){
      char mod = vec[i][0];
      string value = vec[i].substr(1);
      int dbval = 0;
      if ( !stringTo<int>( value, dbval ) ){
	cerr << "expected integer value for --debug=" << mod << value << endl;
	return false;
      }
      switch ( mod ){
      case 't':
	configuration.setatt( "debug", value, "tokenizer" );
	break;
      case 'l':
	configuration.setatt( "debug", value, "mblem" );
	break;
      case 'a':
	configuration.setatt( "debug", value, "mbma" );
	break;
      case 'm':
	configuration.setatt( "debug", value, "mwu" );
	break;
      case 'c':
	configuration.setatt( "debug", value, "IOB" );
	break;
      case 'n':
	configuration.setatt( "debug", value, "NER" );
	break;
      case 'p':
	configuration.setatt( "debug", value, "parser" );
	break;
      default:
	cerr << "unknown module '" << mod << "'" << endl;
	return false;
      }
    }
    Opts.remove("debug");
  }

  if ( Opts.is_present ('n', value, mood)) {
    options.doSentencePerLine = true;
  };
  if ( Opts.is_present ('Q', value, mood)) {
    options.doQuoteDetection = true;
  };
  if ( Opts.is_present( "skip", value )) {
    if ( value.empty() ){
      *Log(theErrLog) << "missing a value for --skip (did you forget the '='?)" << endl;
      return false;
    }
    string skip = value;
    if ( skip.find_first_of("tT") != string::npos )
      options.doTok = false;
    if ( skip.find_first_of("lL") != string::npos )
      options.doLemma = false;
    if ( skip.find_first_of("aA") != string::npos )
      options.doMorph = false;
    if ( skip.find_first_of("mM") != string::npos )
      options.doMwu = false;
    if ( skip.find_first_of("cC") != string::npos )
      options.doIOB = false;
    if ( skip.find_first_of("nN") != string::npos )
      options.doNER = false;
    if ( skip.find_first_of("pP") != string::npos )
      options.doParse = false;
    Opts.remove("skip");
  };

  if ( Opts.is_present( "daring", value ) ) {
    if ( value.empty() )
      value = "1";
    options.doDaringMorph = stringTo<bool>( value );
    if ( options.doDaringMorph ){
      options.doMorph = true;
    }
  }
  if ( Opts.is_present( 'e', value, mood)) {
    options.encoding = value;
  }

  if ( Opts.is_present( "max-parser-tokens", value ) ){
    if ( value.empty() ){
      *Log(theErrLog) << "max-parser-tokens option without value " << endl;
      return false;
    }
    else {
      if ( !stringTo<unsigned int>( value, options.maxParserTokens ) ){
	*Log(theErrLog) << "max-parser-tokens value should be an integer" << endl;
	return false;
      }
    }
    Opts.remove("max-parser-tokens");
  }

  if ( Opts.is_present ('S', value, mood)) {
    options.doServer = true;
    options.listenport= value;
  }
#ifdef HAVE_OPENMP
  if ( options.doServer ) {
    // run in one thread in server mode, forking is too expensive for lots of small snippets
    omp_set_num_threads( 1 );
  }
  else if ( Opts.is_present( "threads", value ) ){
    if ( value.empty() ){
      *Log(theErrLog) << "missing a value for --threads (did you forget the '='?)" << endl;
      return false;
    }
    int num;
    if ( !stringTo<int>( value, num ) || num < 1 ){
      *Log(theErrLog) << "threads value should be a positive integer" << endl;
      return false;
    }
    omp_set_num_threads( num );
  }
#endif

  if ( Opts.is_present( "keep-parser-files", value ) ){
    if ( value.empty() ||
	 stringTo<bool>( value ) ){
      configuration.setatt( "keepIntermediateFiles", "true", "parser" );
      Opts.remove("keep-parser-files");
    }
  }
  options.tmpDirName = configuration.lookUp( "tmpdir", "global" );
  if ( Opts.is_present ( "tmpdir", value )) {
    if ( value.empty() ){
      *Log(theErrLog) << "missing a value for --tmpdir (did you forget the '='?)" << endl;
      return false;
    }
    options.tmpDirName = value;
    Opts.remove("tmpdir");
  }
  if ( options.tmpDirName.empty() ){
    options.tmpDirName = "/tmp/";
  }
  else if ( options.tmpDirName[options.tmpDirName.size()-1] != '/' ){
    options.tmpDirName += "/";
  }
#ifdef HAVE_DIRENT_H
  if ( !options.tmpDirName.empty() ){
    if ( !existsDir( options.tmpDirName ) ){
      *Log(theErrLog) << "temporary dir " << options.tmpDirName << " not readable" << endl;
      return false;
    }
    *Log(theErrLog) << "checking tmpdir: " << options.tmpDirName << " OK" << endl;
  }
#endif
  string TestFileName;
  if ( Opts.is_present ( "testdir", value )) {
#ifdef HAVE_DIRENT_H
    options.doDirTest = true;
    testDirName = value;
    if ( testDirName[testDirName.size()-1] != '/' ){
      testDirName += "/";
    }
    if ( !testDirName.empty() ){
      if ( !existsDir( testDirName ) ){
	*Log(theErrLog) << "input dir " << testDirName << " not readable" << endl;
	return false;
      }
    }
    else {
      *Log(theErrLog) << "missing a value for --testdir (did you forget the '='?)" << endl;
      return false;
    }
#else
      *Log(theErrLog) << "--testdir option not supported!" << endl;
#endif
    Opts.remove("testdir");
  }
  else if ( Opts.is_present( 't', value, mood )) {
    TestFileName = value;
    ifstream is( value.c_str() );
    if ( !is ){
      *Log(theErrLog) << "input stream " << value << " is not readable" << endl;
      return false;
    }
    Opts.remove('t');
  };
  wantOUT = false;
  if ( Opts.is_present( "outputdir", value )) {
    outputDirName = value;
    if ( outputDirName[outputDirName.size()-1] != '/' ){
      outputDirName += "/";
    }
#ifdef HAVE_DIRENT_H
    if ( !outputDirName.empty() ){
      if ( !existsDir( outputDirName ) ){
	*Log(theErrLog) << "output dir " << outputDirName << " not readable" << endl;
	return false;
      }
    }
    else {
      *Log(theErrLog) << "missing a value for --outputdir (did you forget the '='?)" << endl;
      return false;
    }
#endif
    wantOUT = true;
    Opts.remove( "outputdir");
  }
  else if ( Opts.is_present ('o', value, mood)) {
    wantOUT = true;
    outputFileName = value;
    Opts.remove('o');
  };
  options.doXMLout = false;
  if ( Opts.is_present ( "id", value )) {
    if ( value.empty() ){
      *Log(theErrLog) << "missing a value for --id (did you forget the '='?)" << endl;
      return false;
    }
    options.docid = value;
    Opts.remove( "id");
  }
  if ( Opts.is_present( "xmldir", value )) {
    xmlDirName = value;
    if ( xmlDirName[xmlDirName.size()-1] != '/' ){
      xmlDirName += "/";
    }
#ifdef HAVE_DIRENT_H
    if ( !xmlDirName.empty() ){
      if ( !existsDir( xmlDirName ) ){
	*Log(theErrLog) << "XML output dir " << xmlDirName << " not readable" << endl;
	return false;
      }
    }
    else {
      *Log(theErrLog) << "missing a value for --xmldir (did you forget the '='?)" << endl;
      return false;
    }
#endif
    options.doXMLout = true;
    Opts.remove( "xmldir");
  }
  else if ( Opts.is_present ('X', value, mood)) {
    options.doXMLout = true;
    XMLoutFileName = value;
    Opts.remove('X');
  }
  if ( Opts.is_present ("KANON", value ) ){
    options.doKanon = true;
    Opts.remove( "KANON" );
  }
  options.doXMLin = false;
  if ( Opts.is_present ('x', value, mood)) {
    options.doXMLin = true;
    if ( !value.empty() ){
      if ( ! (xmlDirName.empty() &&
	      testDirName.empty() &&
	      TestFileName.empty() ) ){
	*Log(theErrLog) << "-x may not provide a value when --testdir or --xmldir is provided" << endl;
	return false;
      }
      TestFileName = value;
      ifstream is( value.c_str() );
      if ( !is ){
	*Log(theErrLog) << "input stream " << value << " is not readable" << endl;
	return false;
      }
    }
    Opts.remove('x');
  }
  if ( Opts.is_present ( "textclass", value )) {
    if ( !options.doXMLin ){
      *Log(theErrLog) << "--textclass is only valid when -x is also present" << endl;
      return false;
    }
    if ( value.empty() ){
      *Log(theErrLog) << "missing a value for --textclass (did you forget the '='?)" << endl;
      return false;
    }
    options.textclass = value;
    Opts.remove( "textclass");
  }

  if ( !XMLoutFileName.empty() && !testDirName.empty() ){
    *Log(theErrLog) << "useless -X value" << endl;
    return false;
  }

  if ( Opts.is_present ("uttmarker", value )) {
    if ( value.empty() ){
      *Log(theErrLog) << "missing a value for --uttmarker (did you forget the '='?)" << endl;
      return false;
    }
    options.uttmark = value;
  }
  if ( !outputDirName.empty() && testDirName.empty() ){
    *Log(theErrLog) << "useless -outputdir option" << endl;
    return false;
  }
  if ( !testDirName.empty() ){
    if ( options.doXMLin )
      getFileNames( testDirName, ".xml", fileNames );
    else
      getFileNames( testDirName, "", fileNames );
    if ( fileNames.empty() ){
      *Log(theErrLog) << "error: couln't find any files in directory: "
		      << testDirName << endl;
      return false;
    }
  }
  if ( fileNames.empty() ){
    if ( !TestFileName.empty() ){
      fileNames.insert( TestFileName );
    }
    else {
      vector<string> mass = Opts.getMassOpts();
      for (size_t i=0; i < mass.size(); ++i ){
	fileNames.insert( mass[i] );
      }
    }
  }
  if ( fileNames.size() > 1 ){
    if ( !XMLoutFileName.empty() ){
      *Log(theErrLog) << "'-X " << XMLoutFileName
		      << "' is invalid for multiple inputfiles." << endl;
      return false;
    }
  }

  return true;
}




void TestServer( FrogAPI & frog, Sockets::ServerSocket &conn, FrogOptions & options) {

  try {
    while (true) {
      ostringstream outputstream;
      if ( options.doXMLin ){
        string result;
        string s;
        while ( conn.read(s) ){
            result += s + "\n";
            if ( s.empty() )
                break;
        }
        if ( result.size() < 50 ){
            // a FoLia doc must be at least a few 100 bytes
            // so this is wrong. Just bail out
            throw( runtime_error( "read garbage" ) );
        }
        if ( debugFlag )
            *Log(theErrLog) << "received data [" << result << "]" << endl;
        Document doc;
        try {
            doc.readFromString( result );
        } catch ( std::exception& e ){
            *Log(theErrLog) << "FoLiaParsing failed:" << endl << e.what() << endl;
            throw;
        }
        *Log(theErrLog) << "Processing... " << endl;
        frog.tokenizer->tokenize( doc );
        frog.Test( doc, outputstream);
      } else {
        string data = "";
        if ( options.doSentencePerLine ){
            if ( !conn.read( data ) )	 //read data from client
                throw( runtime_error( "read failed" ) );
        }
        else {
            string line;
            while( conn.read(line) ){
                if ( line == "EOT" )
                    break;
                data += line + "\n";
            }
        }
        if (debugFlag)
            *Log(theErrLog) << "Received: [" << data << "]" << endl;
        *Log(theErrLog) << "Processing... " << endl;
        istringstream inputstream(data,istringstream::in);
        Document doc = frog.tokenizer->tokenize( inputstream );
        frog.Test( doc, outputstream );
      }
      if (!conn.write( (outputstream.str()) ) || !(conn.write("READY\n"))  ){
            if (debugFlag)
                *Log(theErrLog) << "socket " << conn.getMessage() << endl;
            throw( runtime_error( "write to client failed" ) );
      }

    }
  }
  catch ( std::exception& e ) {
    if (debugFlag)
      *Log(theErrLog) << "connection lost: " << e.what() << endl;
  }
  *Log(theErrLog) << "Connection closed.\n";
}

#ifdef NO_READLINE
void TestInteractive(FrogAPI & frog, FrogOptions & options) {
  cout << "frog>"; cout.flush();
  string line;
  string data;
  while ( getline( cin, line ) ){
    string data = line;
    if ( options.doSentencePerLine ){
      if ( line.empty() ){
	cout << "frog>"; cout.flush();
	continue;
      }
    }
    else {
      if ( !line.empty() ){
	data += "\n";
      }
      cout << "frog>"; cout.flush();
      string line2;
      while( getline( cin, line2 ) ){
	if ( line2.empty() )
	  break;
	data += line2 + "\n";
	cout << "frog>"; cout.flush();
      }
    }
    if ( data.empty() ){
      cout << "ignoring empty input" << endl;
      cout << "frog>"; cout.flush();
      continue;
    }
    cout << "Processing... " << endl;
    istringstream inputstream(data,istringstream::in);
    Document doc = frog.tokenizer->tokenize( inputstream );
    frog.Test( doc, cout, true );
    cout << "frog>"; cout.flush();
  }
  cout << "Done.\n";
}
#else
void TestInteractive(FrogAPI & frog, FrogOptions & options){
  const char *prompt = "frog> ";
  string line;
  bool eof = false;
  while ( !eof ){
    string data;
    char *input = readline( prompt );
    if ( !input ){
      eof = true;
      break;
    }
    line = input;
    if ( options.doSentencePerLine ){
      if ( line.empty() ){
	continue;
      }
      else {
	data += line + "\n";
	add_history( input );
      }
    }
    else {
      if ( !line.empty() ){
	add_history( input );
	data = line + "\n";
      }
      while ( !eof ){
	char *input = readline( prompt );
	if ( !input ){
	  eof = true;
	  break;
	}
	line = input;
	if ( line.empty() ){
	  break;
	}
	add_history( input );
	data += line + "\n";
      }
    }
    if ( !data.empty() ){
      if ( data[data.size()-1] == '\n' ){
        data = data.substr( 0, data.size()-1 );
      }
      cout << "Processing... '" << data << "'" << endl;
      istringstream inputstream(data,istringstream::in);
      Document doc = frog.tokenizer->tokenize( inputstream );
      frog.Test( doc, cout, true );
    }
  }
  cout << "Done.\n";
}
#endif

int main(int argc, char *argv[]) {
  cerr << "frog " << VERSION << " (c) ILK 1998 - 2014" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  ProgName = argv[0];
  cerr << "based on [" << Tokenizer::VersionName() << ", "
       << folia::VersionName() << ", "
       << Timbl::VersionName() << ", "
       << TimblServer::VersionName() << ", "
       << Tagger::VersionName() << "]" << endl;
  //  cout << "configdir: " << configDir << endl;
  std::ios_base::sync_with_stdio(false);
  try {
    TiCC::CL_Options Opts("c:e:o:t:x::X::nQhVd:S:",
			  "textclass:,testdir:,uttmarker:,max-parser-tokens:,"
			  "skip:,id:,outputdir:,xmldir:,tmpdir:,daring,debug:,"
			  "keep-parser-files:,version,threads:,KANON");

    Opts.init(argc, argv);
    bool parsed = parse_args(Opts);
    if (!parsed) {
        throw runtime_error( "init failed" );
    }
    FrogAPI frog = FrogAPI(&options, &configuration, theErrLog);

    if ( !fileNames.empty() ) {
        string outPath = outputDirName;
        string xmlPath = xmlDirName;
        set<string>::const_iterator it = fileNames.begin();
        ostream *outS = 0;
        if ( !outputFileName.empty() ){
        outS = new ofstream( outputFileName.c_str() );
        }
        while ( it != fileNames.end() ){
            string testName = testDirName;
            testName += *it;
            string outName;
            if ( outS == 0 ){
                if ( wantOUT ){
                    if ( options.doXMLin ){
                        if ( !outPath.empty() )
                        outName = outPath + *it + ".out";
                    } else
                        outName = outPath + *it + ".out";
                    outS = new ofstream( outName.c_str() );
                } else {
                    outS = &cout;
                }
            }
            string xmlName = XMLoutFileName;
            if ( xmlName.empty() ){
                if ( !xmlDirName.empty() ){
                    if ( it->rfind(".xml") == string::npos )
                    xmlName = xmlPath + *it + ".xml";
                    else
                    xmlName = xmlPath + *it;
                } else if ( options.doXMLout )
                    xmlName = *it + ".xml"; // do not clobber the inputdir!
            }
            *Log(theErrLog) << "Frogging " << testName << endl;
            frog.Test( testName, *outS, xmlName );
            if ( !outName.empty() ){
                *Log(theErrLog) << "results stored in " << outName << endl;
                delete outS;
                outS = 0;
            }
            ++it;
        }
        if ( !outputFileName.empty() ){
            *Log(theErrLog) << "results stored in " << outputFileName << endl;
            delete outS;
        }
      } else if ( options.doServer ) {
        //first set up some things to deal with zombies
        struct sigaction action;
        action.sa_handler = SIG_IGN;
        sigemptyset(&action.sa_mask);
#ifdef SA_NOCLDWAIT
        action.sa_flags = SA_NOCLDWAIT;
#endif
        sigaction(SIGCHLD, &action, NULL);

        srand((unsigned)time(0));

        *Log(theErrLog) << "Listening on port " << options.listenport << "\n";

        try {
            // Create the socket
            Sockets::ServerSocket server;
            if ( !server.connect( options.listenport ) )
                throw( runtime_error( "starting server on port " + options.listenport + " failed" ) );
            if ( !server.listen( 5 ) ) {
                // maximum of 5 pending requests
                throw( runtime_error( "listen(5) failed" ) );
            }
            while ( true ) {
                Sockets::ServerSocket conn;
                if ( server.accept( conn ) ){
                    *Log(theErrLog) << "New connection..." << endl;
                    int pid = fork();
                    if (pid < 0) {
                        *Log(theErrLog) << "ERROR on fork" << endl;
                        throw runtime_error( "FORK failed" );
                    } else if (pid == 0)  {
                        //		  server = NULL;
                        TestServer(frog, conn, options );
                        exit(EXIT_SUCCESS);
                    }
                } else {
                    throw( runtime_error( "Accept failed" ) );
                }
            }
        } catch ( std::exception& e ) {
            *Log(theErrLog) << "Server error:" << e.what() << " Exiting." << endl;
            throw;
        }
      } else {
        // interactive mode
        TestInteractive( frog, options );
      }
  } catch ( const exception& e ){
    *Log(theErrLog) << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
