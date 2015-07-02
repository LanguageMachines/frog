/* ex: set tabstop=8 expandtab: */
/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
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
#include "config.h"
#ifdef HAVE_OPENMP
#include <omp.h>
#endif

#include "timbl/TimblAPI.h"
#include "timblserver/FdStream.h"
#include "timblserver/ServerBase.h"

// individual module headers

#include "frog/Frog.h" //internal interface, included by all modules
#include "frog/FrogAPI.h" //public API interface
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"

using namespace std;
using namespace folia;
using namespace TiCC;

string testDirName;
string outputFileName;
bool wantOUT;
string XMLoutFileName;
string outputDirName;
string xmlDirName;
set<string> fileNames;

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

void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -e <encoding>          specify encoding of the input (default UTF-8)\n"
       << "\t -t <testfile>          Run frog on this file\n"
       << "\t -x <testfile>          Run frog on this FoLiA XML file. Or the files from 'testdir'\n"
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
       << "\t -c <filename>    Set configuration file (default "
       << FrogAPI::defaultConfigFile() << ")\n"
       << "\t============= OUTPUT OPTIONS ============================================\n"
       << "\t -o <outputfile>	    Output columned output to file, instead of default stdout\n"
       << "\t -X <xmlfile>          Output also to an XML file in FoLiA format\n"
       << "\t --id=<docid>          Document ID, used in FoLiA output. (Default 'untitled')\n"
       << "\t --outputdir=<dir>     Output to dir, instead of default stdout\n"
       << "\t --xmldir=<dir>        Use 'dir' to output FoliA XML to.\n"
       << "\t --tmpdir=<directory>  (location to store intermediate files. Default /tmp )\n"
       << "\t --keep-parser-files keep intermediate parser files. (last sentence only).\n"
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

bool parse_args( TiCC::CL_Options& Opts, FrogOptions& options,
		 LogStream* theErrLog ) {
  // process the command line and fill FrogOptions to initialize the API
  // also fill some globals we use for our own main.

  string value;
  if ( Opts.is_present('V' ) || Opts.is_present("version" ) ){
    // we already did show what we wanted.
    exit( EXIT_SUCCESS );
  }
  if ( Opts.is_present( 'h' ) ) {
    usage();
    exit( EXIT_SUCCESS );
  };
  // is a config file specified?
  string configFileName = FrogAPI::defaultConfigFile();
  Opts.extract( 'c',  configFileName );
  if ( configuration.fill( configFileName ) ){
    *Log(theErrLog) << "config read from: " << configFileName << endl;
  }
  else {
    cerr << "failed to read configuration from '" << configFileName << "' !!" << endl;
    cerr << "Did you correctly install the frogdata package?" << endl;
    return false;
  }

  // debug opts
  if ( Opts.extract ('d', value) ) {
    if ( !stringTo<int>( value, options.debugFlag ) ){
      *Log(theErrLog) << "-d value should be an integer" << endl;
      return false;
    }
    configuration.setatt( "debug", value );
  }
  else {
    configuration.setatt( "debug", "0" );
  }
  if ( Opts.extract( "debug", value ) ) {
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
  }

  options.doSentencePerLine = Opts.extract( 'n' );
  options.doQuoteDetection = Opts.extract( 'Q' );
  if ( Opts.extract( "skip", value )) {
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
    else if ( !options.doMwu ){
      *Log(theErrLog) << " Parser disabled, because MWU is deselected" << endl;
      options.doParse = false;
    }
    Opts.remove("skip");
  };

  if ( Opts.extract( "daring" ) ) {
    options.doDaringMorph = true;
    options.doMorph = true;
  }
  Opts.extract( 'e', options.encoding );

  if ( Opts.extract( "max-parser-tokens", value ) ){
    if ( !stringTo<unsigned int>( value, options.maxParserTokens ) ){
      *Log(theErrLog) << "max-parser-tokens value should be an integer" << endl;
      return false;
    }
  }

  options.doServer = Opts.extract('S', options.listenport );

#ifdef HAVE_OPENMP
  if ( options.doServer ) {
    // run in one thread in server mode, forking is too expensive for lots of small snippets
    options.numThreads =  1;
  }
  else if ( Opts.extract( "threads", value ) ){
    int num;
    if ( !stringTo<int>( value, num ) || num < 1 ){
      *Log(theErrLog) << "threads value should be a positive integer" << endl;
      return false;
    }
    options.numThreads = num;
  }
#endif

  if ( Opts.extract( "keep-parser-files" ) ){
    configuration.setatt( "keepIntermediateFiles", "true", "parser" );
  }
  options.tmpDirName = configuration.lookUp( "tmpdir", "global" );
  Opts.extract( "tmpdir", options.tmpDirName ); // so might be overridden
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
  if ( Opts.extract( "testdir", TestFileName ) ) {
#ifdef HAVE_DIRENT_H
    options.doDirTest = true;
    testDirName = TestFileName;
    if ( testDirName[testDirName.size()-1] != '/' ){
      testDirName += "/";
    }
    if ( !existsDir( testDirName ) ){
      *Log(theErrLog) << "input dir " << testDirName << " not readable" << endl;
      return false;
    }
#else
      *Log(theErrLog) << "--testdir option not supported!" << endl;
#endif
  }
  else if ( Opts.extract( 't', TestFileName ) ) {
    ifstream is( TestFileName );
    if ( !is ){
      *Log(theErrLog) << "input stream " << TestFileName << " is not readable" << endl;
      return false;
    }
  };
  wantOUT = false;
  if ( Opts.extract( "outputdir", outputDirName )) {
    if ( outputDirName[outputDirName.size()-1] != '/' ){
      outputDirName += "/";
    }
#ifdef HAVE_DIRENT_H
    if ( !existsDir( outputDirName ) ){
      *Log(theErrLog) << "output dir " << outputDirName << " not readable" << endl;
      return false;
    }
#endif
    wantOUT = true;
  }
  else if ( Opts.extract( 'o', outputFileName ) ){
    wantOUT = true;
  };
  options.doXMLout = false;
  Opts.extract( "id", options.docid );
  if ( Opts.extract( "xmldir", xmlDirName ) ){
    if ( xmlDirName[xmlDirName.size()-1] != '/' ){
      xmlDirName += "/";
    }
#ifdef HAVE_DIRENT_H
    if ( !existsDir( xmlDirName ) ){
      *Log(theErrLog) << "XML output dir " << xmlDirName << " not readable" << endl;
      return false;
    }
#endif
    options.doXMLout = true;
  }
  else if ( Opts.extract('X', XMLoutFileName ) ){
    options.doXMLout = true;
  }

  options.doKanon = Opts.extract("KANON");

  options.doXMLin = false;
  if ( Opts.extract ('x', value ) ){
    options.doXMLin = true;
    if ( !value.empty() ){
      if ( ! (xmlDirName.empty() &&
	      testDirName.empty() &&
	      TestFileName.empty() ) ){
	*Log(theErrLog) << "-x may not provide a value when --testdir or --xmldir is provided" << endl;
	return false;
      }
      TestFileName = value;
      ifstream is( value );
      if ( !is ){
	*Log(theErrLog) << "input stream " << value << " is not readable" << endl;
	return false;
      }
    }
  }
  if ( Opts.extract( "textclass", options.textclass ) ){
    if ( !options.doXMLin ){
      *Log(theErrLog) << "--textclass is only valid when -x is also present" << endl;
      return false;
    }
  }

  if ( !XMLoutFileName.empty() && !testDirName.empty() ){
    *Log(theErrLog) << "useless -X value" << endl;
    return false;
  }

  Opts.extract ("uttmarker", options.uttmark );
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
      *Log(theErrLog) << "error: couldn't find any files in directory: "
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
  if ( !Opts.empty() ){
    *Log(theErrLog) << "unhandled commandline options: " << Opts.toString() << endl;

    return false;
  }
  return true;
}


