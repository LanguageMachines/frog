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

#include "frog/FrogAPI.h"

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <algorithm>
#include <string>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <vector>
#include "unicode/schriter.h"
#include "config.h"
#ifdef HAVE_OPENMP
#include <omp.h>
#endif

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

#include "ticcutils/PrettyPrint.h"
#include "ticcutils/FileUtils.h"

// individual module headers
#include "frog/Frog-util.h"
#include "frog/ucto_tokenizer_mod.h"
#include "frog/mblem_mod.h"
#include "frog/mbma_mod.h"
#include "frog/mwu_chunker_mod.h"
#include "frog/cgn_tagger_mod.h"
#include "frog/iob_tagger_mod.h"
#include "frog/ner_tagger_mod.h"
#include "frog/Parser.h"
#include "frog/AlpinoParser.h"
#include "ticcutils/json.hpp"

using namespace std;
using namespace icu;
using namespace nlohmann;
using namespace Tagger;
using TiCC::operator<<;

#define LOG *TiCC::Log(theErrLog)
#define DBG *TiCC::Log(theDbgLog)

/// the FoLiA setname for languages
const string ISO_SET = "http://raw.github.com/proycon/folia/master/setdefinitions/iso639_3.foliaset";

const char *homedir = getenv("HOME") ? getenv("HOME") : getpwuid(getuid())->pw_dir; //never NULL
const char *xdgconfighome = getenv("XDG_CONFIG_HOME"); //may be NULL

string localConfigDir = ((xdgconfighome != NULL) ? string(xdgconfighome) : string(homedir) + "/.config") + "/" + PACKAGE + "/";
string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/";

string FrogAPI::defaultConfigDir( const string& language ){
  /// return the default location of the configuration files
  /*!
    \param language use this to find the configuration for this language
    \return a string representing the (language specific) path
  */
  //! The global location of the configuration files is determined at installation
  //! of the package. Users can place configurations in $XDG_CONFIG_HOME/frog (i.e. ~/.config/frog)
  //! which will take precendence over the global configuration.
  //! All languages are supposed to be in sub-directories
  if ( language.empty() ){
    if (TiCC::isDir(localConfigDir)) {
        return localConfigDir;
    }
    else {
      return configDir;
    }
  }
  else {
    if (TiCC::isDir(localConfigDir + language)) {
        return localConfigDir + language +"/";
    }
    else {
      return configDir+language+"/";
    }
  }
}

string FrogAPI::defaultConfigFile( const string& language ){
  /// return the filename for the default configuration
  /*!
    \param language use this to find the configuration for this language
    \return a string representing (language specific) default filename
  */
  //! The location of the configuration files is determined at installation
  //! of the package. All languages are supposed to be in sub-directories
  return defaultConfigDir( language ) + "frog.cfg";
}

FrogOptions::FrogOptions():
  doTok(true),
  doLemma(true),
  doMbma(true),
  doDeepMorph(false),
  doCompounds(false),
  doMwu(true),
  doIOB(true),
  doNER(true),
  doParse(true),
  doTagger(true),
  doSentencePerLine(false),
  doQuoteDetection(false),
  doRetry(false),
  noStdOut(false),
  doXMLin(false),
  doXMLout(false),
  wantOUT(false),
  doJSONin(false),
  doJSONout(false),
  doServer(false),
  doKanon(false),
  test_API(false),
  hide_timers(false),
  interactive(false),
  doAlpinoServer(false),
  doAlpino(false),
  do_und_language(false),
  do_language_detection(false),
  numThreads(1),
  debugFlag(0),
  JSON_pp(0),
  uttmark("<utt>"),
  listenport("void"),
  docid("untitled"),
  inputclass("current"),
  outputclass("current"),
  textredundancy("minimal"),
  correct_words(false),
  maxParserTokens(500) // 500 words in a sentence is already insane
  // needs about 16 Gb memory to parse!
  // set tot 0 for unlimited
{
#ifdef HAVE_OPENMP
  numThreads = min<int>( 8, omp_get_max_threads() ); // ok, don't overdo
#endif
}

void FrogAPI::test_version( const TiCC::Configuration& configuration,
			    const string& module,
			    double minimum ){
  /// check if a module in the Frog Configuration is at least at the
  /// requested version level
  /*!
    \param module whick module to check
    \param minumum the minimum level
    This function will throw on a mismatch
  */
  string version = configuration.lookUp( "version", module );
  double v = 0.0;
  if ( !version.empty() ){
    if ( !TiCC::stringTo( version, v ) ){
      v = 0.5;
    }
  }
  if ( module == "IOB" ){
    if ( v < minimum ){
      LOG << "[[" << module << "]] Wrong FrogData!. "
	  << "Expected version " << minimum << " or higher for module: "
	  << module << endl;
      if ( version.empty() ) {
	LOG << "but no version info was found!." << endl;
      }
      else {
	LOG << "but found version " << v << endl;
      }
      throw runtime_error( "Frog initialization failed" );
    }
  }
  else if ( module == "NER" ){
    if ( v < minimum ){
      LOG << "[[" << module << "]] Wrong FrogData!. "
	  << "Expected version " << minimum << " or higher for module: "
	  << module << endl;
      if ( version.empty() ) {
	LOG << "but no version info was found!." << endl;
      }
      else {
	LOG << "but found version " << v << endl;
      }
      throw runtime_error( "Frog initialization failed" );
    }
  }
  else {
    throw logic_error( "unknown module:" + module );
  }
}

bool FrogAPI::parse_language_option( const string& languages ){
  if ( languages.empty() ){
    return true;
  }
  LOG << "check: " << languages << endl;
  vector<string> lang_v = TiCC::split_at( languages, "," );
  if ( lang_v.empty() ){
    LOG << "invalid value in --language=" << languages
	<< " option. " << endl;
    return false;
  }
  set<string> ucto_languages = Tokenizer::Setting::installed_languages();
  set<string> rejected;
  auto l = lang_v.begin();
  while ( l != lang_v.end() ){
    if ( *l == "und" ){
      options.do_und_language = true;
      l = lang_v.erase(l);
    }
    else if ( ucto_languages.find( *l ) == ucto_languages.end() ){
      rejected.insert( *l );
      ++l;
    }
    else {
      ++l;
    }
  }
  if ( !rejected.empty() ){
    LOG << "unsupported languages: " << rejected << endl;
    LOG << "available languages: " << ucto_languages << endl;
    return false;
  }
  options.languages = lang_v;
  options.default_language = options.languages[0];  // the first mentioned is the default.
  return true;
}

