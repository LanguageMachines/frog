/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
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
#include <signal.h>
#include <algorithm>
#include <string>
#include <cstring>
#include <iostream>
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


using namespace std;
using namespace Tagger;
using TiCC::operator<<;

#define LOG *TiCC::Log(theErrLog)
#define DBG *TiCC::Log(theDbgLog)

const string ISO_SET = "http://raw.github.com/proycon/folia/master/setdefinitions/iso639_3.foliaset";

string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/";
string configFileName = configDir + "frog.cfg";

string FrogAPI::defaultConfigDir( const string& lang ){
  if ( lang.empty() ){
    return configDir;
  }
  else {
    return configDir+lang+"/";
  }
}

string FrogAPI::defaultConfigFile( const string& lang ){
  return defaultConfigDir( lang ) + "frog.cfg";
}

FrogOptions::FrogOptions() {
  doTok = doLemma = doMorph = doMwu = doIOB = doNER = doParse = doTagger = true;
  doDeepMorph = false;
  doSentencePerLine = false;
  doQuoteDetection = false;
  doDirTest = false;
  doRetry = false;
  noStdOut = false;
  doServer = false;
  doXMLin =  false;
  doXMLout =  false;
  doKanon =  false;
  test_API =  false;
  hide_timers = false;
  interactive = false;

  maxParserTokens = 500; // 500 words in a sentence is already insane
  // needs about 16 Gb memory to parse!
  // set tot 0 for unlimited
#ifdef HAVE_OPENMP
  numThreads = min<int>( 8, omp_get_max_threads() ); // ok, don't overdo
#else
  numThreads = 1;
#endif
  uttmark = "<utt>";
  listenport = "void";
  docid = "untitled";
  inputclass="current";
  outputclass="current";
  textredundancy="minimal";
  debugFlag = 0;
}