int main(int argc, char *argv[]) {
  cerr << "frog " << VERSION << " (c) ILK 1998 - 2015" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  cerr << "based on [" << Tokenizer::VersionName() << ", "
       << folia::VersionName() << ", "
       << Timbl::VersionName() << ", "
       << TimblServer::VersionName() << ", "
       << Tagger::VersionName() << "]" << endl;
  LogStream *theErrLog = new LogStream( cerr, "frog-", StampMessage );
  std::ios_base::sync_with_stdio(false);
  FrogOptions options;

  try {
    TiCC::CL_Options Opts("c:e:o:t:x::X::nQhVd:S:",
			  "textclass:,testdir:,uttmarker:,max-parser-tokens:,"
			  "skip:,id:,outputdir:,xmldir:,tmpdir:,daring,"
			  "debug:,keep-parser-files,version,threads:,KANON");

    Opts.init(argc, argv);
    bool parsed = parse_args( Opts, options, theErrLog );
    if (!parsed) {
      throw runtime_error( "init failed" );
    }
    FrogAPI frog( options, configuration, theErrLog );

    if ( !fileNames.empty() ) {
      string outPath = outputDirName;
      string xmlPath = xmlDirName;

      ostream *outS = 0;
      if ( !outputFileName.empty() ){
        outS = new ofstream( outputFileName );
      }
      for ( auto const& name : fileNames ){
	string testName = testDirName + name;
	if ( !TiCC::isFile( testName ) ){
	  *Log(theErrLog) << "skip " << testName << " (file not found )"
			  << endl;
	  continue;
	}
	string outName;
	if ( outS == 0 ){
	  if ( wantOUT ){
	    if ( options.doXMLin ){
	      if ( !outPath.empty() )
		outName = outPath + name + ".out";
	    }
	    else {
	      outName = outPath + name + ".out";
	    }
	    outS = new ofstream( outName );
	  } else {
	    outS = &cout;
	  }
	}
	string xmlName = XMLoutFileName;
	if ( xmlName.empty() ){
	  if ( !xmlDirName.empty() ){
	    if ( name.rfind(".xml") == string::npos )
	      xmlName = xmlPath + name + ".xml";
	    else
	      xmlName = xmlPath + name;
	  }
	  else if ( options.doXMLout )
	    xmlName = name + ".xml"; // do not clobber the inputdir!
	}
	*Log(theErrLog) << "Frogging " << testName << endl;
	try {
	  frog.FrogFile( testName, *outS, xmlName );
	}
	catch ( exception& e ){
	  *Log(theErrLog) << "problem frogging: " << name << endl;
	  *Log(theErrLog) << e.what() << endl;
	  continue;
	}
	if ( !outName.empty() ){
	  *Log(theErrLog) << "results stored in " << outName << endl;
	  delete outS;
	  outS = 0;
	}
      }
      if ( !outputFileName.empty() ){
	*Log(theErrLog) << "results stored in " << outputFileName << endl;
	delete outS;
      }
    }
    else if ( options.doServer ) {
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
	    }
	    else if (pid == 0)  {
	      frog.FrogServer( conn );
	      exit(EXIT_SUCCESS);
	    }
	  }
	  else {
	    throw( runtime_error( "Accept failed" ) );
	  }
	}
      }
      catch ( std::exception& e ) {
	*Log(theErrLog) << "Server error:" << e.what() << " Exiting." << endl;
	throw;
      }
    }
    else {
      // interactive mode
      frog.FrogInteractive();
    }
  }
  catch ( const exception& e ){
    *Log(theErrLog) << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