bool FrogAPI::collect_options( TiCC::CL_Options& Opts,
			       TiCC::Configuration& configuration,
			       TiCC::LogStream* theErrLog ){
  /// process the command line and fill FrogOptions to initialize the API
  /*!
    \param Opts The command line options we have received
    \param options The FrogOptions structure we will fill
    \param theErrLog the stream to send error messages to
    return true on succes
  */
  options.command = Opts.toString();

  // get the (optional) configuration file
  string configFileName;
  if ( !Opts.extract( 'c',  configFileName ) ){
    Opts.extract( "config",  configFileName );
  }
  // is a language-list specified? Default is dutch
  string languages;
  Opts.extract ("language", languages );
  if ( configFileName.empty() ){
    if ( languages.empty() ){
      // ok no config or languages parameter.
      // use a (default) configfile. Dutch
      configFileName = FrogAPI::defaultConfigFile("nld");
      options.default_language = "nld";
      options.languages.push_back( "nld" );
    }
    else {
      // no config, but languages
      if ( !parse_language_option( languages ) ){
	return false;
      }
      if ( options.languages.size() > 1 ){
	cerr << "WARNING: you used the --language=" << languages << " option"
	     << " with more then one language " << endl
	     << "\t specified. These values will be handled to the tokenizer,"
	     << " but Frog"<< endl
	     << "\t will only handle the first language: '"
	     << options.default_language
	     << "' for further processing!" << endl;
	options.do_language_detection = true;
      }
      configFileName = FrogAPI::defaultConfigFile(options.default_language);
      if ( !TiCC::isFile( configFileName ) ){
	cerr << "configuration file: " << configFileName << " not found" << endl;
	cerr << "Did you correctly install the frogdata package for language="
	     << options.default_language << "?" << endl;
	return false;
      }
    }
  }
  LOG << "requested configuration: " << configFileName << endl;
  if ( !TiCC::isFile( configFileName ) && !(configFileName.rfind("/",0) == 0)){
    // if only a suffix was specified, look in the local resp. global defaults dirs:
    string localConfigFileName = localConfigDir + configFileName;
    if (TiCC::isFile( localConfigFileName )) {
        configFileName = localConfigFileName;
    }
    else {
      LOG << "  not found locally (" << configFileName << ")" << endl;
      //global (final fallback)
      configFileName = configDir + configFileName;
      if (!TiCC::isFile( configFileName )) {
	LOG << "  not found globally (" << configFileName << ")" << endl;
      }
    }
  }
  if ( configuration.fill( configFileName ) ){
    LOG << "config read from: " << configFileName << endl;
    string vers = configuration.lookUp( "version" );
    if ( !vers.empty() ){
      LOG << "configuration version = " << vers << endl;
    }
    string tmp = configuration.lookUp( "languages", "tokenizer" );
    // We will override the language options from the commandline
    // with those from the config file (if available)
    if ( tmp.empty() ){
      // not in the config, so use the commandline then
      tmp = languages;
    }
    if ( !parse_language_option( tmp ) ){
      return false;
    }
  }
  else {
    cerr << "failed to read configuration from '" << configFileName << "' !!" << endl;
    cerr << "Did you correctly install the frogdata package for language="
	 << options.default_language << "?" << endl;
    return false;
  }
  string conf_languages = TiCC::join( options.languages, "," );
  if ( options.do_und_language ){
    conf_languages += ",und";
  }
  if ( !conf_languages.empty() ){
    LOG << "SET config " << conf_languages << endl;
    configuration.setatt( "languages", conf_languages, "tokenizer" );
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
  options.doAlpino = Opts.extract("alpino", opt_val);
  if ( options.doAlpino ){
    options.doParse = false;
    options.doMwu = false;
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
  if ( Opts.extract( "skip", opt_val )) {
    string skip = opt_val;
    if ( skip.find_first_of("tT") != string::npos ){
      options.doTok = false;
    }
    if ( skip.find_first_of("lL") != string::npos ){
      options.doLemma = false;
    }
    if ( skip.find_first_of("aA") != string::npos ){
      options.doMbma = false;
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
    if ( !options.doMbma ){
      LOG << " Option --deep-morph NOT possible, because of --skip=a" << endl;
      return false;
    }
    options.doDeepMorph = true;
  }
  if ( Opts.extract( "compounds" ) ){
    if ( !options.doMbma ){
      LOG << " Option --compounds NOT possible, because of --skip=a" << endl;
      return false;
    }
    options.doCompounds = true;
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
    if ( options.doServer ){
      LOG << "a testdir is not possible when running a Server" << endl;
      return false;
    }
    options.testDirName = TestFileName;
    if ( options.testDirName.back() != '/' ){
      options.testDirName += "/";
    }
    if ( !TiCC::isDir( options.testDirName ) ){
      LOG << "input dir " << options.testDirName << " not readable" << endl;
      return false;
    }
  }
  else if ( Opts.extract( 't', TestFileName ) ) {
    if ( options.doServer ){
      LOG << "an input file is not possible when running a Server" << endl;
      return false;
    }
    if ( !TiCC::isFile( TestFileName ) ){
      LOG << "input stream " << TestFileName << " is not readable" << endl;
      return false;
    }
  };
  options.wantOUT = false;
  if ( Opts.extract( "outputdir", options.outputDirName )) {
    if ( options.doServer ){
      LOG << "an outputdir is not possible when running a Server" << endl;
      return false;
    }
    if ( options.outputDirName.back() != '/' ){
      options.outputDirName += "/";
    }
    if ( !TiCC::createPath( options.outputDirName ) ){
      LOG << "output dir " << options.outputDirName << " not readable" << endl;
      return false;
    }
    options.wantOUT = true;
  }
  else if ( Opts.extract( 'o', options.outputFileName ) ){
    options.wantOUT = true;
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
  if ( Opts.extract( "xmldir", options.xmlDirName ) ){
    if ( options.xmlDirName.back() != '/' ){
      options.xmlDirName += "/";
    }
    if ( !TiCC::createPath( options.xmlDirName ) ){
      LOG << "XML output dir " << options.xmlDirName << " not readable" << endl;
      return false;
    }
    options.doXMLout = true;
  }
  else if ( Opts.extract('X',options.XMLoutFileName ) ){
    options.doXMLout = true;
  }

  options.doKanon = Opts.extract("KANON");
  options.test_API = Opts.extract("TESTAPI");

  options.doXMLin = false;
  if ( Opts.extract ('x', opt_val ) ){
    options.doXMLin = true;
    if ( !opt_val.empty() ){
      if ( !options.xmlDirName.empty() || !options.testDirName.empty() ){
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
  if ( !options.XMLoutFileName.empty() && !options.testDirName.empty() ){
    LOG << "useless -X value" << endl;
    return false;
  }

  Opts.extract ("uttmarker", options.uttmark );
  if ( !options.testDirName.empty() ){
    if ( options.doXMLin ){
      options.fileNames = getFileNames( options.testDirName, ".xml" );
    }
    else {
      options.fileNames = getFileNames( options.testDirName, "" );
    }
    if ( options.fileNames.empty() ){
      LOG << "error: couldn't find any files in directory: "
		      << options.testDirName << endl;
      return false;
    }
  }
  if ( options.fileNames.empty() ){
    if ( !TestFileName.empty() ){
      if ( !TiCC::isFile( TestFileName ) ){
	LOG << "Input file not found: " << TestFileName << endl;
	return false;
      }
      options.fileNames.insert( TestFileName );
    }
    else {
      vector<string> mass = Opts.getMassOpts();
      for ( const auto& name : mass ){
	if ( !TiCC::isFile( name ) ){
	  LOG << "Input file not found: " << name << endl;
	  return false;
	}
	options.fileNames.insert( name );
      }
    }
  }
  if ( options.fileNames.size() > 1 ){
    if ( options.doServer ){
      LOG << "specifying inputfiles not possible when running a Server" << endl;
      return false;
    }
    if ( !options.XMLoutFileName.empty() ){
      LOG << "'-X " << options.XMLoutFileName
	  << "' is invalid for multiple inputfiles." << endl;
      return false;
    }
  }
  string overridestatement;
  while ( Opts.extract("override", overridestatement )) {
    vector<string> values = TiCC::split_at( overridestatement, "=" );
    const int num = values.size();
    if ( num == 2 ) {
      vector<string> module_param = TiCC::split_at(values[0], "." );
      const int num2 = module_param.size();
      if (num2 == 2) {
	LOG << "Overriding configuration parameter " << module_param[0] << "." << module_param[1] << " with " << values[1] << endl;
	configuration.setatt( module_param[1] , values[1], module_param[0] );
      }
      else if (num2 == 1) {
	LOG << "Overriding configuration parameter " << module_param[0] << " with " << values[1] << endl;
	configuration.setatt( module_param[0] , values[1]);
      }
      else {
	LOG << "Invalid syntax for --override option" << endl;
      }
    }
    else {
      LOG << "Invalid syntax for --override option" << endl;
    }
  }
  if ( !Opts.empty() ){
    LOG << "unhandled commandline options: " << Opts.toString() << endl;

    return false;
  }
  if ( options.doTagger && !configuration.hasSection("tagger") ){
    LOG << "Missing [[tagger]] section in config file." << endl;
    LOG << "cannot run without a tagger!" << endl;
    return false;
  }
  if ( options.doTok && !configuration.hasSection("tokenizer") ){
    LOG << "Missing [[tokenizer]] section in config file." << endl;
    LOG << "Disabled the tokenizer." << endl;
    options.doTok = false;
  }
  if ( options.doMbma && !configuration.hasSection("mbma") ){
    LOG << "Missing [[mbma]] section in config file." << endl;
    LOG << "Disabled the Morhological analyzer." << endl;
    options.doMbma = false;
  }
  if ( options.doIOB ){
    if ( !configuration.hasSection("IOB") ){
      LOG << "Missing [[IOB]] section in config file." << endl;
      LOG << "Disabled the IOB Chunker." << endl;
      options.doIOB = false;
    }
    else {
      test_version( configuration, "IOB", 2.0 );
    }
  }
  if ( options.doNER ) {
    if ( !configuration.hasSection("NER") ){
      LOG << "Missing [[NER]] section in config file." << endl;
      LOG << "Disabled the NER." << endl;
      options.doNER = false;
    }
    else {
      test_version( configuration, "NER", 2.0 );
    }
  }
  if ( options.doMwu && !configuration.hasSection("mwu") ){
    LOG << "Missing [[mwu]] section in config file." << endl;
    LOG << "Disabled the Multi Word Unit." << endl;
    LOG << "Also disabled the parser." << endl;
    options.doMwu = false;
    options.doParse = false;
  }
  if ( options.doParse && !configuration.hasSection("parser") ){
    LOG << "Missing [[parser]] section in config file." << endl;
    LOG << "Disabled the parser." << endl;
    options.doParse = false;
  }
  if ( !options.outputFileName.empty() ){
    if ( options.doRetry && TiCC::isFile( options.outputFileName ) ){
      LOG << "retry, skip: " << options.outputFileName << " already exists" << endl;
      return EXIT_SUCCESS;
    }
    if ( !TiCC::createPath( options.outputFileName ) ) {
      LOG << "problem: unable to create outputfile: "
	  << options.outputFileName << endl;
      return EXIT_FAILURE;
    }
    outS = new ofstream( options.outputFileName );
  }
  return true;
}

FrogAPI::FrogAPI( TiCC::CL_Options& Opts,
		  TiCC::LogStream *err_log,
		  TiCC::LogStream *dbg_log ):
  outS(0),
  theErrLog(err_log),
  theDbgLog(dbg_log),
  myMbma(0),
  myMblem(0),
  myMwu(0),
  myParser(0),
  myCGNTagger(0),
  myIOBTagger(0),
  myNERTagger(0),
  tokenizer(0)
{
  /// Initialize an FrogAPI class
  /*!
    \param Opts CommandLine Options as found by parsing the command line
    \param err_log A LogStream for error messages
    \param dbg_log A LogStream for debugging purposes

    This will throw on problems with 'Opts'

    Otherwise a fully instantiated Frog will be available for further use
  */
  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  TiCC::Configuration configuration;
  bool parsed = collect_options( Opts, configuration, theErrLog );
  if (!parsed) {
    throw runtime_error( "init failed" );
  }
  run_api( configuration );
}

void FrogAPI::run_api( const TiCC::Configuration& configuration ){
  if ( options.doServer ){
    // we use fork(). omp (GCC version) doesn't do well when omp is used
    // before the fork!
    // see: http://bisqwit.iki.fi/story/howto/openmp/#OpenmpAndFork
    tokenizer = new UctoTokenizer(theErrLog,theDbgLog);
    bool stat = tokenizer->init( configuration );
    if ( stat ){
      tokenizer->setPassThru( !options.doTok );
      tokenizer->setDocID( options.docid );
      tokenizer->setSentencePerLineInput( options.doSentencePerLine );
      tokenizer->setQuoteDetection( options.doQuoteDetection );
      tokenizer->setInputEncoding( options.encoding );
      tokenizer->setInputXml( options.doXMLin );
      tokenizer->setUttMarker( options.uttmark );
      tokenizer->setInputClass( options.inputclass );
      tokenizer->setOutputClass( options.outputclass );
      tokenizer->setWordCorrection( options.correct_words );
      tokenizer->setUndLang( options.do_und_language );
      tokenizer->setLangDetection( options.do_language_detection );
      myCGNTagger = new CGNTagger( theErrLog,theDbgLog );
      stat = myCGNTagger->init( configuration );
      if ( stat ){
	UnicodeString u_mark = TiCC::UnicodeFromUTF8( options.uttmark );
	myCGNTagger->set_eos_mark( u_mark );
	if ( options.doIOB ){
	  myIOBTagger = new IOBTagger( theErrLog, theDbgLog );
	  stat = myIOBTagger->init( configuration );
	  if ( stat ){
	    myIOBTagger->set_eos_mark( u_mark );
	  }
	}
	if ( stat && options.doNER ){
	  myNERTagger = new NERTagger( theErrLog, theDbgLog );
	  stat = myNERTagger->init( configuration );
	  if ( stat ){
	    myNERTagger->set_eos_mark( u_mark );
	  }
	}
	if ( stat && options.doLemma ){
	  myMblem = new Mblem(theErrLog,theDbgLog);
	  stat = myMblem->init( configuration );
	}
	if ( stat && options.doMbma ){
	  myMbma = new Mbma(theErrLog,theDbgLog);
	  stat = myMbma->init( configuration );
	  if ( stat ) {
	    if ( options.doDeepMorph ){
	      myMbma->setDeepMorph(true);
	    }
	  }
	}
	if ( stat && options.doAlpino ){
	  myParser = new AlpinoParser(theErrLog,theDbgLog);
	  stat = myParser->init( configuration );
	}
	else if ( stat && options.doMwu ){
	  myMwu = new Mwu(theErrLog,theDbgLog);
	  stat = myMwu->init( configuration );
	  if ( stat && options.doParse ){
	    myParser = new Parser(theErrLog,theDbgLog);
	    stat = myParser->init( configuration );
	  }
	}
      }
    }
    if ( !stat ){
      LOG << "Frog initialization failed." << endl;
      throw runtime_error( "Frog initialization failed" );
    }
  }
  else {
#ifdef HAVE_OPENMP
    omp_set_num_threads( options.numThreads );
    int curt = omp_get_max_threads();
    if ( curt != options.numThreads ){
      LOG << "attempt to set to " << options.numThreads
		      << " threads FAILED, running on " << curt
		      << " threads instead" << endl;
    }
    else if ( options.debugFlag ){
      DBG << "running on " << curt << " threads" << endl;
    }

#endif

    bool tokStat = true;
    string tokWhat;
    bool lemStat = true;
    string lemWhat;
    bool mwuStat = true;
    string mwuWhat;
    bool mbaStat = true;
    string mbaWhat;
    bool parStat = true;
    string parWhat;
    bool tagStat = true;
    string tagWhat;
    bool iobStat = true;
    string iobWhat;
    bool nerStat = true;
    string nerWhat;

#pragma omp parallel sections
    {
#pragma omp section
      {
	try {
	  tokenizer = new UctoTokenizer(theErrLog,theDbgLog);
	  tokStat = tokenizer->init( configuration );
	}
	catch ( const exception& e ){
	  tokWhat = e.what();
	  tokStat = false;
	}
	if ( tokStat ){
	  tokenizer->setPassThru( !options.doTok );
	  tokenizer->setDocID( options.docid );
	  tokenizer->setSentencePerLineInput( options.doSentencePerLine );
	  tokenizer->setQuoteDetection( options.doQuoteDetection );
	  tokenizer->setInputEncoding( options.encoding );
	  tokenizer->setInputXml( options.doXMLin );
	  tokenizer->setUttMarker( options.uttmark );
	  tokenizer->setInputClass( options.inputclass );
	  tokenizer->setOutputClass( options.outputclass );
	  tokenizer->setWordCorrection( options.correct_words );
	  tokenizer->setUndLang( options.do_und_language );
	  tokenizer->setLangDetection( options.do_language_detection );
	}
      }
#pragma omp section
      {
	if ( options.doLemma ){
	  try {
	    myMblem = new Mblem(theErrLog,theDbgLog);
	    lemStat = myMblem->init( configuration );
	  }
	  catch ( const exception& e ){
	    lemWhat = e.what();
	    lemStat = false;
	  }
	}
      }
#pragma omp section
      {
	if ( options.doMbma ){
	  try {
	    myMbma = new Mbma(theErrLog,theDbgLog);
	    mbaStat = myMbma->init( configuration );
	    if ( options.doDeepMorph ){
	      myMbma->setDeepMorph(true);
	    }
	  }
	  catch ( const exception& e ){
	    mbaWhat = e.what();
	    mbaStat = false;
	  }
	}
      }
#pragma omp section
      {
	try {
	  myCGNTagger = new CGNTagger( theErrLog, theDbgLog );
	  tagStat = myCGNTagger->init( configuration );
	}
	catch ( const exception& e ){
	  tagWhat = e.what();
	  tagStat = false;
	}
      }
#pragma omp section
      {
	if ( options.doIOB ){
	  try {
	    myIOBTagger = new IOBTagger( theErrLog, theDbgLog );
	    iobStat = myIOBTagger->init( configuration );
	  }
	  catch ( const exception& e ){
	    iobWhat = e.what();
	    iobStat = false;
	  }
	}
      }
#pragma omp section
      {
	if ( options.doNER ){
	  try {
	    myNERTagger = new NERTagger( theErrLog, theDbgLog );
	    nerStat = myNERTagger->init( configuration );
	  }
	  catch ( const exception& e ){
	    nerWhat = e.what();
	    nerStat = false;
	  }
	}
      }
#pragma omp section
      {
	if ( options.doAlpino ){
	  TiCC::Timer initTimer;
	  initTimer.start();
	  try {
	    myParser = new AlpinoParser( theErrLog, theDbgLog );
	    parStat = myParser->init( configuration );
	    initTimer.stop();
	    LOG << "init Parse took: " << initTimer << endl;
	  }
	  catch ( const exception& e ){
	    parWhat = e.what();
	    parStat = false;
	  }
	}
	else if ( options.doMwu ){
	  try {
	    myMwu = new Mwu( theErrLog, theDbgLog );
	    mwuStat = myMwu->init( configuration );
	    if ( mwuStat && options.doParse ){
	      TiCC::Timer initTimer;
	      initTimer.start();
	      try {
		myParser = new Parser( theErrLog, theDbgLog );
		parStat = myParser->init( configuration );
		initTimer.stop();
		LOG << "init Parse took: " << initTimer << endl;
	      }
	      catch ( const exception& e ){
		parWhat = e.what();
		parStat = false;
	      }
	    }
	  }
	  catch ( const exception& e ){
	    mwuWhat = e.what();
	    mwuStat = false;
	  }
	}
      }
    }   // end omp parallel sections
    if ( ! ( tokStat && iobStat && nerStat && tagStat && lemStat
	     && mbaStat && mwuStat && parStat ) ){
      string out = "Initialization failed for: ";
      if ( !tokStat ){
	out += "[tokenizer] " + tokWhat;
      }
      if ( !tagStat ){
	out += "[tagger] " + tagWhat;
      }
      if ( !iobStat ){
	out += "[IOB] " + iobWhat;
      }
      if ( !nerStat ){
	out += "[NER] " + nerWhat;
      }
      if ( !lemStat ){
	out += "[lemmatizer] " + lemWhat;
      }
      if ( !mbaStat ){
	out += "[morphology] " + mbaWhat;
      }
      if ( !mwuStat ){
	out += "[multiword unit] " + mwuWhat;
      }
      if ( !parStat ){
	out += "[parser] " + parWhat;
      }
      LOG << out << endl;
      throw runtime_error( "Frog init failed" );
    }
  }
  LOG << TiCC::Timer::now() <<  " Initialization done." << endl;
}


FrogAPI::~FrogAPI() {
  /// Destructor. Clears all resources
  delete myMbma;
  delete myMblem;
  delete myMwu;
  delete myCGNTagger;
  delete myIOBTagger;
  delete myNERTagger;
  delete myParser;
  delete tokenizer;
}

void FrogAPI::run_on_files(){
  string outPath = options.outputDirName;
  string xmlPath = options.xmlDirName;

  if ( options.fileNames.size() > 1 ){
    LOG << "start processing " << options.fileNames.size() << " files..." << endl;
  }
  for ( auto const& name : options.fileNames ){
    string testName = options.testDirName + name;
    if ( !TiCC::isFile( testName ) ){
      LOG << "skip " << testName << " (file not found )"
	  << endl;
      continue;
    }
    string outName;
    if ( outS == 0 ){
      if ( options.wantOUT ){
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
    string xmlOutName = options.XMLoutFileName;
    if ( xmlOutName.empty() ){
      if ( !options.xmlDirName.empty() ){
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
      run_api_tests( testName );
    }
    else {
      folia::Document *result = 0;
      try {
	result = FrogFile( testName );
      }
      catch ( exception& e ){
	LOG << "problem frogging: " << name << endl
	    << e.what() << endl;
	continue;
      }
      if ( !xmlOutName.empty() ){
	if ( !result ){
	  LOG << "FAILED to create FoLiA: " << xmlOutName << endl;
	}
	else {
	  result->save( xmlOutName, options.doKanon );
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
      if ( !options.outputFileName.empty() ){
	LOG << "results stored in " << options.outputFileName << endl;
	if ( outS != &cout ){
	  delete outS;
	  outS = 0;
	}
      }
    }
  }
  LOG << TiCC::Timer::now() << " Frog finished" << endl;
}

static bool StillRunning = true;

void KillServerFun( int Signal ){
  if ( Signal == SIGTERM ){
    cerr << TiCC::Timer::now() << " KillServerFun caught a signal SIGTERM" << endl;
    sleep(5); // give children some spare time...
    StillRunning = false;
  }
}

bool FrogAPI::run_a_server(){
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
	LOG << "New connection, socketid=" << conn.getSockId() << endl;
	int pid = fork();
	if (pid < 0) {
	  string err = strerror(errno);
	  LOG << "ERROR on fork: " << err << endl;
	  throw runtime_error( "FORK failed: " + err );
	}
	else if (pid == 0)  {
	  FrogServer( conn );
	  return true;
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
  return true;
}

folia::processor *FrogAPI::add_provenance( folia::Document& doc ) const {
  /// add Frog provenance information to a FoLiA::Document.
  /*!
    \param doc The folia::Document.
    \return an instantiated folia::processor associated with Frog
   */
  string _label = "frog";
  vector<folia::processor *> procs = doc.get_processors_by_name( _label );
  if ( !procs.empty() ){
    cerr << "unable to run frog on already frogged documents!" << endl;
    exit(1);
  }
  folia::processor *proc = doc.get_processor( _label );
  if ( !proc ){
    folia::KWargs args;
    args["name"] = _label;
    args["generate_id"] = "auto()";
    args["command"] = "frog " + options.command;
    args["version"] = PACKAGE_VERSION;
    args["generator"] = "yes";
    proc = doc.add_processor( args );
    proc->get_system_defaults();
  }
  if ( options.debugFlag > 4 ){
    DBG << "add_provenance(), using processor: " << proc->id() << endl;
  }
  tokenizer->add_provenance( doc, proc ); // unconditional
  if ( options.doTagger ){
    myCGNTagger->add_provenance( doc, proc );
  }
  if ( options.doLemma ){
    myMblem->add_provenance( doc, proc );
  }
  if ( options.doMbma ){
    myMbma->add_provenance( doc, proc );
  }
  if ( options.doIOB ){
    myIOBTagger->add_provenance( doc, proc );
  }
  if ( options.doNER ){
    myNERTagger->add_provenance( doc, proc );
  }
  if ( options.doMwu ){
    myMwu->add_provenance( doc, proc );
  }
  if ( options.doAlpino || options.doParse ){
    myParser->add_provenance( doc, proc );
  }
  return proc;
}

folia::FoliaElement* FrogAPI::start_document( const string& id,
					      folia::Document *& doc ) const {
  /// Initialize a FoLiA::Document
  /*!
    \param id The Document ID to use
    \param doc The created folia::Document
    \return a pointer to the top \<text\> node of the Document.
   */
  doc = new folia::Document( "xml:id='" + id + "'" );
  doc->addStyle( "text/xsl", "folia.xsl" );
  if ( options.debugFlag > 1 ){
    DBG << "start document!!!" << endl;
  }
  add_provenance( *doc );
  folia::KWargs args;
  args["xml:id"] = doc->id() + ".text";
  return doc->setTextRoot( args );
}

void FrogAPI::append_to_sentence( folia::Sentence *sent,
				  const frog_data& fd ) const {
  /// add the results from frogging to a folia::Sentence
  /*!
    \param sent The sentence to add to
    \param fd The frog_data structure which holds all results

   */
  vector<folia::Word*> wv = tokenizer->add_words( sent, fd );
  string la;
  if ( sent->has_annotation<folia::LangAnnotation>() ){
    la = sent->annotation<folia::LangAnnotation>()->cls();
  }
  string def_lang = tokenizer->default_language();
  string detected_lang = fd.get_language();
  if ( options.debugFlag > 1 ){
    DBG << "append_to_sentence()" << endl;
    DBG << "fd.language = " << detected_lang << endl;
    DBG << "default_language = " << def_lang << endl;
    DBG << "sentence language = '" << la << "'" << endl;
  }
  if ( la.empty()
       && !detected_lang.empty()
       && detected_lang != "default"
       && detected_lang != def_lang ){
    //
    // so the language is non default, and not set
    //
    Tokenizer::set_language( sent, detected_lang );
    if ( options.textredundancy == "full" ){
      sent->setutext( sent->unicode(options.inputclass),
		      options.inputclass );
    }
    return;
  }
  if ( !la.empty() && la != def_lang ){
    //
    // so the language is set, and it is NOT the default language
    // Don't process any further
    //
    if ( options.debugFlag > 0 ){
      DBG << "append_to_sentence() SKIP a sentence: " << la << endl;
    }
  }
  else {
    if ( options.doTagger ){
      myCGNTagger->add_tags( wv, fd );
    }
    if ( options.doLemma ){
      myMblem->add_lemmas( wv, fd );
    }
    if ( options.doMbma ){
      myMbma->add_folia_morphemes( wv, fd );
    }
    if ( options.doIOB ){
      myIOBTagger->add_result( fd, wv );
    }
    if ( options.doMwu && !fd.mwus.empty() ){
      myMwu->add_result( fd, wv );
    }
    if ( options.doNER ){
      myNERTagger->add_result( fd, wv );
    }
    if ( options.doAlpino
	 || options.doParse ){
      if ( options.maxParserTokens != 0
	   && fd.size() > options.maxParserTokens ){
	DBG << "no parse results added. sentence too long" << endl;
      }
      else {
	myParser->add_result( fd, wv );
      }
    }
    if (options.debugFlag > 1){
      DBG << "done with append_to_sentence()" << endl;
    }
  }
}

folia::FoliaElement *FrogAPI::append_to_folia( folia::FoliaElement *root,
					       const frog_data& fd,
					       unsigned int& p_count ) const {
  /// append Frog results to a folia element.
  /*!
    \param root The FoliaElement to append to
    \param fd The frog_data structure which holds all results
    \param p_count the current paragraph count. Can be updated.
    \return the root element OR a newly created folia::Paragraph under that root
   */
  if ( !root || !root->doc() ){
    return 0;
  }
  if  (options.debugFlag > 5 ){
    DBG << "append_to_folia, root = " << root << endl;
    DBG << "frog_data=\n" << fd << endl;
  }
  folia::FoliaElement *result = root;
  folia::KWargs args;
  if ( fd.units[0].new_paragraph ){
    if  (options.debugFlag > 5 ){
      DBG << "append_to_folia, NEW paragraph " << endl;
    }
    args["xml:id"] = root->doc()->id() + ".p." + TiCC::toString(++p_count);
    folia::Paragraph *p;
    if ( root->element_id() == folia::Text_t ){
      if  (options.debugFlag > 5 ){
	DBG << "append_to_folia, add paragraph to Text" << endl;
      }
      p = root->add_child<folia::Paragraph>( args );
      if ( options.debugFlag > 5 ){
	DBG << "created a new: " << p << endl;
      }
    }
    else {
      // root is a paragraph, which is done now.
      p = root->parent()->add_child<folia::Paragraph>( args );
      if ( options.textredundancy == "full" ){
	root->setutext( root->unicode(options.outputclass),
			options.outputclass);
      }
      if  (options.debugFlag > 5 ){
	DBG << "append_to_folia, add paragraph to parent of " << root << endl;
      }
    }
    result = p;
  }
  args.clear();
  args["generate_id"] = result->id();
  folia::Sentence *s = result->add_child<folia::Sentence>( args );
  if ( options.debugFlag > 5 ){
    DBG << "append_to_folia, created Sentence" << s << endl;
  }
  append_to_sentence( s, fd );
  if ( options.debugFlag > 5 ){
    DBG << "append_to_folia, done, result node = " << result << endl;
  }
  return result;
}

void FrogAPI::append_to_words( const vector<folia::Word*>& wv,
			       const frog_data& fd ) const {
  /// add all Frogged information to a vector of folia::Words
  /*!
    \param wv The vector of folia::Word elements
    \param fd The frog_data structure which holds all results

    the arity of both parameters should be equal!
  */
  string def_lang = tokenizer->default_language();
  string detected_lang = fd.get_language();
  if ( detected_lang != "default"
       && detected_lang != def_lang ){
    if ( options.debugFlag > 0 ){
      DBG << "append_words() SKIP a sentence (different language: "
	  << detected_lang << "), default= " << def_lang << endl;
    }
  }
  else {
    if ( options.doTagger ){
      myCGNTagger->add_tags( wv, fd );
    }
    if ( options.doLemma ){
      myMblem->add_lemmas( wv, fd );
    }
    if ( options.doMbma ){
      myMbma->add_folia_morphemes( wv, fd );
    }
    if ( options.doNER ){
      myNERTagger->add_result( fd, wv );
    }
    if ( options.doIOB ){
      myIOBTagger->add_result( fd, wv );
    }
    if ( options.doMwu && !fd.mwus.empty() ){
      myMwu->add_result( fd, wv );
    }
    if ( ( options.doAlpino
	   || options.doParse )
	 && wv.size() > 1 ){
      if ( options.maxParserTokens != 0
	   && wv.size() > options.maxParserTokens ){
	DBG << "no parse results added. sentence too long" << endl;
      }
      else {
	myParser->add_result( fd, wv );
      }
    }
  }
}

void FrogAPI::FrogServer( Sockets::ClientSocket &conn ){
  /// Run a server
  /*!
    \param conn A Sockets::ClientSocket object to connect to

    The 'conn' object should be correctly set up using the right parameters.
    Depending on the Frog settings we can serve text, FoLiA and JSON.
    At the moment only TCP connections are supported.
  */
  try {
    while ( conn.isValid() ) {
      ostringstream output_stream;
      if ( options.doXMLin ){
        string result;
        string s;
        while ( conn.read(s) ){
	  result += s + "\n";
	  if ( s.empty() ){
	    break;
	  }
        }
        if ( result.size() < 50 ){
	  // a FoLia doc must be at least a few 100 bytes
	  // so this is clearly wrong. Just bail out
	  throw( runtime_error( "read garbage" ) );
        }
        if ( options.debugFlag > 5 ){
	  DBG << "received data [" << result << "]" << endl;
	}
	TiCC::tmp_stream ts( "frog" );
	ts.os() << result << endl;
	ts.close();
	folia::Document *xml = run_folia_engine( ts.tmp_name(), output_stream );
	if ( xml && options.doXMLout ){
	  xml->set_canonical(options.doKanon);
	  output_stream << xml;
	  delete xml;
	}
	LOG << "Done Processing XML... " << endl;
      }
      else if ( options.doJSONin ){
	json the_json;
	string json_line;
	// read data from the connection
	if ( !conn.read( json_line ) ){
	  throw( runtime_error( "read failed: '" + json_line + "' (" +
				conn.getMessage() + ")" ) );
	}
        if ( options.debugFlag > 5 ){
	  DBG << "JSON read line: " << json_line << endl;
	}
	if ( json_line.empty() ){
	  // assume we are done
	  if ( options.debugFlag > 5 ){
	    DBG << "Done with processing JSON. exit" << endl;
	  }
	  LOG << "Done with JSON" << endl;
	  return; // closes this connection
	}
	else {
	  try {
	    the_json = json::parse( json_line );
	  }
	  catch ( const exception& e ){
	    cerr << "json parsing failed on '" << json_line + "':"
		 << e.what() << endl;
	    throw runtime_error( "json failure" );
	  }
	  if ( options.debugFlag ){
	    DBG << "Parsed JSON: " << the_json << endl;
	  }
	  for ( const auto& it : the_json ){
	    UnicodeString data = TiCC::UnicodeFromUTF8(it["sentence"]);
	    timers.tokTimer.start();
	    vector<Tokenizer::Token> toks = tokenizer->tokenize_line( data );
	    timers.tokTimer.stop();
	    while ( toks.size() > 0 ){
	      frog_data sent = frog_sentence( toks, 1 );
	      show_results( output_stream, sent );
	      timers.tokTimer.start();
	      toks = tokenizer->tokenize_next();
	      timers.tokTimer.stop();
	    }
	  }
	}
      }
      else {
        string data = "";
        if ( options.doSentencePerLine ){
	  if ( !conn.read( data ) ){
	    //read data from client
	    throw( runtime_error( "read failed" ) );
	  }
	}
        else {
	  string line;
	  while( conn.read(line) ){
	    if ( line == "EOT" ){
	      break;
	    }
	    data += line + "\n";
	  }
        }
	// So data will contain the COMPLETE input,
	// OR a FoLiA Document (which should be valid)
	// OR a sequence of lines, forming sentences and paragraphs
        if ( options.debugFlag > 5 ){
	  DBG << "Received: [" << data << "]" << endl;
	}
        LOG << TiCC::Timer::now() << " Processing... " << endl;
	folia::Document *doc = 0;
	folia::FoliaElement *root = 0;
	unsigned int par_count = 0;
	if ( options.doXMLout ){
	  string doc_id = options.docid;
	  if ( doc_id.empty() ){
	    doc_id = "untitled";
	  }
	  root = start_document( doc_id, doc );
	}
	timers.tokTimer.start();
	// start tokenizing
	// tokenize_data() delivers the first sentence, call
	//  tokenize_next() multiple times to get all sentences!
	vector<Tokenizer::Token> toks = tokenizer->tokenize_data( data );
	timers.tokTimer.stop();
	while ( toks.size() > 0 ){
	  frog_data sent = frog_sentence( toks, 1 );
	  if ( options.doXMLout ){
	    root = append_to_folia( root, sent, par_count );
	  }
	  else {
	    show_results( output_stream, sent );
	  }
	  timers.tokTimer.start();
	  toks = tokenizer->tokenize_next();
	  timers.tokTimer.stop();
	}
	if ( options.doXMLout && doc ){
	  doc->set_canonical(options.doKanon);
	  output_stream << doc;
	  delete doc;
	}
	//	DBG << "Done Processing... " << endl;
      }
      if ( options.doJSONout ){
        if ( options.debugFlag > 5 ){
	  LOG << "start JSON output:"<< endl;
	}
        if ( options.debugFlag > 10 ){
	  LOG << "JSON:" << output_stream.str() << endl;
	}
	if (!conn.write( output_stream.str() ) ){
	  if (options.debugFlag > 5 ) {
	    DBG << "socket " << conn.getMessage() << endl;
	  }
	  throw ( runtime_error( "JSON write to client failed: "
				 + conn.getMessage() ) );
	}
        if ( options.debugFlag > 5 ){
	  LOG << "done with JSON output" << endl;
	}
      }
      else if ( !conn.write( output_stream.str() )
		|| !(conn.write("READY\n\n"))  ){
	if (options.debugFlag > 5 ) {
	  DBG << "socket " << conn.getMessage() << endl;
	}
	throw ( runtime_error( "write to client failed" +
			       conn.getMessage() ) );
      }
    }
  }
  catch ( std::exception& e ) {
    LOG << TiCC::Timer::now() << ": connection lost unexpected : "
	<< e.what() << endl;
  }
  LOG << "Connection closed.\n";
}

void FrogAPI::FrogStdin( bool prompt ) {
  /// run frog on stdin, output to stdout
  /*!
    \param prompt Use a prompt. fixed to the string 'frog>'

    This is the fallback when run_interactive is used on NON-tty devices
    (e.g. a pipe) OR when Frog is build without READLINE support.
  */
  if ( prompt ){
    cout << "frog>"; cout.flush();
  }
  string line;
  while ( getline( cin, line ) ){
    string data = line;
    if ( options.doSentencePerLine ){
      if ( line.empty() ){
	if ( prompt ){
	  cout << "frog>"; cout.flush();
	}
	continue;
      }
    }
    else {
      if ( !line.empty() ){
	data += "\n";
      }
      if ( prompt ){
	cout << "frog>"; cout.flush();
      }
      string line2;
      while( getline( cin, line2 ) ){
	if ( line2.empty() ){
	  break;
	}
	data += line2 + "\n";
	if (prompt ){
	  cout << "frog>"; cout.flush();
	}
      }
    }
    if ( prompt && data.empty() ){
      cout << "ignoring empty input" << endl;
      cout << "frog>"; cout.flush();
      continue;
    }
    if ( prompt ){
      cout << "Processing... " << endl;
    }
    vector<Tokenizer::Token> toks = tokenizer->tokenize_data( data );
    while ( toks.size() > 0 ){
      frog_data res = frog_sentence( toks, 1 );
      show_results( cout, res );
      toks = tokenizer->tokenize_next();
    }
    if ( prompt ){
      cout << "frog>"; cout.flush();
    }
  }
  if ( prompt ){
    cout << "Done.\n";
  }
}

void FrogAPI::run_interactive(){
  /// Run an interactive frog on a terminal or a pipe.
  /*! Normally READLINE support is used to set up an interactive session to
    communicate with a user on a terminal.

    On non-terminal devices (like a pipe) frog wil just consume input from the
    input device and output to the output device.
    This is also the fallback when READLINE support is not available.
   */
  bool isTTY = isatty(0);
#ifdef NO_READLINE
  FrogStdin(isTTY);
#else
  if ( !isTTY ){
    FrogStdin( false );
  }
  else {
    const char *prompt = "frog> ";
    string line;
    bool eof = false;
    while ( !eof ){
      string data;
      char *input = readline( prompt );
      if ( !input ){
	break;
      }
      line = input;
      if ( options.doSentencePerLine ){
	if ( line.empty() ){
	  free( input );
	  continue;
	}
	else {
	  add_history( input );
	  free( input );
	  data += line + "\n";
	}
      }
      else {
	if ( !line.empty() ){
	  add_history( input );
	  free( input );
	  data = line + "\n";
	}
	while ( !eof ){
	  input = readline( prompt );
	  if ( !input ){
	    eof = true;
	    break;
	  }
	  line = input;
	  if ( line.empty() ){
	    free( input );
	    break;
	  }
	  add_history( input );
	  free( input );
	  data += line + "\n";
	}
      }
      if ( !data.empty() ){
	if ( data.back() == '\n' ){
	  data.pop_back();
	}
	cout << "Processing... '" << data << "'" << endl;
	vector<Tokenizer::Token> toks = tokenizer->tokenize_data( data );
	while ( !toks.empty() ){
	  frog_data res = frog_sentence( toks, 1 );
	  show_results( cout, res );
	  toks = tokenizer->tokenize_next();
	}
      }
    }
    cout << "Done.\n";
  }
#endif
}

string FrogAPI::Frogtostring( const string& s ){
  /// Parse a string, Frog it and return the result as a string.
  /*!
    \param s: an UTF8 encoded string. May be multilined. May be FoLiA too.
    \return the results of frogging.

    Depending of the current frog settings the input can be interpreted as XML,
    an the output will be XML or tab separated
  */
  if ( s.empty() ){
    return s;
  }
  TiCC::tmp_stream ts( "frog" );
  ts.os() << s << endl;
  ts.close();
  return Frogtostringfromfile( ts.tmp_name() );
}

string FrogAPI::Frogtostringfromfile( const string& infilename ){
  /// Parse a file, Frog it and return the result as a string.
  /*!
    \param infilename The filename.
    \return the results of frogging endoded in an UTF8 string.

    Depending of the current frog settings the inputfile can be interpreted as
    XML, and the output will be XML or tab separated.
  */
  stringstream ss;
  options.hide_timers = true;
  bool old_val = options.noStdOut;
  ostream* old_out = outS;
  if ( options.doXMLout ){
    options.noStdOut = true;
  } else {
    options.noStdOut = false;
    outS = &ss;
  }
  folia::Document *result = FrogFile( infilename );
  options.noStdOut = old_val;
  outS = old_out;
  if ( result ){
    result->set_canonical( options.doKanon );
    ss << result;
    delete result;
  }
  return ss.str();
}

frog_data extract_fd( vector<Tokenizer::Token>& tokens,
		      bool no_eos ){
  /// extract a frog_data structure from a list of Tokens
  /*!
    \param tokens a list of Tokenizer::Token
    \param no_eos if true, ignore ENDOFSENTENCE marks, and read on
    \return the new frog_data structure

    The tokens list may span multiple sentences and paragraphs; this function
    will return the data for a single sentence and must be called multiple times
    until the whole list is consumed.
   */
  frog_data result;
  int quotelevel = 0;
  while ( !tokens.empty() ){
    const auto tok = tokens.front();
    tokens.erase(tokens.begin());
    frog_record tmp;
    tmp.word = tok.us;
    tmp.token_class = tok.type;
    tmp.no_space = (tok.role & Tokenizer::TokenRole::NOSPACE);
    tmp.language = tok.lang_code;
    tmp.new_paragraph = (tok.role & Tokenizer::TokenRole::NEWPARAGRAPH);
    result.units.push_back( tmp );
    if ( (tok.role & Tokenizer::TokenRole::BEGINQUOTE) ){
      ++quotelevel;
    }
    if ( (tok.role & Tokenizer::TokenRole::ENDQUOTE) ){
      --quotelevel;
    }
    if ( !no_eos
	 && (tok.role & Tokenizer::TokenRole::ENDOFSENTENCE) ){
      // we are at ENDOFSENTENCE.
      // when quotelevel == 0, we step out, until the next call
      if ( quotelevel == 0 ){
	//	result.language = tok.lang_code;
	break;
      }
    }
  }
  return result;
}

frog_data FrogAPI::frog_sentence( vector<Tokenizer::Token>& sent,
				  const size_t s_count,
				  bool no_eos ){
  /// extract a frog_data structure, representing 1 frogged sentence
  /*!
    \param sent a list of Tokenizer::Token
    \param s_count holds the sentence count
    \parem no_eos if true,process the whole sent, ignoring ENDOFSENTENCE marks
    \return a frog_data structure representing 1 totally frogged sentence. Will
    be empty if we are done.

    This function is reentrant and should be called multiple times until the
    whole 'sent' list is consumed and an empty frog_data is returned.

    All enabled Frog modules are run on the \e sent and for every completed
    sentence a frog_data structure is returned.

    \e s_count is incremented for every returned frog_data element.

  */
  if ( options.debugFlag > 0 ){
    DBG << "tokens:\n" << sent << endl;
  }
  frog_data sentence = extract_fd( sent, no_eos );
  if ( options.debugFlag > 0 ){
    DBG << "sentence:\n" << sentence << endl;
  }
  string lan = sentence.get_language();
  string def_lang = tokenizer->default_language();
  if ( options.debugFlag > 0 ){
    DBG << "frog_sentence() on a part. (lang=" << lan << ")" << endl;
    DBG << "default_language=" << def_lang << endl;
    DBG << "frog_data:\n" << sentence << endl;
  }
  if ( !def_lang.empty()
       && !lan.empty()
       && lan != "default"
       && lan != def_lang ){
    if ( options.debugFlag > 0 ){
      DBG << "skipping sentence " << s_count << " (different language: " << lan
	   << " --language=" << def_lang << ")" << endl;
    }
    return sentence;
  }
  else {
    timers.frogTimer.start();
    if ( options.debugFlag > 5 ){
      DBG << "Frogging sentence:\n" << sentence << endl;
      DBG << "tokenized text = " << sentence.sentence() << endl;
    }
    bool all_well = true;
    string exs;
    timers.tagTimer.start();
    try {
      myCGNTagger->Classify( sentence );
    }
    catch ( exception&e ){
      all_well = false;
      exs += string(e.what()) + " ";
    }
    timers.tagTimer.stop();
    if ( !all_well ){
      throw runtime_error( exs );
    }
    for ( auto& word : sentence.units ) {
#pragma omp parallel sections
      {
	// Lemmatization and Mophological analysis can be done in parallel
	// per word
#pragma omp section
	{
	  if ( options.doMbma ){
	    timers.mbmaTimer.start();
	    if (options.debugFlag > 1){
	      DBG << "Calling mbma..." << endl;
	    }
	    try {
	      myMbma->Classify( word );
	    }
	    catch ( exception& e ){
	      all_well = false;
	      exs += string(e.what()) + " ";
	    }
	    timers.mbmaTimer.stop();
	  }
	}
#pragma omp section
	{
	  if ( options.doLemma ){
	    timers.mblemTimer.start();
	    if (options.debugFlag > 1) {
	      DBG << "Calling mblem..." << endl;
	    }
	    try {
	      myMblem->Classify( word );
	    }
	    catch ( exception&e ){
	      all_well = false;
	      exs += string(e.what()) + " ";
	    }
	    timers.mblemTimer.stop();
	  }
	}
      } // omp parallel sections
    } //for all words
    if ( !all_well ){
      throw runtime_error( exs );
    }
    //    cout << endl;
#pragma omp parallel sections
    {
      // NER and IOB tagging can be done in parallel, per Sentence
#pragma omp section
      {
	if ( options.doNER ){
	  timers.nerTimer.start();
	  if (options.debugFlag > 1) {
	    DBG << "Calling NER..." << endl;
	  }
	  try {
	    myNERTagger->Classify( sentence );
	  }
	  catch ( exception&e ){
	    all_well = false;
	    exs += string(e.what()) + " ";
	  }
	  timers.nerTimer.stop();
	}
      }
#pragma omp section
      {
	if ( options.doIOB ){
	  timers.iobTimer.start();
	  try {
	    myIOBTagger->Classify( sentence );
	  }
	  catch ( exception&e ){
	    all_well = false;
	    exs += string(e.what()) + " ";
	  }
	  timers.iobTimer.stop();
	}
      }
    }
    //
    // MWU resolution needs the previous results per sentence
    // AND must be done before parsing
    //
    if ( !all_well ){
      throw runtime_error( exs );
    }
    if ( options.doMwu ){
      if ( !sentence.empty() ){
	timers.mwuTimer.start();
	myMwu->Classify( sentence );
	timers.mwuTimer.stop();
      }
    }
    if ( options.doAlpino || options.doParse ){
      if ( options.maxParserTokens == 0
	   || sentence.size() <= options.maxParserTokens ){
	myParser->Parse( sentence, timers );
      }
      else {
	LOG << "WARNING!" << endl;
	LOG << "Sentence " << s_count
	    << " isn't parsed because it contains more tokens ("
	    << sentence.size()
	    << ") then set with the --max-parser-tokens="
	    << options.maxParserTokens << " option." << endl;
	DBG << 	"Sentence " << s_count << " is too long: " << endl
	    << sentence.sentence(true) << endl;
      }
    }
    timers.frogTimer.stop();
    if ( options.debugFlag > 5 ){
      DBG << "Frogged one sentence:" << endl << sentence << endl;
    }
    return sentence;
  }
}

string filter_non_NC( const string& filename ){
  /// Filter out characters that would be invalid as an XML NCname
  /*!
    \param filename a string representing a filename
    \return a string where troublesome characters are skipped or replaced

    see https://www.w3.org/TR/xmlschema-2/#NCName for more information
  */
  string result;
  bool at_start = true;
  for ( size_t i=0; i < filename.length(); ++i ){
    if ( at_start ){
      if ( isdigit( filename[i] ) ){
	continue; //ditch numerics!
      }
      else {
	at_start = false;
      }
    }
    if ( filename[i] == ':' ){
      if ( at_start ){
	result += "C";
      }
      else {
	result += "-";
      }
    }
    else {
      result += filename[i];
    }
  }
  if ( result.empty()
       || result[0] == '.' ){
    // ouch, only numbers? Or only before the extension?
    return filter_non_NC( "N" + filename );
  }
  return result;
}

const string Tab = "\t";

void FrogAPI::output_tabbed( ostream& os, const frog_record& fd ) const {
  /// output a frog_record in tabbed format
  /*!
    \param os the output stream
    \param fd the record to display

    This function is used as part of outputting a complete frog_data structure
    in show_results
  */
  os << fd.word << Tab;
  if ( options.doLemma ){
    if ( !fd.lemmas.empty() ){
      os << fd.lemmas[0];
    }
    else {
      os << Tab;
    }
  }
  else {
    os << Tab;
  }
  os << Tab;
  if ( options.doMbma ){
    os << fd.morph_string;
    if ( options.doCompounds
	 || options.doDeepMorph ){
      os << Tab;
      if ( fd.compound_string.find("0") != string::npos  ){
	os << "0";
      }
      else {
	os << fd.compound_string + "-compound";
      }
    }
  }
  if ( options.doTagger ){
    if ( fd.tag.isEmpty() ){
      os << Tab << Tab << fixed << showpoint << std::setprecision(6) << 1.0;
    }
    else {
      os << Tab << fd.tag << Tab
	 << fixed << showpoint << std::setprecision(6) << fd.tag_confidence;
    }
  }
  else {
    os << Tab << Tab << Tab;
  }
  if ( options.doNER ){
    os << Tab << fd.ner_tag;
  }
  else {
    os << Tab << Tab;
  }
  if ( options.doIOB ){
    os << Tab << fd.iob_tag;
  }
  else {
    os << Tab << Tab;
  }
  if ( options.doParse || options.doAlpino ){
    if ( fd.parse_index == -1 ){
      os << Tab << "0" << Tab << "ROOT"; // bit strange, but backward compatible
    }
    else {
      os << Tab << fd.parse_index << Tab << fd.parse_role;
    }
  }
  else {
    os << Tab << Tab << Tab << Tab;
  }
}

void FrogAPI::output_JSON( ostream& os,
			   const frog_data& fd,
			   int pp_val ) const {
  /// output a frog_data structure as JSON
  /*!
    \param os the output stream
    \param fd the frog_data to display
    \param pp_val value to use for formatted output.

    If pp_val is 0, the whole JSON is output as a (very) long string.
    If pp_val > 0, the JSON is formatted neatly with pp_val as indentation
  */
  json out_json = json::array();
  if ( fd.mw_units.empty() ){
    for ( size_t pos=0; pos < fd.units.size(); ++pos ){
      json part = fd.units[pos].to_json();
      part["index"] = pos+1;
      out_json.push_back( part );
    }
  }
  else {
    for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
      json part = fd.mw_units[pos].to_json();
      part["index"] = pos+1;
      out_json.push_back( part );
    }
  }
  if ( options.debugFlag ){
    DBG << "spitting out JSON:" << out_json << endl;
  }
  os << std::setw(pp_val) << out_json << endl;
}

void FrogAPI::show_results( ostream& os,
			    const frog_data& fd ) const {
  /// output a frog_data structure to a stream.
  /*!
    \param os the outputstream
    \param fd the structure to display

    Depending on Frog settings, the output can be 'tabbed' or JSON
  */
  if ( options.doJSONout ){
    output_JSON( os, fd, options.JSON_pp );
  }
  else {
    if ( fd.mw_units.empty() ){
      for ( size_t pos=0; pos < fd.units.size(); ++pos ){
	os << pos+1 << Tab;
	output_tabbed( os, fd.units[pos] );
	os << endl;
      }
    }
    else {
      for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
	os << pos+1 << Tab;
	output_tabbed( os, fd.mw_units[pos] );
	os << endl;
      }
    }
    os << endl;
  }
}

UnicodeString replace_spaces( const UnicodeString& in ) {
  /// replace spaces by underscores
  /*!
    \param in an Unicode string with embedded spaces
    \return an Unicode string where all spaces have been replaced by an
    underscore

    'spaces' are detected using the ICU u_ispace() function
  */
  UnicodeString result;
  StringCharacterIterator sit(in);
  while ( sit.hasNext() ){
    UChar32 c = sit.current32();
    if ( u_isspace(c)) {
      result += '_';
    }
    else {
      result += c;
    }
    sit.next32();
  }
  return result;
}

void FrogAPI::handle_word_vector( ostream& os,
				  const vector<folia::Word*>& wv_in,
				  const size_t s_cnt ){
  /// run frog on a vector of folia::Word as extracted from a document
  /*!
    \param os stream for output
    \param wv_in the Word list to handle
    \param s_cnt the sentence count
  */
  folia::FoliaElement *parent = wv_in[0]->parent();
  //  LOG << "PARENT = " << parent << endl;
  vector<folia::Word*> wv = wv_in;
  vector<Tokenizer::Token> toks;
  if ( options.correct_words ){
    // we are allowed to let the tokenizer correct those
    toks = tokenizer->correct_words( parent, wv );
    wv = parent->words();
  }
  else {
    // assume unfrogged BUT tokenized!
    UnicodeString text;
    for ( const auto& w : wv ){
      UnicodeString tmp = w->unicode( options.inputclass );
      tmp = replace_spaces( tmp );
      text += tmp + " ";
    }
    if  ( options.debugFlag > 1 ){
      DBG << "handle_one_sentence() on existing words" << endl;
      DBG << "handle_one_sentence() untokenized string: '" << text << "'" << endl;
    }
    toks = tokenizer->tokenize_line( text );
    //    cerr << "text:" << text << " size=" << wv.size() << endl;
  }
  //  cerr << "tokens:" << toks << " size=" << toks.size() << endl;
  if ( toks.size() == wv.size() ){
    frog_data res = frog_sentence( toks, s_cnt );
    //    cerr << "res:" << res << " size=" << res.size() << endl;
    if ( res.size() > 0 ){
      if ( !options.noStdOut ){
	show_results( os, res );
      }
      if ( options.doXMLout ){
	append_to_words( wv, res );
      }
    }
  }
  else {
    string msg = parent->doc()->filename() + ": unable to frog: " + parent->id()
      + " because it contains untokenized Words.";
    LOG << msg << endl;
    if ( !options.correct_words ) {
      LOG << " (you might try --allow-word-corrections)" << endl;
    }
    throw runtime_error( msg );
  }
}

void FrogAPI::handle_one_sentence( ostream& os,
				   folia::Sentence *s,
				   const size_t s_cnt ){
  /// run frog on a folia::Sentence as extracted from a document
  /*!
    \param os stream for output
    \param s the Sentence to handle
    \param s_cnt the sentence count
  */
  if  ( options.debugFlag > 1 ){
    DBG << "handle_one_sentence: " << s << endl;
  }
  string sent_lang = s->language();
  if ( sent_lang.empty() ){
    sent_lang = tokenizer->default_language();
  }
  if ( find( options.languages.begin(), options.languages.end(), sent_lang )
       == options.languages.end() ){
    // ignore this language!
    if ( options.debugFlag > 0 ){
      DBG << sent_lang << " NOT in: " << options.languages << endl;
    }
    return;
  }
  vector<folia::Word*> wv = s->words( options.inputclass );
  if ( wv.empty() ){
    wv = s->words();
  }
  if ( !wv.empty() ){
    // there are already words.
    handle_word_vector( os, wv, s_cnt );
  }
  else {
    UnicodeString text = s->unicode(options.inputclass);
    if ( options.debugFlag > 0 ){
      DBG << "handle_one_sentence() from string: '" << text << "' (lang="
	  << sent_lang << ")" << endl;
    }
    if ( options.doXMLout ){
      timers.tokTimer.start();
      vector<Tokenizer::Token> all_toks;
      vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text, sent_lang );
      while ( toks.size() > 0 ){
	// the tokenizer may split the text into more than one sentences
	// but we don't want that, it spoils the resulting FoLiA.
	// The input Sentence node should stay leading
	all_toks.insert( all_toks.end(), toks.begin(), toks.end() );
	toks = tokenizer->tokenize_next();
      }
      timers.tokTimer.stop();
      frog_data sent = frog_sentence( all_toks, s_cnt, true );
      if ( !options.noStdOut ){
	show_results( os, sent );
      }
      append_to_sentence( s, sent );
    }
    else {
      timers.tokTimer.start();
      vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text, sent_lang );
      timers.tokTimer.stop();
      while ( toks.size() > 0 ){
	frog_data sent = frog_sentence( toks, s_cnt );
	if ( !options.noStdOut ){
	  show_results( os, sent );
	}
	timers.tokTimer.start();
	toks = tokenizer->tokenize_next();
	timers.tokTimer.stop();
      }
    }
  }
}

void FrogAPI::handle_one_paragraph( ostream& os,
				    folia::Paragraph *p,
				    int& sentences_done ){
  /// run frog on a folia::Paragraph as extracted from a document
  /*!
    \param os stream for output
    \param p the Paragraph to handle
    \param sentence_done holds the number of sentences done, may be incremented

    a Paragraph may contain both Word and Sentence nodes
    if so, the Sentences should be handled separately
  */
  vector<folia::Word*> wv = p->select<folia::Word>(false);
  vector<folia::Sentence*> sv = p->select<folia::Sentence>(false);
  if ( options.debugFlag > 1 ){
    DBG << "found some Words " << wv << endl;
    DBG << "found some Sentences " << sv << endl;
  }
  if ( sv.empty() ){
    // No Sentence, so only words OR just text
    if ( wv.empty() ){
      UnicodeString text = p->unicode(options.inputclass);
      if ( options.debugFlag > 0 ){
	DBG << "handle_one_paragraph:" << text << endl;
      }
      timers.tokTimer.start();
      vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text );
      timers.tokTimer.stop();
      while ( toks.size() > 0 ){
	frog_data res = frog_sentence( toks, ++sentences_done );
	while ( !res.empty() ){
	  if ( !options.noStdOut ){
	    show_results( os, res );
	  }
	  if ( options.doXMLout ){
	    folia::KWargs args;
	    string p_id = p->id();
	    if ( !p_id.empty() ){
	      args["generate_id"] = p_id;
	    }
	    folia::Sentence *s = p->add_child<folia::Sentence>( args );
	    append_to_sentence( s, res );
	  }
	  if ( toks.size() == 0 ){
	    break;
	  }
	  res = frog_sentence( toks, ++sentences_done );
	}
	timers.tokTimer.start();
	toks = tokenizer->tokenize_next();
	timers.tokTimer.stop();
      }
    }
    else {
      handle_word_vector( os, wv, sentences_done );
    }
  }
  else {
    if ( !wv.empty() ){
      string msg = "paragraph " + p->id() + " has both <w> and <s> nodes. "
	+ "this is NOT supported by Frog";
      throw runtime_error( msg );
#ifdef TOO_OPTIMISTIC
      // Not good enough. The words in wv can be scattered within sentences
      // You should split wv further in parts with adjacent Words
      folia::KWargs args;
      args["generate_id"] = p->id();
      folia::Sentence *s = new folia::Sentence( args, p->doc() );
      p->insert_after( wv.back(), s );
      for ( auto& w : wv ){
	p->remove( w );
	s->append( w );
      }
      handle_word_vector( os, wv, sentences_done );
#endif
    }
    for ( const auto& s : sv ){
      handle_one_sentence( os, s, sentences_done );
    }
  }
}

void FrogAPI::handle_one_text_parent( ostream& os,
				      folia::FoliaElement *e,
				      int& sentence_done ){
  /// genric function to handle text from FoliaElements
  /*!
    \param os output stream for the results
    \param e a FoLiA element containing text.
    \param sentence_done holds the number of sentences done, may be incremented

    The input 'e' this can be a Word, Sentence, Paragraph or some other element
    In the latter case, we construct a Sentence from the text, and
    a Paragraph if more then one Sentence is found
  */
  if ( e->xmltag() == "w" ){
    // already tokenized into words!
    folia::Word *word = dynamic_cast<folia::Word*>(e);
    UnicodeString text = word->unicode( options.inputclass );
    text = replace_spaces( text );
    vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text );
    if ( toks.size() > 0 ){
      frog_data res = frog_sentence( toks, ++sentence_done );
      if ( !options.noStdOut ){
	show_results( os, res );
      }
      if ( options.doXMLout ){
	vector<folia::Word*> wv;
	wv.push_back( word );
	append_to_words( wv, res );
      }
    }
  }
  else if ( e->xmltag() == "s" ){
    // OK a text in a sentence
    if ( options.debugFlag > 2 ){
      DBG << "found text in a sentence " << e << endl;
    }
    handle_one_sentence( os,
			 dynamic_cast<folia::Sentence*>(e),
			 ++sentence_done );
  }
  else if ( e->xmltag() == "p" ){
    // OK a longer text in some paragraph
    if ( options.debugFlag > 2 ){
      DBG << "found text in a paragraph " << e << endl;
    }
    handle_one_paragraph( os,
			  dynamic_cast<folia::Paragraph*>(e),
			  sentence_done );
  }
  else {
    // Some text outside word, paragraphs or sentences (yet)
    // mabe <div> or <note> or such
    // there may be Paragraph, Word and Sentence nodes
    // if so, Paragraphs and Sentences should be handled separately
    vector<folia::Word*> wv = e->select<folia::Word>(false);
    vector<folia::Sentence*> sv = e->select<folia::Sentence>(false);
    vector<folia::Paragraph*> pv = e->select<folia::Paragraph>(false);
    if ( options.debugFlag > 1 ){
      DBG << "found some Words " << wv << endl;
      DBG << "found some Sentences " << sv << endl;
      DBG << "found some Paragraphs " << pv << endl;
    }
    if ( pv.empty() && sv.empty() ){
      // just words of text
      UnicodeString text = e->unicode(options.inputclass);
      if ( options.debugFlag > 1 ){
	DBG << "frog-" << e->xmltag() << ":" << text << endl;
      }
      timers.tokTimer.start();
      vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text );
      timers.tokTimer.stop();
      vector<frog_data*> sents; // a vector of pointers, to avoid premature
      // deletion of the frog_data
      while ( toks.size() > 0 ){
	frog_data *res = new frog_data( frog_sentence( toks, ++sentence_done) );
	sents.push_back( res );
	if ( !options.noStdOut ){
	  show_results( os, *res );
	}
	timers.tokTimer.start();
	toks = tokenizer->tokenize_next( );
	timers.tokTimer.stop();
      }
      if ( options.doXMLout ){
	if ( sents.size() == 0 ){
	  // might happen in rare cases
	  // just skip
	}
	else if ( sents.size() > 1 ){
	  // multiple sentences. We need an extra Paragraph. when allowed
	  if ( e->acceptable(folia::Paragraph_t) ){
	    folia::KWargs p_args;
	    string e_id = e->id();
	    if ( !e_id.empty() ){
	      p_args["generate_id"] = e_id;
	    }
	    folia::Paragraph *p = e->add_child<folia::Paragraph>( p_args );
	    for ( const auto& sent : sents ){
	      folia::KWargs args;
	      string p_id = p->id();
	      if ( !p_id.empty() ){
		args["generate_id"] = p_id;
	      }
	      folia::Sentence *s = p->add_child<folia::Sentence>( args );
	      append_to_sentence( s, *sent );
	      if  (options.debugFlag > 0){
		DBG << "created a new sentence: " << s << endl;
	      }
	    }
	  }
	  else {
	    if ( options.debugFlag > 5 ){
	      DBG << "not e->acceptable\n" << e << endl;
	    }
	    for ( const auto& sent : sents ){
	      folia::KWargs args;
	      string e_id = e->id();
	      if ( !e_id.empty() ){
		args["generate_id"] = e_id;
	      }
	      folia::Sentence *s = e->add_child<folia::Sentence>( args );
	      append_to_sentence( s, *sent );
	      if  (options.debugFlag > 0){
		DBG << "created a new sentence: " << s << endl;
	      }
	    }
	  }
	}
	else {
	  // 1 sentence, connect directly.
	  folia::KWargs args;
	  string e_id = e->id();
	  if ( e_id.empty() ){
	    e_id = e->generateId( e->xmltag() );
	    args["xml:id"] = e_id + ".s.1";
	  }
	  else {
	    args["generate_id"] = e_id;
	  }
	  folia::Sentence *s = e->add_child<folia::Sentence>( args );
	  append_to_sentence( s, *sents[0] );
	  if  (options.debugFlag > 0){
	    DBG << "created a new sentence: " << s << endl;
	  }
	}
      }
      // now we are allowed to cleanup the frog_data
      for ( const auto& s : sents ){
	delete s;
      }
    }
    else if ( !pv.empty() ){
      // For now wu only handle the Paragraphs, ignore sentences and words
      for ( const auto& p : pv ){
	handle_one_paragraph( os, p, sentence_done );
      }
    }
    else {
      // For now we just IGNORE the loose words (backward compatability)
      for ( const auto& s : sv ){
	handle_one_sentence( os, s, sentence_done );
      }
    }
  }
}

folia::Document *FrogAPI::run_folia_engine( const string& infilename,
					    ostream& output_stream ){
  /// Run frog on a FoLiA XML file
  /*!
    \param infilename the name of the inputfile containing FoLiA
    \param output_stream the stream to output tabbed/JSON to.
    \return a Frogged FoLiA Document

    using folia::TextEngine, this function will loop through all relevant
    parents of <t> nodes in the document specified by infilename.
    The strings in those nodes will be frogged, and they are enriched with all
    found information, like POS, lemma etc.
  */
  tokenizer->reset();
  if ( options.inputclass == options.outputclass ){
    tokenizer->setFiltering(false);
  }
  if ( options.debugFlag > 0 ){
    DBG << "run_folia_engine(" << infilename << ")" << endl;
  }
  folia::TextEngine engine;
  if  (options.debugFlag > 8){
    engine.set_dbg_stream( theDbgLog );
    engine.set_debug( true );
  }
  engine.init_doc( infilename );
  engine.setup( options.inputclass, true );
  if ( engine.text_parent_count() == 0 ){
    LOG << "document contains no text in the desired inputclass: "
	<< options.inputclass << endl;
    LOG << "NO real frogging is done!" << endl;
    return 0;
  }
  else {
    folia::Document &doc = *engine.doc();
    string def_lang = tokenizer->default_language();
    if ( !def_lang.empty() ){
      if ( doc.metadata_type() == "native" ){
	doc.set_metadata( "language", def_lang );
      }
    }
    add_provenance( doc );
    int sentence_done = 0;
    folia::FoliaElement *p = 0;
    while ( (p = engine.next_text_parent() ) ){
      if ( options.debugFlag > 3 ){
	DBG << "next text parent: " << p << endl;
      }
      handle_one_text_parent( output_stream, p, sentence_done );
      if ( options.debugFlag > 0 ){
	DBG << "done with sentence " << sentence_done << endl;
      }
    }
    if ( sentence_done == 0 ){
      LOG << "Strange: didn't process any sentence...." << endl;
    }
  }
  if ( options.doXMLout ){
    return engine.doc(true); //disconnect from the engine!
  }
  return 0;
}

folia::Document *FrogAPI::run_text_engine( const string& infilename,
					   ostream& os ){
  /// Run frog on a TEXT file
  /*!
    \param infilename the name of the inputfile containing text
    \param os the stream to output tabbed/JSON to.
    \return a Frogged FoLiA Document

    this function will loop all text in the inputfile, using the tokenizer to
    detect Paragraphs and Sentences and creating a FoLiA document on the fly
  */
  ifstream test_file( infilename );
  int i = 0;
  folia::Document *doc = 0;
  folia::FoliaElement *root = 0;
  unsigned int par_count = 0;
  if ( options.doXMLout ){
    string doc_id = infilename;
    if ( options.docid != "untitled" ){
      doc_id = options.docid;
    }
    size_t xml_pos = doc_id.find( ".xml" );
    if ( xml_pos != string::npos ){
      doc_id.resize( xml_pos );
    }
    doc_id = filter_non_NC( TiCC::basename(doc_id) );
    root = start_document( doc_id, doc );
  }
  timers.tokTimer.start();
  vector<Tokenizer::Token> toks = tokenizer->tokenize_stream( test_file );
  timers.tokTimer.stop();
  while ( toks.size() > 0 ){
    frog_data res = frog_sentence( toks, ++i );
    if ( !options.noStdOut ){
      show_results( os, res );
    }
    if ( options.doXMLout ){
      root = append_to_folia( root, res, par_count );
    }
    if  (options.debugFlag > 0){
      DBG << TiCC::Timer::now() << " done with sentence[" << i << "]" << endl;
    }
    timers.tokTimer.start();
    toks = tokenizer->tokenize_stream_next();
    timers.tokTimer.stop();
  }
  return doc;
}

folia::Document *FrogAPI::FrogFile( const string& infilename ){
  /// generic function to Frog a file
  /*!
    \param infilename the input file-name
    \param os the outputstream for \e tabbed or \e JSON output
    \return a FoLiA Document. May be empty if XML output is not required.

    This function autodetects FoLiA files vs. text files and will run Frog
    for the respective types.
   */

  folia::Document *result = 0;
  bool xml_in = options.doXMLin;
  if ( TiCC::match_back( infilename, ".xml.gz" )
       || TiCC::match_back( infilename, ".xml.bz2" )
       || TiCC::match_back( infilename, ".xml" ) ){
    // auto detect (compressed) xml.
    xml_in = true;
  }
  timers.reset();
  if ( xml_in ){
    result = run_folia_engine( infilename, *outS );
  }
  else {
    result = run_text_engine( infilename, *outS );
  }
  if ( !options.hide_timers ){
    LOG << "tokenisation took:  " << timers.tokTimer << endl;
    LOG << "CGN tagging took:   " << timers.tagTimer << endl;
    if ( options.doIOB){
      LOG << "IOB chunking took:  " << timers.iobTimer << endl;
    }
    if ( options.doNER){
      LOG << "NER took:           " << timers.nerTimer << endl;
    }
    if ( options.doMbma ){
      LOG << "MBMA took:          " << timers.mbmaTimer << endl;
    }
    if ( options.doLemma ){
      LOG << "Mblem took:         " << timers.mblemTimer << endl;
    }
    if ( options.doMwu ){
      LOG << "MWU resolving took: " << timers.mwuTimer << endl;
    }
    if ( options.doAlpino ){
      LOG << "Parsing (prepare) took: " << timers.prepareTimer << endl;
      LOG << "Parsing (total)   took: " << timers.parseTimer << endl;
    }
    else if ( options.doParse ){
      LOG << "Parsing (prepare) took: " << timers.prepareTimer << endl;
      LOG << "Parsing (pairs)   took: " << timers.pairsTimer << endl;
      LOG << "Parsing (rels)    took: " << timers.relsTimer << endl;
      LOG << "Parsing (dir)     took: " << timers.dirTimer << endl;
      LOG << "Parsing (csi)     took: " << timers.csiTimer << endl;
      LOG << "Parsing (total)   took: " << timers.parseTimer << endl;
    }
    LOG << "Frogging in total took: " << timers.frogTimer + timers.tokTimer << endl;
  }
  return result;
}

int folia_diff( const string& s1, const string& s2 ){
  TiCC::tmp_stream ts1( "diff" );
  ts1.os() << s1 << endl;
  ts1.close();
  TiCC::tmp_stream ts2( "diff" );
  ts2.os() << s2 << endl;
  ts2.close();
  string cmd = "foliadiff.sh " + ts1.tmp_name() + " " + ts2.tmp_name();
  int res = system( cmd.c_str() );
  return res == 0;
}

void FrogAPI::run_api_tests( const string& testName ){
  /// Helper function to run some specific tests for the API
  /*!
    \param testName a file to test on
  */
  *outS << "running some extra Frog tests...." << endl;
  if ( testName.find( ".xml" ) != string::npos ){
    options.doXMLin = true;
    options.doXMLout = true;
  }
  else {
    options.doXMLin = false;
    options.doXMLout = false;
  }
  {
    *outS << "Start test 1: " << testName << endl;
    stringstream ss;
    ifstream is( testName );
    string line;
    while ( getline( is, line ) ){
      ss << line << endl;
    }
    string s1 = Frogtostring( ss.str() );
    string s2 = Frogtostringfromfile( testName );
    if ( options.doXMLout ){
      if ( folia_diff( s1, s2 ) ){
	*outS << "test 1 FoLiA OK!" << endl;
      }
      else {
	*outS << "FAILED FoLiA test 1 :" << testName << endl;
	*outS << "test 1 STRING 1 " << endl;
	*outS << s1 << endl;
	*outS << "test 1 STRING 2 " << endl;
	*outS << s2 << endl;
      }
    }
    else if ( s1 != s2 ){
      *outS << "FAILED test 1 :" << testName << endl;
      *outS << "test 1 STRING 1 " << endl;
      *outS << s1 << endl;
      *outS << "test 1 STRING 2 " << endl;
      *outS << s2 << endl;
    }
    else {
      *outS << "test 1 OK!" << endl;
    }
    *outS << "Done with:" << testName << endl;
  }
  //
  // also test FoLiA in en text out
  {
    if ( testName.find( ".xml" ) != string::npos ){
      options.doXMLin = true;
      options.doXMLout = false;
    }
    *outS << "Start test 2: " << testName << endl;
    stringstream ss;
    ifstream is( testName );
    string line;
    while ( getline( is, line ) ){
      ss << line << endl;
    }
    string s1 = Frogtostring( ss.str() );
    string s2 = Frogtostringfromfile( testName );
    if ( s1 != s2 ){
      *outS << "FAILED test 2:" << testName << endl;
      *outS << "test 2 STRING 1 " << endl;
      *outS << s1 << endl;
      *outS << "test 2 STRING 2 " << endl;
      *outS << s2 << endl;
    }
    else {
      *outS << "test 2 OK!" << endl;
    }
    *outS << "Done with:" << testName << endl;
  }
  //
  // and even text in and FoLiA out
  {
    if ( testName.find( ".xml" ) == string::npos ){
      options.doXMLin = false;
      options.doXMLout = true;
    }
    *outS << "Start test 3: " << testName << endl;
    stringstream ss;
    ifstream is( testName );
    string line;
    while ( getline( is, line ) ){
      ss << line << endl;
    }
    options.docid = "test";
    string s1 = Frogtostring( ss.str() );
    string s2 = Frogtostringfromfile( testName );
    if ( options.doXMLout ){
      // no simple string comparison possible. just say OK
      if ( folia_diff( s1, s2 ) ){
	*outS << "test 3 FoLiA OK!" << endl;
      }
      else {
	*outS << "FAILED FoLiA test 3 :" << testName << endl;
	*outS << "test 3 STRING 1 " << endl;
	*outS << s1 << endl;
	*outS << "test 3 STRING 2 " << endl;
	*outS << s2 << endl;
      }
    }
    else {
      if ( s1 != s2 ){
	*outS << "FAILED test 3 :" << testName << endl;
	*outS << "test 3 STRING 1 " << endl;
	*outS << s1 << endl;
	*outS << "test 3 STRING 2 " << endl;
	*outS << s2 << endl;
      }
      else {
	*outS << "test 3 OK!" << endl;
      }
    }
    if ( options.doDeepMorph ){
      *outS << "TESTING DEEP " << endl;
      folia::Document *doc = 0;
      try {
	doc = FrogFile( testName );
      }
      catch ( exception& e ){
	*outS << "problem frogging: " << testName << endl
	    << e.what() << endl;
      }
      if ( doc ){
	*outS << "GOT FOLIA, extracting WORDS " << endl;
	vector<folia::Word*> words = doc->words();
	for ( const auto& w : words ){
	  vector<string> morphs = get_full_morph_analysis( w );
	  vector<string> compounds = get_compound_analysis( w );
	  *outS << w->text() << ": " << morphs << " :: " << compounds << endl;
	}
	*outS << "DEEP_MORPH test DONE " << endl;
	delete doc;
      }
    }
    *outS << "Done with:" << testName << endl;
  }
}

const string& get_mbma_tagset( const std::string& module ){
  /// expose tagset information from de mbma module tot the world
  /*!
    \param module the specific set
    \return a string with the setname

  */

  if ( module == "mbma" ){
    return Mbma::mbma_tagset;
  }
  else if ( module == "celex" ){
    return Mbma::clex_tagset;
  }
  else if ( module == "cgn" ){
    return Mbma::pos_tagset;
  }
  else {
    string msg = "get_mbma_tagset(): Unsupported module " + module + "\n"
      + "Valid modules are: 'mbma', 'cgn' and 'celex'";
    throw runtime_error( msg );
  }
}

vector<string> get_compound_analysis( folia::Word* word ){
  /// extract compound information from the deep_morph analysis in word
  /*!
    \param word the folia::Word to extract from
    \return a vector of strings with compound information

    this function is ONLY used by TSCAN, should be moved there

  */
  vector<string> result;
  vector<folia::MorphologyLayer*> layers
    = word->annotations<folia::MorphologyLayer>( get_mbma_tagset( "mbma" ) );
  for ( const auto& layer : layers ){
    vector<folia::Morpheme*> m =
      layer->select<folia::Morpheme>( get_mbma_tagset( "mbma" ), false );
    if ( m.size() == 1 ) {
      // check for top layer compound
      folia::PosAnnotation *tag
	= m[0]->annotation<folia::PosAnnotation>( get_mbma_tagset( "celex" ) );
      if ( tag ){
	result.push_back( tag->feat( "compound" ) ); // might be empty
      }
      else {
	result.push_back( "" ); // pad with empty strings
      }
    }
  }
  return result;
}

string flatten( const string& s ){
  /// helper function to 'flatten out' bracketed morpheme strings
  /*!
    \param s a bracketed string of morphemes
    \return a string with multiple '[' and ']' reduced to single occurrences
  */
  string result;
  string::size_type bpos = s.find_first_not_of( "[" );
  if ( bpos != string::npos ){
    string::size_type epos = s.find_first_of( "]", bpos );
    result += "[" + s.substr( bpos, epos-bpos ) + "]";
    bpos = s.find_first_of( "[", epos+1 );
    bpos = s.find_first_not_of( "[", bpos );
    while ( bpos != string::npos ){
      epos = s.find_first_of( "]", bpos );
      if ( epos == string::npos ){
	break;
      }
      result += "[" + s.substr( bpos, epos-bpos ) + "]";
      bpos = s.find_first_of( "[", epos+1 );
      bpos = s.find_first_not_of( "[", bpos );
    }
  }
  else {
    result = s;
  }
  return result;
}

vector<string> get_full_morph_analysis( folia::Word *word,
					const string& cls,
					bool make_flat ){
  /// extract a full morpheme analysis from the deep_morph analysis in word
  /*!
    \param word the folia::Word to extract from
    \param cls the textclass to use
    \param make_flat When 'true' remove extra brackets
    \return a vector of strings with flattended morpheme information

    this function is ONLY used by TSCAN, should be moved there

  */
  vector<string> result;
  vector<folia::MorphologyLayer*> layers
    = word->annotations<folia::MorphologyLayer>( get_mbma_tagset( "mbma" ) );
  for ( const auto& layer : layers ){
    vector<folia::Morpheme*> m =
      layer->select<folia::Morpheme>( get_mbma_tagset( "mbma" ), false );
    bool is_deep = false;
    if ( m.size() == 1 ) {
      // check for top layer from deep morph analysis
      string str = m[0]->feat( "structure" );
      if ( !str.empty() ){
	is_deep = true;
	if ( make_flat ){
	  str = flatten(str);
	}
	result.push_back( str );
      }
    }
    if ( !is_deep ){
      // flat structure
      string morph;
      m = layer->select<folia::Morpheme>( get_mbma_tagset( "mbma" ) );
      for ( const auto& mor : m ){
	string txt = TiCC::UnicodeToUTF8( mor->text( cls ) );
	morph += "[" + txt + "]";
      }
      result.push_back( morph );
    }
  }
  return result;
}

vector<string> get_full_morph_analysis( folia::Word *word, bool make_flat ){
  /// extract a full morpheme analysis from the deep_morph analysis in word
  /*!
    \param word the folia::Word to extract from
    \param make_flat When 'true' remove extra brackets
    \return a vector of strings with flattended morpheme information

    this function is ONLY used by TSCAN, should be moved there

  */
  return get_full_morph_analysis( word, "current", make_flat );
}
