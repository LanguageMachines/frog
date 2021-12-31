/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2022
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
  cerr << "frog " << VERSION << " (c) CLST, ILK 1998 - 2022" << endl
       << "CLST  - Centre for Language and Speech Technology,"
       << "Radboud University" << endl
       << "ILK   - Induction of Linguistic Knowledge Research Group,"
       << "Tilburg University" << endl;
  cerr << "based on [" << Tokenizer::VersionName() << ", "
       << folia::VersionName() << ", "
       << Timbl::VersionName() << ", "
       << TiCCServer::VersionName() << ", "
       << Tagger::VersionName() << "]" << endl;
  TiCC::LogStream *theErrLog = 0;
  ostream *the_dbg_stream = 0;
  TiCC::LogStream *theDbgLog = 0;
  std::ios_base::sync_with_stdio(false);
  FrogOptions options;
  string db_filename;
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
      return EXIT_SUCCESS;
    }
    if ( Opts.is_present( 'h' )
	 || Opts.is_present( "help" ) ) {
      usage();
      return EXIT_SUCCESS;
    };
    string remove_command = "find frog.*.debug -mtime +1 -exec rm {} \\;";
    cerr << "removing old debug files using: '" << remove_command
	 << "'" << endl;
    if ( system( remove_command.c_str() ) ){
      // nothing
    }
    theErrLog = new TiCC::LogStream( cerr, "frog-", StampMessage );
    Opts.extract( "debugfile", db_filename );
    if ( db_filename.empty() ){
      db_filename = "frog." + randnum(8) + ".debug";
    }
    the_dbg_stream = new ofstream( db_filename );
    theDbgLog = new TiCC::LogStream( *the_dbg_stream, "frog-", StampMessage );
    FrogAPI frog( Opts, theErrLog, theDbgLog );
    if ( !frog.options.fileNames.empty() ) {
      frog.run_on_files();
    }
    else if ( frog.options.doServer ) {
      if ( frog.run_a_server() ){
	return EXIT_SUCCESS;
      }
      else {
	return EXIT_FAILURE;
      }
    }
    else {
      // interactive mode
      frog.run_interactive();
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