void FrogAPI::test_version( const string& where, double minimum ){
  string version = configuration.lookUp( "version", where );
  double v = 0.0;
  if ( !version.empty() ){
    if ( !TiCC::stringTo( version, v ) ){
      v = 0.5;
    }
  }
  if ( where == "IOB" ){
    if ( v < minimum ){
      LOG << "[[" << where << "]] Wrong FrogData!. "
	  << "Expected version " << minimum << " or higher for module: "
	  << where << endl;
      if ( version.empty() ) {
	LOG << "but no version info was found!." << endl;
      }
      else {
	LOG << "but found version " << v << endl;
      }
      throw runtime_error( "Frog initialization failed" );
    }
  }
  else if ( where == "NER" ){
    if ( v < minimum ){
      LOG << "[[" << where << "]] Wrong FrogData!. "
	  << "Expected version " << minimum << " or higher for module: "
	  << where << endl;
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
    throw logic_error( "unknown where:" + where );
  }
}

FrogAPI::FrogAPI( FrogOptions &opt,
		  const TiCC::Configuration &conf,
		  TiCC::LogStream *err_log,
		  TiCC::LogStream *dbg_log ):
  configuration(conf),
  options(opt),
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
  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  if ( options.doTok && !configuration.hasSection("tagger") ){
    LOG << "Missing [[tagger]] section in config file." << endl;
    LOG << "cannot run without a tagger!" << endl;
    throw runtime_error( "Frog initialization failed" );
  }
  if ( options.doTok && !configuration.hasSection("tokenizer") ){
    LOG << "Missing [[tokenizer]] section in config file." << endl;
    LOG << "Disabled the tokenizer." << endl;
    options.doTok = false;
  }
  if ( options.doMorph && !configuration.hasSection("mbma") ){
    LOG << "Missing [[mbma]] section in config file." << endl;
    LOG << "Disabled the Morhological analyzer." << endl;
    options.doMorph = false;
  }
  if ( options.doIOB ){
    if ( !configuration.hasSection("IOB") ){
      LOG << "Missing [[IOB]] section in config file." << endl;
      LOG << "Disabled the IOB Chunker." << endl;
      options.doIOB = false;
    }
    else {
      test_version( "IOB", 2.0 );
    }
  }
  if ( options.doNER ) {
    if ( !configuration.hasSection("NER") ){
      LOG << "Missing [[NER]] section in config file." << endl;
      LOG << "Disabled the NER." << endl;
      options.doNER = false;
    }
    else {
      test_version( "NER", 2.0 );
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
      tokenizer->setTextRedundancy( options.textredundancy );
      myCGNTagger = new CGNTagger( theErrLog,theDbgLog );
      stat = myCGNTagger->init( configuration );
      if ( stat ){
	myCGNTagger->set_eos_mark( options.uttmark );
	if ( options.doIOB ){
	  myIOBTagger = new IOBTagger( theErrLog, theDbgLog );
	  stat = myIOBTagger->init( configuration );
	  if ( stat ){
	    myIOBTagger->set_eos_mark( options.uttmark );
	  }
	}
	if ( stat && options.doNER ){
	  myNERTagger = new NERTagger( theErrLog, theDbgLog );
	  stat = myNERTagger->init( configuration );
	  if ( stat ){
	    myNERTagger->set_eos_mark( options.uttmark );
	  }
	}
	if ( stat && options.doLemma ){
	  myMblem = new Mblem(theErrLog,theDbgLog);
	  stat = myMblem->init( configuration );
	}
	if ( stat && options.doMorph ){
	  myMbma = new Mbma(theErrLog,theDbgLog);
	  stat = myMbma->init( configuration );
	  if ( stat ) {
	    if ( options.doDeepMorph ){
	      myMbma->setDeepMorph(true);
	    }
	  }
	}
	if ( stat && options.doMwu ){
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
      DBG << "running on " << curt
		      << " threads" << endl;
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
	  tokenizer->setTextRedundancy( options.textredundancy );
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
	if ( options.doMorph ){
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
	if ( options.doMwu ){
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
  delete myMbma;
  delete myMblem;
  delete myMwu;
  delete myCGNTagger;
  delete myIOBTagger;
  delete myNERTagger;
  delete myParser;
  delete tokenizer;
}

folia::processor *FrogAPI::add_provenance( folia::Document& doc ) const {
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
  if ( options.doMorph ){
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
  if ( options.doParse ){
    myParser->add_provenance( doc, proc );
  }
  return proc;
}

folia::FoliaElement* FrogAPI::start_document( const string& id,
					      folia::Document *& doc ) const {
  doc = new folia::Document( "xml:id='" + id + "'" );
  doc->addStyle( "text/xsl", "folia.xsl" );
  if ( options.debugFlag > 1 ){
    DBG << "start document!!!" << endl;
  }
  add_provenance( *doc );
  folia::KWargs args;
  args["xml:id"] = doc->id() + ".text";
  folia::Text *text = new folia::Text( args );
  doc->addText( text );
  return text;
}

void FrogAPI::append_to_sentence( folia::Sentence *sent,
				  const frog_data& fd ) const {
  // add tokenization, when applicable
  vector<folia::Word*> wv = tokenizer->add_words( sent,
						  fd );
  string la;
  if ( sent->has_annotation<folia::LangAnnotation>() ){
    la = sent->annotation<folia::LangAnnotation>()->cls();
  }
  string def_lang = tokenizer->default_language();
  if ( options.debugFlag > 1 ){
    DBG << "append_to_sentence()" << endl;
    DBG << "fd.language = " << fd.language << endl;
    DBG << "default_language = " << def_lang << endl;
    DBG << "sentence language = '" << la << "'" << endl;
  }
  if ( la.empty()
       && !fd.language.empty()
       && fd.language != "default"
       && fd.language != def_lang ){
    //
    // so the language is non default, and not set
    //
    Tokenizer::set_language( sent, fd.language );
    if ( options.textredundancy == "full" ){
      sent->settext( sent->str(options.inputclass), options.inputclass );
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
    if ( options.doMorph ){
      myMbma->add_morphemes( wv, fd );
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
    if ( options.doParse ){
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
    folia::Paragraph *p = new folia::Paragraph( args, root->doc() );
    if ( root->element_id() == folia::Text_t ){
      if  (options.debugFlag > 5 ){
	DBG << "append_to_folia, add paragraph to Text" << endl;
      }
      root->append( p );
    }
    else {
      // root is a paragraph, which is done now.
      if ( options.textredundancy == "full" ){
	root->settext( root->str(options.outputclass), options.outputclass);
      }
      if  (options.debugFlag > 5 ){
	DBG << "append_to_folia, add paragraph to parent of " << root << endl;
      }
      root->parent()->append( p );
    }
    result = p;
  }
  args.clear();
  args["generate_id"] = result->id();
  folia::Sentence *s = new folia::Sentence( args, root->doc() );
  result->append( s );
  if  (options.debugFlag > 5 ){
    DBG << "append_to_folia, created Sentence" << s << endl;
  }
  append_to_sentence( s, fd );
  if  (options.debugFlag > 5 ){
    DBG << "append_to_folia, done, result node = " << result << endl;
  }
  return result;
}

void FrogAPI::append_to_words( const vector<folia::Word*>& wv,
			       const frog_data& fd ) const {
  string def_lang = tokenizer->default_language();
  if ( fd.language != "default"
       && fd.language != def_lang ){
    if ( options.debugFlag > 0 ){
      DBG << "append_words() SKIP a sentence (different language: "
	  << fd.language << "), default= " << def_lang << endl;
    }
  }
  else {
    if ( options.doTagger ){
      myCGNTagger->add_tags( wv, fd );
    }
    if ( options.doLemma ){
      myMblem->add_lemmas( wv, fd );
    }
    if ( options.doMorph ){
      myMbma->add_morphemes( wv, fd );
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
    if ( options.doParse && wv.size() > 1 ){
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

void FrogAPI::FrogServer( Sockets::ServerSocket &conn ){
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
	string tmp_file = tmpnam(0);
	ofstream os( tmp_file );
	os << result << endl;
	os.close();
	folia::Document *xml = run_folia_engine( tmp_file, output_stream );
	if ( xml && options.doXMLout ){
	  xml->set_kanon(options.doKanon);
	  output_stream << xml;
	  delete xml;
	}
	LOG << "Done Processing XML... " << endl;
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
	// tokenize_line() delivers 1 sentence at a time and should
	//  be called multiple times to get all sentences!
	vector<Tokenizer::Token> toks = tokenizer->tokenize_line( data );
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
	  toks = tokenizer->tokenize_line_next();
	  timers.tokTimer.stop();
	}
	if ( options.doXMLout && doc ){
	  doc->save( output_stream, options.doKanon );
	  delete doc;
	}
	//	DBG << "Done Processing... " << endl;
      }
      if (!conn.write( (output_stream.str()) ) || !(conn.write("READY\n\n"))  ){
	if (options.debugFlag > 5 ) {
	  DBG << "socket " << conn.getMessage() << endl;
	}
	throw( runtime_error( "write to client failed" ) );
      }
    }
  }
  catch ( std::exception& e ) {
    LOG << "connection lost unexpected : " << e.what() << endl;
  }
  LOG << "Connection closed.\n";
}

void FrogAPI::FrogStdin( bool prompt ) {
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
    vector<Tokenizer::Token> toks = tokenizer->tokenize_line( data );
    while ( toks.size() > 0 ){
      frog_data res = frog_sentence( toks, 1 );
      show_results( cout, res );
      toks = tokenizer->tokenize_line_next();
    }
    if ( prompt ){
      cout << "frog>"; cout.flush();
    }
  }
  if ( prompt ){
    cout << "Done.\n";
  }
}

void FrogAPI::FrogInteractive(){
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
	  char *input = readline( prompt );
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
	  data = data.substr( 0, data.size()-1 );
	}
	cout << "Processing... '" << data << "'" << endl;
	vector<Tokenizer::Token> toks = tokenizer->tokenize_line( data );
	while ( !toks.empty() ){
	  frog_data res = frog_sentence( toks, 1 );
	  show_results( cout, res );
	  toks = tokenizer->tokenize_line_next();
	}
      }
    }
    cout << "Done.\n";
  }
#endif
}

string FrogAPI::Frogtostring( const string& s ){
  /// Parse a string, Frog it and return the result as a string.
  /// @s: an UTF8 decoded string. May be multilined.
  /// @return the results of frogging. Depending of the current frog settings
  /// the input can be interpreted as XML, an the ouput will be XML or
  /// tab separated
  if ( s.empty() ){
    return s;
  }
  string tmp_file = tmpnam(0);
  ofstream os( tmp_file );
  os << s << endl;
  os.close();
  return Frogtostringfromfile( tmp_file );
}

string FrogAPI::Frogtostringfromfile( const string& infilename ){
  /// Parse a file, Frog it and return the result as a string.
  /// @name: The filename.
  /// @return the results of frogging. Depending of the current frog settings
  /// the inputfile can be interpreted as XML, an the ouput will be XML or
  /// tab separated
  options.hide_timers = true;
  bool old_val = options.noStdOut;
  stringstream ss;
  if ( options.doXMLout ){
    options.noStdOut = true;
  }
  folia::Document *result = FrogFile( infilename, ss );
  options.noStdOut = old_val;
  if ( result ){
    result->set_kanon( options.doKanon );
    ss << result;
    delete result;
  }
  return ss.str();
}

string get_language( frog_data& fd ){
  fd.language = "default";
  for ( const auto& r : fd.units ){
    if ( !r.language.empty() ){
      fd.language = r.language;
    }
    break;
  }
  return fd.language;
}

frog_data extract_fd( vector<Tokenizer::Token>& tokens ){
  frog_data result;
  int quotelevel = 0;
  while ( !tokens.empty() ){
    const auto tok = tokens.front();
    tokens.erase(tokens.begin());
    frog_record tmp;
    tmp.word = TiCC::UnicodeToUTF8(tok.us);
    tmp.token_class = TiCC::UnicodeToUTF8(tok.type);
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
    if ( (tok.role & Tokenizer::TokenRole::ENDOFSENTENCE) ){
      // we are at ENDOFSENTENCE.
      // when quotelevel == 0, we step out, until the next call
      if ( quotelevel == 0 ){
	result.language = tok.lang_code;
	break;
      }
    }
  }
  return result;
}

frog_data FrogAPI::frog_sentence( vector<Tokenizer::Token>& sent,
				  const size_t s_count ){
  if ( options.debugFlag > 0 ){
    DBG << "tokens:\n" << sent << endl;
  }
  frog_data sentence = extract_fd( sent );
  string lan = get_language( sentence );
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
	  if ( options.doMorph ){
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
    if ( options.doParse ){
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
	DBG << 	"Sentence is too long: " << s_count << endl
	    << sentence.sentence(true) << endl;
      }
    }
    timers.frogTimer.stop();
    if ( options.debugFlag > 5 ){
      DBG << "Frogged one sentence:\n" << sentence << endl;
    }
    return sentence;
  }
}

string filter_non_NC( const string& filename ){
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
  ///
  /// output a frog_record @fd in tabbed format to stream @os
  /// This done in a backward compatible manor to older Frog versions
  ///
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
  if ( options.doMorph ){
    if ( fd.morphs.empty() ){
      if ( !fd.deep_morph_string.empty() ){
	os << fd.deep_morph_string << Tab;
	if ( fd.compound_string == "0"  ){
	  os << "0";
	}
	else {
	  os << fd.compound_string + "-compound";
	}
      }
    }
    else {
      os << fd.morph_string;
    }
  }
  if ( options.doTagger ){
    if ( fd.tag.empty() ){
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
    os << Tab << TiCC::uppercase(fd.ner_tag);
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
  if ( options.doParse ){
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

void FrogAPI::show_results( ostream& os,
			    const frog_data& fd ) const {
  ///
  /// output a frog_data structure @fd in tabbed format to stream @os
  /// This done in a backward compatible manor to older Frog versions
  ///
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

frog_record extract_from_word( const folia::Word* word,
			       const string& textclass ){
  frog_record rec;
  rec.word = word->str(textclass);
  folia::KWargs atts = word->collectAttributes();
  if ( atts.find( "space" ) != atts.end() ){
    rec.no_space = true;
  }
  if ( atts.find( "class" ) != atts.end() ){
    rec.token_class = atts["class"];
  }
  else {
    rec.token_class = "WORD";
  }
  rec.language = word->language();
  return rec;
}

UnicodeString replace_spaces( const UnicodeString& in ) {
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


void FrogAPI::handle_one_sentence( ostream& os,
				   folia::Sentence *s,
				   const size_t s_cnt ){
  if  ( options.debugFlag > 1 ){
    DBG << "handle_one_sentence: " << s << endl;
  }
  string sent_lang =  s->language();
  if ( sent_lang.empty() ){
    sent_lang = tokenizer->default_language();
  }
  if ( options.languages.find( sent_lang ) == options.languages.end() ){
    // ignore this language!
    if ( options.debugFlag > 0 ){
      DBG << sent_lang << " NOT in: " << options.languages << endl;
    }
    return;
  }
  vector<folia::Word*> wv;
  wv = s->words( options.inputclass );
  if ( wv.empty() ){
    wv = s->words();
  }
  if ( !wv.empty() ){
    // there are already words.
    // assume unfrogged yet
    string text;
    for ( const auto& w : wv ){
      UnicodeString tmp = w->unicode( options.inputclass );
      tmp = replace_spaces( tmp );
      text += TiCC::UnicodeToUTF8(tmp) + " ";
    }
    if  ( options.debugFlag > 1 ){
      DBG << "handle_one_sentence() on existing words" << endl;
      DBG << "handle_one_sentence() untokenized string: '" << text << "'" << endl;
    }
    vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text );
    // cerr << "text:" << text << " size=" << wv.size() << endl;
    // cerr << "tokens:" << toks << " size=" << toks.size() << endl;
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
    string text = s->str(options.inputclass);
    if ( options.debugFlag > 0 ){
      DBG << "handle_one_sentence() from string: '" << text << "' (lang="
	  << sent_lang << ")" << endl;
    }
    timers.tokTimer.start();
    vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text, sent_lang );
    timers.tokTimer.stop();
    while ( toks.size() > 0 ){
      frog_data sent = frog_sentence( toks, s_cnt );
      if ( !options.noStdOut ){
	show_results( os, sent );
      }
      if ( options.doXMLout ){
	append_to_sentence( s, sent );
      }
      timers.tokTimer.start();
      toks = tokenizer->tokenize_line_next();
      timers.tokTimer.stop();
    }
  }
}

void FrogAPI::handle_one_paragraph( ostream& os,
				    folia::Paragraph *p,
				    int& sentence_done ){
  // a Paragraph may contain both Word and Sentence nodes
  // if so, the Sentences should be handled separately
  vector<folia::Word*> wv = p->select<folia::Word>(false);
  vector<folia::Sentence*> sv = p->select<folia::Sentence>(false);
  if ( options.debugFlag > 1 ){
    DBG << "found some Words " << wv << endl;
    DBG << "found some Sentences " << sv << endl;
  }
  if ( sv.empty() ){
    // No Sentence, so only words OR just text
    string text = p->str(options.inputclass);
    if ( options.debugFlag > 0 ){
      DBG << "handle_one_paragraph:" << text << endl;
    }
    timers.tokTimer.start();
    vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text );
    timers.tokTimer.stop();
    while ( toks.size() > 0 ){
      frog_data res = frog_sentence( toks, ++sentence_done );
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
	  folia::Sentence *s = new folia::Sentence( args, p->doc() );
	  p->append( s );
	  append_to_sentence( s, res );
	}
	res = frog_sentence( toks, ++sentence_done );
      }
      timers.tokTimer.start();
      toks = tokenizer->tokenize_line_next();
      timers.tokTimer.stop();
    }
  }
  else {
    // For now wu just IGNORE the loose words (backward compatability)
    for ( const auto& s : sv ){
      handle_one_sentence( os, s, sentence_done );
    }
  }
}

void FrogAPI::handle_one_text_parent( ostream& os,
				      folia::FoliaElement *e,
				      int& sentence_done ){
  ///
  /// input is a FoLiA element @e containing text.
  /// this can be a Word, Sentence, Paragraph or some other element
  /// In the latter case, we construct a Sentence from the text, and
  /// a Paragraph if more then one Sentence is found
  ///
  if ( e->xmltag() == "w" ){
    // already tokenized into words!
    folia::Word *word = dynamic_cast<folia::Word*>(e);
    UnicodeString utext = word->unicode( options.inputclass );
    string text = TiCC::UnicodeToUTF8(replace_spaces( utext ));
    vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text );
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
      string text = e->str(options.inputclass);
      if ( options.debugFlag > 1 ){
	DBG << "frog-" << e->xmltag() << ":" << text << endl;
      }
      timers.tokTimer.start();
      vector<Tokenizer::Token> toks = tokenizer->tokenize_line( text );
      timers.tokTimer.stop();
      vector<frog_data> sents;
      while ( toks.size() > 0 ){
	frog_data res = frog_sentence( toks, ++sentence_done );
	sents.push_back( res );
	if ( !options.noStdOut ){
	  show_results( os, res );
	}
	timers.tokTimer.start();
	toks = tokenizer->tokenize_line_next( );
	timers.tokTimer.stop();
      }
      if ( options.doXMLout ){
	if ( sents.size() == 0 ){
	  // might happen in rare cases
	  // just skip
	}
	else if ( sents.size() > 1 ){
	  // multiple sentences. We need an extra Paragraph.
	  folia::KWargs args;
	  string e_id = e->id();
	  if ( !e_id.empty() ){
	    args["generate_id"] = e_id;
	  }
	  folia::Paragraph *p = new folia::Paragraph( args, e->doc() );
	  e->append( p );
	  for ( const auto& sent : sents ){
	    folia::KWargs args;
	    string p_id = p->id();
	    if ( !p_id.empty() ){
	      args["generate_id"] = p_id;
	    }
	    folia::Sentence *s = new folia::Sentence( args, e->doc() );
	    append_to_sentence( s, sent );
	    if  (options.debugFlag > 0){
	      DBG << "created a new sentence: " << s << endl;
	    }
	    p->append( s );
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
	  folia::Sentence *s = new folia::Sentence( args, e->doc() );
	  append_to_sentence( s, sents[0] );
	  if  (options.debugFlag > 0){
	    DBG << "created a new sentence: " << s << endl;
	  }
	  e->append( s );
	}
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
    if ( engine.next() ){
      if ( options.debugFlag > 1 ){
	DBG << "looping for more ..." << endl;
      }
    }
  }
  if ( sentence_done == 0 ){
    LOG << "document contains no text in the desired inputclass: "
	<< options.inputclass << endl;
    LOG << "NO result!" << endl;
    return 0;
  }
  if ( options.doXMLout ){
    return engine.doc(true); //disconnect from the engine!
  }
  return 0;
}

folia::Document *FrogAPI::run_text_engine( const string& infilename,
					   ostream& os ){
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
    doc_id = doc_id.substr( 0, doc_id.find( ".xml" ) );
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

folia::Document *FrogAPI::FrogFile( const string& infilename,
				    ostream& os ){
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
    result = run_folia_engine( infilename, os );
  }
  else {
    result = run_text_engine( infilename, os );
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
    if ( options.doMorph ){
      LOG << "MBMA took:          " << timers.mbmaTimer << endl;
    }
    if ( options.doLemma ){
      LOG << "Mblem took:         " << timers.mblemTimer << endl;
    }
    if ( options.doMwu ){
      LOG << "MWU resolving took: " << timers.mwuTimer << endl;
    }
    if ( options.doParse ){
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

// the functions below here are ONLY used by TSCAN.
// the should be moved there probably
// =======================================================================
vector<string> get_compound_analysis( folia::Word* word ){
  vector<string> result;
  vector<folia::MorphologyLayer*> layers
    = word->annotations<folia::MorphologyLayer>( Mbma::mbma_tagset );
  for ( const auto& layer : layers ){
    vector<folia::Morpheme*> m =
      layer->select<folia::Morpheme>( Mbma::mbma_tagset, false );
    if ( m.size() == 1 ) {
      // check for top layer compound
      try {
	folia::PosAnnotation *tag = m[0]->annotation<folia::PosAnnotation>( Mbma::clex_tagset );
	result.push_back( tag->feat( "compound" ) ); // might be empty
      }
      catch (...){
	result.push_back( "" ); // pad with empty strings
      }
    }
  }
  return result;
}

string flatten( const string& s ){
  string result;
  string::size_type bpos = s.find_first_not_of( "[" );
  if ( bpos != string::npos ){
    string::size_type epos = s.find_first_of( "]", bpos );
    result += "[" + s.substr( bpos, epos-bpos ) + "]";
    bpos = s.find_first_of( "[", epos+1 );
    bpos = s.find_first_not_of( "[", bpos );
    while ( bpos != string::npos ){
      string::size_type epos = s.find_first_of( "]", bpos );
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

vector<string> get_full_morph_analysis( folia::Word* w,
                                       const string& cls,
                                       bool flat ){
  vector<string> result;
  vector<folia::MorphologyLayer*> layers
    = w->annotations<folia::MorphologyLayer>( Mbma::mbma_tagset );
  for ( const auto& layer : layers ){
    vector<folia::Morpheme*> m =
      layer->select<folia::Morpheme>( Mbma::mbma_tagset, false );
    bool is_deep = false;
    if ( m.size() == 1 ) {
      // check for top layer from deep morph analysis
      string str  = m[0]->feat( "structure" );
      if ( !str.empty() ){
	is_deep = true;
	if ( flat ){
	  str = flatten(str);
	}
	result.push_back( str );
      }
    }
    if ( !is_deep ){
      // flat structure
      string morph;
      vector<folia::Morpheme*> m
	= layer->select<folia::Morpheme>( Mbma::mbma_tagset );
      for ( const auto& mor : m ){
	string txt = TiCC::UnicodeToUTF8( mor->text( cls ) );
	morph += "[" + txt + "]";
      }
      result.push_back( morph );
    }
  }
  return result;
}

vector<string> get_full_morph_analysis( folia::Word* w, bool flat ){
  return get_full_morph_analysis( w, "current", flat );
}
