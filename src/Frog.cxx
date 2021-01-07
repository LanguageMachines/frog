/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2021
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

#include <cstdlib>
#include <cstdio>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <cstring>
#include <random>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include "config.h"
#ifdef HAVE_OPENMP
#include <omp.h>
#endif

#include "timbl/TimblAPI.h"
#include "ticcutils/FdStream.h"
#include "ticcutils/ServerBase.h"

// individual module headers

#include "frog/Frog-util.h"
#include "frog/FrogAPI.h" //public API interface
#include "frog/ucto_tokenizer_mod.h"
#include "frog/tagger_base.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"

using namespace std;

#define LOG *TiCC::Log(theErrLog)
#define DBG *TiCC::Log(theDbgLog)

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

TiCC::Configuration configuration;

void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -e <encoding>          specify encoding of the input (default UTF-8)\n"
       << "\t -t <testfile>          Run frog on this file\n"
       << "\t -x <testfile>          Run frog on this FoLiA XML file. Or the files from 'testdir'\n"
       << "\t --textclass=<cls>      use the specified class to search for text in the the FoLiA docs. (default 'current'). \n"
       << "\t                        the same value is used for output too.\n"
       << "\t                        Deprecated! use --inputclass and --outputclass.\n"
       << "\t --inputclass=<cls>     use the specified class to search for text in the the FoLiA docs. (default 'current') \n"
       << "\t --outputclass=<cls>    use the specified class to output text in the the FoLia docs. (default 'inputclass') \n"
       << "\t --testdir=<directory>  All files in this dir will be tested\n"
       << "\t --uttmarker=<mark>     utterances are separated by 'mark' symbols (default none)\n"
       << "\t -n                     Assume input file to hold one sentence per line\n"
       << "\t --retry                assume frog is running again on the same input,\n"
       << "\t                        already done files are skipped. (detected on the basis of already existing output files)\n"
       << "\t --max-parser-tokens=<n> inhibit parsing when a sentence contains over 'n' tokens. (default: 500, needs already 16Gb of memory!)\n"
    //       << "\t -Q                     Enable quote detection in tokenizer.\n"
       << "\t --JSONin               The input is JSON. Implies JSONout too! (server mode only)\n"
       << "\t -T or --textredundancy=[full|minimal|none]\n"
       << "\t                        Set the text redundancy level in the tokenizer for text nodes in FoLiA output: \n"
       << "\t                        'full' - add text to all levels: <p> <s> <w> etc.\n"
       << "\t                        'minimal' - don't introduce text on higher levels, but retain what is already there.\n"
       << "\t                        'none' - only introduce text on <w>, AND remove all text from higher levels\n"
       << "\t ============= MODULE SELECTION ==========================================\n"
       << "\t --skip=[mptncla]       Skip Tokenizer (t), Lemmatizer (l), Morphological Analyzer (a), Chunker (c),\n"
       << "\t                        Multi-Word Units (m), Named Entity Recognition (n), or Parser (p)\n"
       << "\t --alpino               use a locally installed Alpino parser\n"
       << "\t --alpino=server        use a remote installed Alpino server\n"
       << "\t                        (as specified in the frog configuration file \n"
       << "\t ============= CONFIGURATION OPTIONS =====================================\n"
       << "\t -c <filename> OR --config=<filename>\n"
       << "\t                        use this configuration file (default " << FrogAPI::defaultConfigFile("nld") << ")\n"
       << "\t                        you can use -c lang/config-file to select the config-file for an installed language 'lang'\n"
       << "\t --language <language-list>\n"
       << "\t                        Set the languages to be used in the tokenizer.\n"
       << "\t                        e.g. --language=nld,eng,por\n"
       << "\t                        The first language in the list will be the default. (default dutch).\n"
       << "\t                        IMPORTANT: frog can handle only one language at a time, determined by the config.\n"
       << "\t                        So other languages mentioned here will be tokenized correctly, but further handled as the configured language.\n"
       << "\t --override <section>.<parameter>=<value>\n"
       << "\t                        Override a configuration option, can be used multiple times\n"
       << "\t ============= OUTPUT OPTIONS ============================================\n"
       << "\t -o <outputfile>        Output Tab separated or JSON output to file, instead of default stdout\n"
       << "\t --nostdout             suppress Tabbed/JSON output to stdout\n"
       << "\t -X <xmlfile>           Output also to an XML file in FoLiA format\n"
       << "\t --id=<docid>           Document ID, used in FoLiA output. (Default 'untitled')\n"
       << "\t --allow-word-corrections         allow the tokenizer to correct <w> nodes. (FoLiA only)\n"
       << "\t --outputdir=<dir>      Output to dir, instead of default stdout\n"
       << "\t --xmldir=<dir>         Use 'dir' to output FoliA XML to.\n"
       << "\t --deep-morph           add deep morphological information to the output\n"
       << "\t --JSONout=n            Output JSON instead of Tabbed.\n"
       << "\t                        When n != 0, use it for pretty-printing the output. (default n=0) \n"
       << "\t ============= OTHER OPTIONS ============================================\n"
       << "\t -h or --help           give some help.\n"
       << "\t -V or --version        Show version info.\n"
       << "\t -d <debug level>  (for more verbosity)\n"
       << "\t --debug=<module><level>,<module><level>... (eg --debug=l5,n3) \n"
       << "\t                        Set debug value for Tagger (T), Tokenizer (t), Lemmatizer (l), Morphological Analyzer (a), Chunker (c),\n"
       << "\t                        Multi-Word Units (m), Named Entity Recognition (n), or Parser (p)\n"
       << "\t -S <port>              Run as server instead of reading from testfile\n"
#ifdef HAVE_OPENMP
       << "\t --threads=<n>          Use a maximum of 'n' threads. Default: 8. \n"
#endif
       << "\t                         (but always 1 for server mode)\n";
}

bool parse_args( TiCC::CL_Options& Opts,
		 FrogOptions& options,
		 TiCC::LogStream* theErrLog ){
  /// process the command line and fill FrogOptions to initialize the API
  /// also fill some globals we use for our own main.
  /*!
    \param Opts The command line options we have received
    \param options The FrogOptions structure we will fill
    \param theErrLog the stream to send error messages to
    return true on succes
  */
  options.command = Opts.toString();
  // is a language-list specified? Default is dutch
  string language;
  string languages;
  Opts.extract ("language", languages );
  string configFileName;
  if ( languages.empty() ){
    // ok no languages parameter.
    // use a (default) configfile. Dutch
    configFileName = FrogAPI::defaultConfigFile("nld");
    language = "nld";
    options.languages.insert( "nld" );
  }
  else {
    vector<string> lang_v = TiCC::split_at( languages, "," );
    if ( lang_v.empty() ){
      cerr<< "invalid value in --language=" << languages
	  << " option. " << endl;
      return false;
    }
    language = lang_v[0]; // the first mentioned is the default.
    for ( const auto& l : lang_v ){
      options.languages.insert( l );
    }
    if ( lang_v.size() > 1 ){
      cerr << "WARNING: you used the --language=" << languages << " option"
	   << " with more then one language " << endl
	   << "\t specified. These values will be handled to the tokenizer,"
	   << " but Frog"<< endl
	   << "\t will only handle the first language: " << language
	   << " for further processing!" << endl;
    }
    configFileName = FrogAPI::defaultConfigFile(language);
    if ( !TiCC::isFile( configFileName ) ){
      cerr << "configuration file: " << configFileName << " not found" << endl;
      cerr << "Did you correctly install the frogdata package for language="
	   << language << "?" << endl;
      configFileName = FrogAPI::defaultConfigFile();
      cerr << "using fallback configuration file: " << configFileName << endl;
    }
  }
  options.default_language = language;
  // override default config settings when a configfile is specified
  if ( !Opts.extract( 'c',  configFileName ) ){
    Opts.extract( "config",  configFileName );
  }
  if ( !TiCC::isFile( configFileName ) ){
    // maybe it is in the default dir?
    configFileName = FrogAPI::defaultConfigDir() + configFileName;
  }
  if ( configuration.fill( configFileName ) ){
    LOG << "config read from: " << configFileName << endl;
    string vers = configuration.lookUp( "version" );
    if ( !vers.empty() ){
      LOG << "configuration version = " << vers << endl;
    }
    string langs = configuration.getatt( "languages", "tokenizer" );
    if ( !langs.empty() ){
      vector<string> lang_v = TiCC::split_at( langs, "," );
      options.default_language = lang_v[0];
      for ( const auto& l : lang_v ){
	options.languages.insert( l );
      }
    }
  }
  else {
    cerr << "failed to read configuration from '" << configFileName << "' !!" << endl;
    cerr << "Did you correctly install the frogdata package for language="
	 << language << "?" << endl;
    return false;
  }
  if ( !languages.empty() ){
    configuration.setatt( "languages", languages, "tokenizer" );
  }
  string opt_val;
  // debug opts
  if ( Opts.extract ('d', opt_val) ) {
    if ( !TiCC::stringTo<int>( opt_val, options.debugFlag ) ){
      LOG << "-d value should be an integer" << endl;
      return false;
    }
    configuration.setatt( "debug", opt_val );
  }
  else {
    configuration.setatt( "debug", "0" );
  }
  bool old_mwu = Opts.extract("OLDMWU");
  if ( Opts.extract( "debug", opt_val ) ) {
    vector<string> vec = TiCC::split_at( opt_val, "," );
    for ( const auto& val : vec ){
      char mod = val[0];
      string value = val.substr(1);
      int dbval = 0;
      if ( !TiCC::stringTo<int>( value, dbval ) ){
	cerr << "expected integer value for --debug=" << mod << value << endl;
	return false;
      }
      switch ( mod ){
      case 'T':
	configuration.setatt( "debug", value, "tagger" );
	break;
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
	cerr << "unknown module code:'" << mod << "'" << endl;
	return false;
      }
    }
  }
  string redundancy;
  Opts.extract( 'T', redundancy );
  Opts.extract( "textredundancy", redundancy );
  if ( !redundancy.empty() ){
    if ( redundancy != "full"
	 && redundancy != "minimal"
	 && redundancy != "none" ){
      LOG << "unknown textredundancy level: " << redundancy << endl;
      return false;
    }
    options.textredundancy = redundancy;
  }
  options.doSentencePerLine = Opts.extract( 'n' );
  options.correct_words = Opts.extract( "allow-word-corrections" );
  options.doQuoteDetection = Opts.extract( 'Q' );
  if (  options.doQuoteDetection ){
    LOG << "Quote detection is NOT supported!" << endl;
    return false;
  }
  if ( Opts.extract( "skip", opt_val )) {
    string skip = opt_val;
    if ( skip.find_first_of("tT") != string::npos ){
      options.doTok = false;
    }
    if ( skip.find_first_of("lL") != string::npos ){
      options.doLemma = false;
    }
    if ( skip.find_first_of("aA") != string::npos ){
      options.doMorph = false;
    }
    if ( skip.find_first_of("mM") != string::npos ){
      if ( options.doAlpino ){
	LOG << "option skip=m conflicts with --alpino"
	    << endl;
	return false;
      }
      options.doMwu = false;
    }
    if ( skip.find_first_of("cC") != string::npos ){
      options.doIOB = false;
    }
    if ( skip.find_first_of("nN") != string::npos ){
      options.doNER = false;
    }
    if ( skip.find_first_of("gG") != string::npos ){
      options.doTagger = false;
    }
    if ( skip.find_first_of("pP") != string::npos ){
      if ( options.doAlpino ){
	LOG << "option skip=p conflicts with --alpino"
	    << endl;
	return false;
      }
      else if ( options.doMwu && !old_mwu ){
	LOG << " MWU disabled, because the Parser is deselected" << endl;
	options.doMwu = false;
      }
      options.doParse = false;
    }
    else if ( !options.doMwu && options.doParse ){
      LOG << " Parser disabled, because MWU is deselected" << endl;
      options.doParse = false;
    }
    Opts.remove("skip");
  };

  if ( Opts.extract( "deep-morph" ) ) {
    options.doDeepMorph = true;
    options.doMorph = true;
  }
  options.doRetry = Opts.extract( "retry" );
  options.noStdOut = Opts.extract( "nostdout" );
  Opts.extract( 'e', options.encoding );

  if ( Opts.extract( "max-parser-tokens", opt_val ) ){
    if ( !TiCC::stringTo<unsigned int>( opt_val, options.maxParserTokens ) ){
      LOG << "max-parser-tokens value should be an integer" << endl;
      return false;
    }
  }

  if ( Opts.extract( "ner-override", opt_val ) ){
    configuration.setatt( "ner_override", opt_val, "NER" );
  }
  options.doServer = Opts.extract('S', options.listenport );
  options.doJSONin = Opts.extract( "JSONin" );
  if ( options.doJSONin && !options.doServer ){
    LOG << "option JSONin is only allowed for server mode. (-S option)" << endl;
    return false;
  }
  options.doAlpino = Opts.extract("alpino", opt_val);
  if ( options.doAlpino ){
    if ( !opt_val.empty() ){
      if ( opt_val == "server" ){
	options.doAlpinoServer = true;
	configuration.setatt( "alpinoserver", "true", "parser" );
      }
      else {
	LOG << "unsupported value for --alpino: " << opt_val << endl;
	return false;
      }
    }
  }
  if ( options.doAlpino ){
    options.doParse = false;
    options.doMwu = false;
  }
#ifdef HAVE_OPENMP
  if ( options.doServer ) {
    // run in one thread in server mode, forking is too expensive for lots of small snippets
    options.numThreads =  1;
    Opts.extract( "threads", opt_val ); //discard threads option
  }
  else if ( Opts.extract( "threads", opt_val ) ){
    int num;
    if ( !TiCC::stringTo<int>( opt_val, num ) || num < 1 ){
      LOG << "threads value should be a positive integer" << endl;
      return false;
    }
    options.numThreads = num;
  }
#else
  if ( Opts.extract( "threads", opt_val ) ){
    LOG << "WARNING!\n---> There is NO OpenMP support enabled\n"
		    << "---> --threads=" << opt_val << " is ignored.\n"
		    << "---> Will continue on just 1 thread." << endl;
  }
#endif

  if ( Opts.extract( "keep-parser-files" ) ){
    LOG << "keep-parser-files option not longer supported. (ignored)" << endl;
  }
  if ( Opts.extract( "tmpdir" ) ){
    LOG << "tmpdir option not longer supported. (ignored)" << endl;
  }
  string TestFileName;
  if ( Opts.extract( "testdir", TestFileName ) ) {
    testDirName = TestFileName;
    if ( testDirName.back() != '/' ){
      testDirName += "/";
    }
    if ( !TiCC::isDir( testDirName ) ){
      LOG << "input dir " << testDirName << " not readable" << endl;
      return false;
    }
  }
  else if ( Opts.extract( 't', TestFileName ) ) {
    if ( !TiCC::isFile( TestFileName ) ){
      LOG << "input stream " << TestFileName << " is not readable" << endl;
      return false;
    }
  };
  wantOUT = false;
  if ( Opts.extract( "outputdir", outputDirName )) {
    if ( outputDirName.back() != '/' ){
      outputDirName += "/";
    }
    if ( !TiCC::createPath( outputDirName ) ){
      LOG << "output dir " << outputDirName << " not readable" << endl;
      return false;
    }
    wantOUT = true;
  }
  else if ( Opts.extract( 'o', outputFileName ) ){
    wantOUT = true;
  };
  string json_pps;
  options.doJSONout = Opts.extract( "JSONout", json_pps );
  if ( options.doJSONout ){
    if ( json_pps.empty() ){
      options.JSON_pp = 0;
    }
    else {
      int tmp;
      if ( !TiCC::stringTo<int>( json_pps, tmp )
	   || tmp < 0 ){
	LOG << "--JSONout value should be an integer >=0 " << endl;
	return false;
      }
      else {
	options.JSON_pp = tmp;
      }
    }
  }
  else {
    if ( options.doJSONin ){
      options.doJSONout = true;
    }
  }
  options.doXMLout = false;
  Opts.extract( "id", options.docid );
  if ( !options.docid.empty() ){
    if ( !folia::isNCName(options.docid) ){
      LOG << "Invalid value for 'id': " << options.docid << " (not a valid NCName)" << endl;
      return false;
    }
  }
  if ( Opts.extract( "xmldir", xmlDirName ) ){
    if ( xmlDirName.back() != '/' ){
      xmlDirName += "/";
    }
    if ( !TiCC::createPath( xmlDirName ) ){
      LOG << "XML output dir " << xmlDirName << " not readable" << endl;
      return false;
    }
    options.doXMLout = true;
  }
  else if ( Opts.extract('X', XMLoutFileName ) ){
    options.doXMLout = true;
  }

  options.doKanon = Opts.extract("KANON");
  options.test_API = Opts.extract("TESTAPI");

  options.doXMLin = false;
  if ( Opts.extract ('x', opt_val ) ){
    options.doXMLin = true;
    if ( !opt_val.empty() ){
      if ( !xmlDirName.empty() || !testDirName.empty() ){
	LOG << "-x may not provide a value when --testdir or --xmldir is provided" << endl;
	return false;
      }
      TestFileName = opt_val;
      if ( !TiCC::isFile( TestFileName ) ){
	LOG << "input stream " << opt_val << " is not readable" << endl;
	return false;
      }
    }
  }
  string textclass;
  string inputclass;
  string outputclass;
  Opts.extract( "textclass", textclass );
  Opts.extract( "inputclass", inputclass );
  Opts.extract( "outputclass", outputclass );
  if ( !textclass.empty() ){
    if ( !inputclass.empty() || !outputclass.empty() ){
      LOG << "when --textclass is specified, --inputclass or --outputclass may NOT be present." << endl;
      return false;
    }
    options.inputclass = textclass;
    options.outputclass = textclass;
    configuration.setatt( "inputclass", textclass );
    configuration.setatt( "outputclass", textclass );
  }
  else {
    if ( !inputclass.empty() ){
      options.inputclass = inputclass;
      configuration.setatt( "inputclass", inputclass );
      if ( outputclass.empty() ){
	options.outputclass = inputclass;
	configuration.setatt( "outputclass", inputclass );
      }
    }
    if ( !outputclass.empty() ){
      options.outputclass = outputclass;
      configuration.setatt( "outputclass", outputclass );
    }
  }
  if ( !XMLoutFileName.empty() && !testDirName.empty() ){
    LOG << "useless -X value" << endl;
    return false;
  }

  Opts.extract ("uttmarker", options.uttmark );
  if ( !testDirName.empty() ){
    if ( options.doXMLin ){
      fileNames = getFileNames( testDirName, ".xml" );
    }
    else {
      fileNames = getFileNames( testDirName, "" );
    }
    if ( fileNames.empty() ){
      LOG << "error: couldn't find any files in directory: "
		      << testDirName << endl;
      return false;
    }
  }
  if ( fileNames.empty() ){
    if ( !TestFileName.empty() ){
      if ( !TiCC::isFile( TestFileName ) ){
	LOG << "Input file not found: " << TestFileName << endl;
	return false;
      }
      fileNames.insert( TestFileName );
    }
    else {
      vector<string> mass = Opts.getMassOpts();
      for ( const auto& name : mass ){
	if ( !TiCC::isFile( name ) ){
	  LOG << "Input file not found: " << name << endl;
	  return false;
	}
	fileNames.insert( name );
      }
    }
  }
  if ( fileNames.size() > 1 ){
    if ( !XMLoutFileName.empty() ){
      LOG << "'-X " << XMLoutFileName
		      << "' is invalid for multiple inputfiles." << endl;
      return false;
    }
  }
  string overridestatement;
  while ( Opts.extract("override", overridestatement )) {
    vector<string> values;
    const int num = TiCC::split_at( overridestatement,values,  "=" );
    if ( num == 2 ) {
        vector<string> module_param;
        const int num2 = TiCC::split_at(values[0], module_param, "." );
        if (num2 == 2) {
            LOG << "Overriding configuration parameter " << module_param[0] << "." << module_param[1] << " with " << values[1] << endl;
            configuration.setatt( module_param[1] , values[1], module_param[0] );
        } else if (num2 == 1) {
            LOG << "Overriding configuration parameter " << module_param[0] << " with " << values[1] << endl;
            configuration.setatt( module_param[0] , values[1]);
        } else {
            LOG << "Invalid syntax for --override option" << endl;
        }
    } else {
        LOG << "Invalid syntax for --override option" << endl;
    }
  }
  if ( !Opts.empty() ){
    LOG << "unhandled commandline options: " << Opts.toString() << endl;

    return false;
  }
  return true;
}

static bool StillRunning = true;

void KillServerFun( int Signal ){
  if ( Signal == SIGTERM ){
    cerr << TiCC::Timer::now() << " KillServerFun caught a signal SIGTERM" << endl;
    sleep(5); // give children some spare time...
    StillRunning = false;
  }
}

unsigned long long random64(){
  /// generate a 64 bit random number
  static std::random_device rd;
  static std::uniform_int_distribution<unsigned long long> dis;
  static std::mt19937_64 gen(rd());
  return dis(gen);
}

string randnum( int len ){
  /// generate a rather random string of length \e len
  /*!
    \param len The length of the output string

    We generate a 64 bit random number an convert is to a string.
    Then this string is truncated at \em len characters. (which means that
    the result migth be NOT unique!)
  */
  stringstream ss;
  ss << random64() << endl;
  string result = ss.str();
  return result.substr(0,len);
}

int main(int argc, char *argv[]) {
  /// Frog's main program.
  cerr << "frog " << VERSION << " (c) CLST, ILK 1998 - 2021" << endl
       << "CLST  - Centre for Language and Speech Technology,"
       << "Radboud University" << endl
       << "ILK   - Induction of Linguistic Knowledge Research Group,"
       << "Tilburg University" << endl;
  cerr << "based on [" << Tokenizer::VersionName() << ", "
       << folia::VersionName() << ", "
       << Timbl::VersionName() << ", "
       << TiCCServer::VersionName() << ", "
       << Tagger::VersionName() << "]" << endl;
  TiCC::LogStream *theErrLog
    = new TiCC::LogStream( cerr, "frog-", StampMessage );
  ostream *the_dbg_stream = 0;
  TiCC::LogStream *theDbgLog = 0;
  std::ios_base::sync_with_stdio(false);
  FrogOptions options;
  string db_filename;
  string remove_command = "find frog.*.debug -mtime +1 -exec rm {} \\;";
  cerr << "removing old debug files using: '" << remove_command << "'" << endl;
  if ( system( remove_command.c_str() ) ){
  }
  try {
    TiCC::CL_Options Opts("c:e:o:t:T:x::X::nQhVd:S:",
			  "config:,testdir:,"
			  "textclass:,inputclass:,outputclass:,"
			  "uttmarker:,max-parser-tokens:,textredundancy:,"
			  "skip:,id:,outputdir:,xmldir:,tmpdir:,deep-morph,"
			  "help,language:,retry,nostdout,ner-override:,"
			  "debug:,keep-parser-files,version,threads:,alpino::,"
			  "override:,KANON,TESTAPI,debugfile:,JSONin,JSONout::,"
			  "allow-word-corrections,OLDMWU");
    Opts.init(argc, argv);
    if ( Opts.is_present('V' ) || Opts.is_present("version" ) ){
      // we already did show what we wanted.
      delete theErrLog;
      return EXIT_SUCCESS;
    }
    if ( Opts.is_present( 'h' )
	 || Opts.is_present( "help" ) ) {
      usage();
      delete theErrLog;
      return EXIT_SUCCESS;
    };
    Opts.extract( "debugfile", db_filename );
    if ( db_filename.empty() ){
      db_filename = "frog." + randnum(8) + ".debug";
    }
    the_dbg_stream = new ofstream( db_filename );
    theDbgLog = new TiCC::LogStream( *the_dbg_stream, "frog-", StampMessage );
    bool parsed = parse_args( Opts, options, theErrLog );
    if (!parsed) {
      throw runtime_error( "init failed" );
    }
    FrogAPI frog( options, configuration, theErrLog, theDbgLog );
    if ( !fileNames.empty() ) {
      string outPath = outputDirName;
      string xmlPath = xmlDirName;

      ostream *outS = 0;
      if ( !outputFileName.empty() ){
	if ( options.doRetry && TiCC::isFile( outputFileName ) ){
	  LOG << "retry, skip: " << outputFileName << " already exists" << endl;
	  return EXIT_SUCCESS;
	}
	if ( !TiCC::createPath( outputFileName ) ) {
	  LOG << "problem: unable to create outputfile: "
	      << outputFileName << endl;
	  return EXIT_FAILURE;
	}
        outS = new ofstream( outputFileName );
      }
      if ( fileNames.size() > 1 ){
	LOG << "start processing " << fileNames.size() << " files..." << endl;
      }
      for ( auto const& name : fileNames ){
	string testName = testDirName + name;
	if ( !TiCC::isFile( testName ) ){
	  LOG << "skip " << testName << " (file not found )"
	      << endl;
	  continue;
	}
	string outName;
	if ( outS == 0 ){
	  if ( wantOUT ){
	    if ( options.doXMLin ){
	      if ( !outPath.empty() ){
		outName = outPath + name + ".out";
	      }
	    }
	    else {
	      outName = outPath + name + ".out";
	    }
	    if ( options.doRetry && TiCC::isFile( outName ) ){
	      LOG << "retry, skip: " << outName << " already exists" << endl;
	      continue;
	    }
	    if ( !TiCC::createPath( outName ) ) {
	      LOG << "problem frogging: " << name << endl
			      << "unable to create outputfile: " << outName
			      << endl;
	      continue;
	    }
	    outS = new ofstream( outName );
	  }
	  else {
	    outS = &cout;
	  }
	}
	string xmlOutName = XMLoutFileName;
	if ( xmlOutName.empty() ){
	  if ( !xmlDirName.empty() ){
	    if ( name.rfind(".xml") == string::npos ){
	      xmlOutName = xmlPath + name + ".xml";
	    }
	    else {
	      xmlOutName = xmlPath + name;
	    }
	  }
	  else if ( options.doXMLout ){
	    xmlOutName = name + ".xml"; // do not clobber the inputdir!
	  }
	}
	if ( !xmlOutName.empty() ){
	  if ( options.doRetry && TiCC::isFile( xmlOutName ) ){
	    LOG << "retry, skip: " << xmlOutName << " already exists" << endl;
	    continue;
	  }
	  if ( !TiCC::createPath( xmlOutName ) ){
	    LOG << "problem frogging: " << name << endl
		<< "unable to create outputfile: " << xmlOutName
		<< endl;
	    continue;
	  }
	  else {
	    remove( xmlOutName.c_str() );
	  }
	}
	LOG << TiCC::Timer::now() << " Frogging " << testName << endl;
	if ( options.test_API ){
	  frog.run_api_tests( testName, *outS );
	}
	else {
	  folia::Document *result = 0;
	  try {
	    result = frog.FrogFile( testName, *outS );
	  }
	  catch ( exception& e ){
	    LOG << "problem frogging: " << name << endl
		<< e.what() << endl;
	    continue;
	  }
	  if ( !xmlOutName.empty() ){
	    if ( !result ){
	      LOG << "FAILED to create FoLiA??" << endl;
	    }
	    else {
	      result->save( xmlOutName );
	      LOG << "FoLiA stored in " << xmlOutName << endl;
	      delete result;
	    }
	  }
	  if ( !outName.empty() ){
	    LOG << "results stored in " << outName << endl;
	    if ( outS != &cout ){
	      delete outS;
	      outS = 0;
	    }
	  }
	  if ( !outputFileName.empty() ){
	    LOG << "results stored in " << outputFileName << endl;
	    if ( outS != &cout ){
	      delete outS;
	      outS = 0;
	    }
	  }
	}
      }
      LOG << TiCC::Timer::now() << " Frog finished" << endl;
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
      struct sigaction act;
      sigaction( SIGTERM, NULL, &act ); // get current action
      act.sa_handler = KillServerFun;
      act.sa_flags &= ~SA_RESTART;      // do not continue after SIGTERM
      sigaction( SIGTERM, &act, NULL );

      srand((unsigned)time(0));

      LOG << "Listening on port " << options.listenport << "\n";

      try {
	// Create the socket
	Sockets::ServerSocket server;
	if ( !server.connect( options.listenport ) ){
	  throw( runtime_error( "starting server on port " + options.listenport + " failed" ) );
	}
	if ( !server.listen( 5 ) ) {
	  // maximum of 5 pending requests
	  throw( runtime_error( "listen(5) failed" ) );
	}
	while ( StillRunning ) {
	  Sockets::ClientSocket conn;
	  if ( server.accept( conn ) ){
	    LOG << "New connection..." << endl;
	    int pid = fork();
	    if (pid < 0) {
	      string err = strerror(errno);
	      LOG << "ERROR on fork: " << err << endl;
	      throw runtime_error( "FORK failed: " + err );
	    }
	    else if (pid == 0)  {
	      frog.FrogServer( conn );
	      return EXIT_SUCCESS;
	    }
	  }
	  else {
	    throw( runtime_error( "Accept failed" ) );
	  }
	}
	LOG << TiCC::Timer::now() << " server terminated by SIGTERM" << endl;
      }
      catch ( exception& e ) {
	LOG << "Server error:" << e.what() << " Exiting." << endl;
	throw;
      }
    }
    else {
      // interactive mode
      frog.FrogInteractive();
    }
  }
  catch ( const TiCC::OptionError& e ){
    usage();
    return EXIT_FAILURE;
  }
  catch ( const exception& e ){
    LOG << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  delete theErrLog;
  delete theDbgLog;
  delete the_dbg_stream;

  if ( !db_filename.empty() ){
    ifstream test(db_filename,ifstream::ate);
    if ( test.tellg() > 0 ){
      cerr << "Some debugging information is available in: "
	   << db_filename << endl;
    }
    else {
      remove( db_filename.c_str() );
    }
  }
  return EXIT_SUCCESS;
}
