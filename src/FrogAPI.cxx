/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2018
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
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>
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
  doTok = doLemma = doMorph = doMwu = doIOB = doNER = doParse = true;
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
	myCGNTagger->set_text_redundancy( options.textredundancy );
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
	    if ( options.doDeepMorph )
	      myMbma->setDeepMorph(true);
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
	    if ( options.doDeepMorph )
	      myMbma->setDeepMorph(true);
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
	  myCGNTagger->set_text_redundancy( options.textredundancy );
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

folia::FoliaElement* FrogAPI::start_document( const string& id,
					      folia::Document *& doc ) const {
  doc = new folia::Document( "id='" + id + "'" );
  if ( options.language != "none" ){
    doc->set_metadata( "language", options.language );
  }
  doc->addStyle( "text/xsl", "folia.xsl" );
  if ( !options.doTok ){
    doc->declare( folia::AnnotationType::TOKEN, "passthru", "annotator='ucto', annotatortype='auto', datetime='now()'" );
  }
  else {
    doc->declare( folia::AnnotationType::TOKEN,
		  configuration.lookUp( "rulesFile", "tokenizer" ),
		  "annotator='ucto', annotatortype='auto', datetime='now()'");
    if ( !doc->isDeclared( folia::AnnotationType::LANG ) ){
      doc->declare( folia::AnnotationType::LANG,
		    ISO_SET, "annotator='ucto'" );
    }
  }
  myCGNTagger->addDeclaration( *doc );
  if ( options.doLemma ){
    myMblem->addDeclaration( *doc );
  }
  if ( options.doMorph ){
    myMbma->addDeclaration( *doc );
  }
  if ( options.doIOB ){
    myIOBTagger->addDeclaration( *doc );
  }
  if ( options.doNER ){
    myNERTagger->addDeclaration( *doc );
  }
  if ( options.doMwu ){
    myMwu->addDeclaration( *doc );
  }
  if ( options.doParse ){
    myParser->addDeclaration( *doc );
  }
  folia::KWargs args;
  args["id"] = doc->id() + ".text";
  folia::Text *text = new folia::Text( args );
  doc->setRoot( text );
  return text;
}

static int p_count = 0;

folia::FoliaElement *FrogAPI::append_to_folia( folia::FoliaElement *root,
					       const frog_data& fd ) const {
  folia::FoliaElement *result = root;
  folia::KWargs args;
  if ( fd.units[0].new_paragraph ){
    args["id"] = root->doc()->id() + ".p." + TiCC::toString(++p_count);
    folia::Paragraph *p = new folia::Paragraph( args, root->doc() );
    if ( root->element_id() == folia::Text_t ){
      root->append( p );
    }
    else {
      // root is a paragraph, which is done now.
      if ( options.textredundancy == "full" ){
	root->settext( root->str(options.outputclass), options.outputclass);
      }
      root->parent()->append( p );
    }
    result = p;
  }
  args.clear();
  args["generate_id"] = result->id();
  folia::Sentence *s = new folia::Sentence( args, root->doc() );
  result->append( s );
  if ( fd.language != "default"
       && options.language != "none"
       && fd.language != options.language ){
    folia::KWargs args;
    args["class"] = fd.language;
    string sett = root->doc()->defaultset( folia::AnnotationType::LANG );
    if ( !sett.empty() && sett != "default" ){
      args["set"] = sett;
    }
    folia::LangAnnotation *la = new folia::LangAnnotation( args, root->doc() );
    s->append( la );
  }
  else {
    vector<folia::Word*> wv = myCGNTagger->add_result( s, fd );
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
    if ( options.doParse ){
      myParser->add_result( fd, wv );
    }
  }
  return result;
}

void FrogAPI::append_to_sentence( folia::Sentence *sent,
				  const frog_data& fd ) const {
  string la;
  if ( sent->hasannotation<folia::LangAnnotation>() ){
    la = sent->annotation<folia::LangAnnotation>()->cls();
  }
  if (options.debugFlag > 1){
    DBG << "fd.language = " << fd.language << endl;
    DBG << "options.language = " << options.language << endl;
    DBG << "sentence language = " << la << endl;
  }
  if ( !la.empty() && la != options.language
       && options.language != "none" ){
    // skip
    if ( options.debugFlag > 0 ){
      DBG << "append_to_sentence() SKIP a sentence: " << la << endl;
    }
  }
  else {
    if ( options.language != "none"
	 && fd.language != "default"
	 && fd.language != options.language ){
      if (!sent->doc()->isDeclared( folia::AnnotationType::LANG ) ){
	sent->doc()->declare( folia::AnnotationType::LANG,
			      ISO_SET, "annotator='ucto'" );
      }
      folia::KWargs args;
      args["class"] = fd.language;
      string sett = sent->doc()->defaultset( folia::AnnotationType::LANG );
      if ( !sett.empty() && sett != "default" ){
	args["set"] = sett;
      }
      folia::LangAnnotation *la = new folia::LangAnnotation( args, sent->doc() );
      sent->append( la );
    }
    vector<folia::Word*> wv = myCGNTagger->add_result( sent, fd );
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
    if ( options.doParse ){
      myParser->add_result( fd, wv );
    }
  }
}

void FrogAPI::append_to_words( const vector<folia::Word*>& wv,
			       const frog_data& fd ) const {
  if ( fd.language != "default"
       && options.language != "none"
       && fd.language != options.language ){
    if ( options.debugFlag > 0 ){
      DBG << "append_words() SKIP a sentence: " << fd.language << endl;
    }
  }
  else {
    myCGNTagger->add_result( wv, fd );
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
      myParser->add_result( fd, wv );
    }
  }
}

void FrogAPI::FrogServer( Sockets::ServerSocket &conn ){
  if ( options.doXMLout ){
    options.noStdOut = true;
  }
  try {
    while ( conn.isValid() ) {
      ostringstream output_stream;
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
        if ( options.debugFlag > 5 ){
	  DBG << "received data [" << result << "]" << endl;
	}
	string tmp_file = tmpnam(0);
	ofstream os( tmp_file );
	os << result << endl;
	os.close();
	run_folia_processor( tmp_file, output_stream );
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
	    if ( line == "EOT" )
	      break;
	    data += line + "\n";
	  }
        }
        if ( options.debugFlag > 5 ){
	  DBG << "Received: [" << data << "]" << endl;
	}
        LOG << TiCC::Timer::now() << " Processing... " << endl;
	istringstream inputstream(data,istringstream::in);
	frog_data res = tokenizer->tokenize_stream( inputstream );
	timers.tokTimer.stop();
	folia::Document *doc = 0;
	folia::FoliaElement *root = 0;
	if ( options.doXMLout ){
	  string doc_id = "untitled";
	  root = start_document( doc_id, doc );
	}
	while ( res.size() > 0 ){
	  frog_sentence( res );
	  if ( options.doXMLout ){
	    root = append_to_folia( root, res );
	  }
	  else {
	    showResults( output_stream, res );
	  }
	  res = tokenizer->tokenize_stream( inputstream );
	}
	if ( options.doXMLout ){
	  doc->save( output_stream, options.doKanon );
	  delete doc;
	}
	//	DBG << "Done Processing... " << endl;
      }
      if (!conn.write( (output_stream.str()) ) || !(conn.write("READY\n"))  ){
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
    istringstream inputstream(data,istringstream::in);
    frog_data res = tokenizer->tokenize_stream( inputstream );
    if ( res.size() > 0 ){
      frog_sentence( res );
      showResults( cout, res );
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
	istringstream inputstream(data,istringstream::in);
	frog_data res = tokenizer->tokenize_stream( inputstream );
	frog_sentence( res );
	showResults( cout, res );
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
  options.hide_timers = true;
  string tmp_file = tmpnam(0);
  ofstream os( tmp_file );
  os << s << endl;
  os.close();
  stringstream ss;
  FrogFile( tmp_file, ss, "" );
  return ss.str();
}

string FrogAPI::Frogtostringfromfile( const string& name ){
  /// Parse a file, Frog it and return the result as a string.
  /// @s: an UTF8 decoded string. May be multilined.
  /// @return the results of frogging. Depending of the current frog settings
  /// the inputfile can be interpreted as XML, an the ouput will be XML or
  /// tab separated
  stringstream ss;
  FrogFile( name, ss, "" );
  return ss.str();
}

string get_language( frog_data& fd ){
  fd.language = "none";
  for ( const auto& r : fd.units ){
    fd.language = r.language;
    break;
  }
  return fd.language;
}

bool FrogAPI::frog_sentence( frog_data& sent ){
  string lan = get_language( sent );
  if ( !options.language.empty()
       && options.language != "none"
       && !lan.empty()
       && lan != "default"
       && lan != options.language ){
    if ( options.debugFlag >= 0 ){
       DBG << "skipping sentence (different language: " << lan
	   << " --language=" << options.language << ")" << endl;
    }
    return false;
  }
  else {
    bool showParse = options.doParse;
    timers.frogTimer.start();
    if ( options.debugFlag > 5 ){
      DBG << "Frogging sentence:" << sent << endl;
    }
    bool all_well = true;
    string exs;
    timers.tagTimer.start();
    try {
      myCGNTagger->Classify( sent );
    }
    catch ( exception&e ){
      all_well = false;
      exs += string(e.what()) + " ";
    }
    timers.tagTimer.stop();
    if ( !all_well ){
      throw runtime_error( exs );
    }
    for ( auto& word : sent.units ) {
#pragma omp parallel sections
      {
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
    cout << endl;
#pragma omp parallel sections
    {
#pragma omp section
      {
	if ( options.doNER ){
	  timers.nerTimer.start();
	  if (options.debugFlag > 1) {
	    DBG << "Calling NER..." << endl;
	  }
	  try {
	    myNERTagger->Classify( sent );
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
	    myIOBTagger->Classify( sent );
	  }
	  catch ( exception&e ){
	    all_well = false;
	    exs += string(e.what()) + " ";
	  }
	  timers.iobTimer.stop();
	}
      }
    }
    if ( !all_well ){
      throw runtime_error( exs );
    }
    if ( options.doMwu ){
      if ( !sent.empty() ){
	timers.mwuTimer.start();
	myMwu->Classify( sent );
	timers.mwuTimer.stop();
      }
    }
    if ( options.doParse ){
      if ( options.maxParserTokens != 0
	   && sent.size() > options.maxParserTokens ){
	showParse = false;
      }
      else {
	myParser->Parse( sent, timers );
      }
    }
    timers.frogTimer.stop();
    return showParse;
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
  if ( result.empty() ){
    // ouch, only numbers?
    return filter_non_NC( "N" + filename );
  }
  return result;
}

void FrogAPI::show_record( ostream& os, const frog_record& fd ) const {
  os << fd.word << TAB;
  if ( options.doLemma ){
    if ( !fd.lemmas.empty() ){
      os << fd.lemmas[0];
    }
    else {
      os << TAB;
    }
  }
  else {
    os << TAB;
  }
  os << TAB;
  if ( options.doMorph ){
    if ( fd.morphs.empty() ){
      if ( !fd.deep_morph_string.empty() ){
	os << fd.deep_morph_string << TAB;
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
  else {
    os << TAB;
  }
  os << TAB << fd.tag << TAB << fixed << showpoint << std::setprecision(6) << fd.tag_confidence;
  if ( options.doNER ){
    os << TAB << TiCC::uppercase(fd.ner_tag);
  }
  else {
    os << TAB << TAB;
  }
  if ( options.doIOB ){
    os << TAB << fd.iob_tag;
  }
  else {
    os << TAB << TAB;
  }
  if ( options.doParse ){
    os << TAB << fd.parse_index << TAB << fd.parse_role;
  }
  else {
    os << TAB << TAB << TAB << TAB;
  }
}

void FrogAPI::showResults( ostream& os,
			   const frog_data& fd ) const {
  if ( fd.mw_units.empty() ){
    for ( size_t pos=0; pos < fd.units.size(); ++pos ){
      os << pos+1 << TAB;
      show_record( os, fd.units[pos] );
      os << endl;
    }
  }
  else {
    for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
      os << pos+1 << TAB;
      show_record( os, fd.mw_units[pos] );
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

void FrogAPI::handle_one_sentence( ostream& os, folia::Sentence *s ){
  vector<folia::Word*> wv;
  wv = s->select<folia::Word>( options.inputclass );
  if ( wv.empty() ){
    wv = s->select<folia::Word>();
  }
  if ( !wv.empty() ){
    // there are already words.
    // assume unfrogged yet
    frog_data res;
    for ( const auto& w : wv ){
      frog_record rec = extract_from_word( w, options.inputclass );
      res.units.push_back( rec );
    }
    if  (options.debugFlag > 0){
      DBG << "before frog_sentence() 1" << endl;
    }
    frog_sentence( res );
    if ( !options.noStdOut ){
      showResults( os, res );
    }
    if  (options.debugFlag > 0){
      DBG << "before append_to_words()" << endl;
    }
    append_to_words( wv, res );
    if (options.debugFlag > 0){
      DBG << "after append_to_words()" << endl;
    }
  }
  else {
    string text = s->str(options.inputclass);
    cerr << "frog: " << text << endl;
    istringstream inputstream(text,istringstream::in);
    timers.tokTimer.start();
    frog_data res = tokenizer->tokenize_stream( inputstream );
    timers.tokTimer.stop();
    while ( res.size() > 0 ){
      if  (options.debugFlag > 0){
	DBG << "before frog_sentence() LOOP" << endl;
      }
      frog_sentence( res );
      if ( !options.noStdOut ){
	showResults( os, res );
      }
      if  (options.debugFlag > 0){
	DBG << "before append_to_sentence() A" << endl;
      }
      append_to_sentence( s, res );
      if  (options.debugFlag > 0){
	DBG << "after append_to_sentence() A" << endl;
      }
      timers.tokTimer.start();
      res = tokenizer->tokenize_stream( inputstream );
      timers.tokTimer.stop();
    }
  }
}

void FrogAPI::handle_one_paragraph( ostream& os,
				    folia::Paragraph *p,
				    int& sentence_done ){
  string text = p->str(options.inputclass);
  cerr << "frog: " << text << endl;
  istringstream inputstream(text,istringstream::in);
  timers.tokTimer.start();
  frog_data res = tokenizer->tokenize_stream( inputstream );
  timers.tokTimer.stop();
  while ( res.size() > 0 ){
    frog_sentence( res );
    if ( !options.noStdOut ){
      showResults( os, res );
    }
    folia::KWargs args;
    args["generate_id"] = p->id();
    folia::Sentence *s = new folia::Sentence( args, p->doc() );
    p->append( s );
    if  (options.debugFlag > 0){
      DBG << "before append_to_sentence() B" << endl;
    }
    append_to_sentence( s, res );
    if  (options.debugFlag > 0){
      DBG << "after append_to_sentence() B" << endl;
    }
    timers.tokTimer.start();
    res = tokenizer->tokenize_stream( inputstream );
    timers.tokTimer.stop();
    ++sentence_done;
  }
}

void FrogAPI::handle_one_element( ostream& os,
				  folia::FoliaElement *e,
				  int& sentence_done ){
  if ( e->xmltag() == "w" ){
    // already tokenized into words!
    folia::Word *word = dynamic_cast<folia::Word*>(e);
    frog_data res;
    frog_record tmp = extract_from_word( word, options.inputclass );
    res.units.push_back(tmp);
    frog_sentence( res );
    if ( !options.noStdOut ){
      showResults( os, res );
    }
    vector<folia::Word*> wv;
    wv.push_back( word );
    append_to_words( wv, res );
    ++sentence_done;
  }
  else if ( e->xmltag() == "s" ){
    // OK a text in a sentence
    if  (options.debugFlag > 0){
      DBG << "found text in a sentence " << e << endl;
    }
    handle_one_sentence( os, dynamic_cast<folia::Sentence*>(e) );
    ++sentence_done;
    if  (options.debugFlag > 0){
      DBG << "after handle_one_sentence()" << endl;
    }
  }
  else if ( e->xmltag() == "p" ){
    // OK a longer text in some paragraph
    if  (options.debugFlag > 0){
      DBG << "found text in a paragraph " << e << endl;
    }
    handle_one_paragraph( os,
			  dynamic_cast<folia::Paragraph*>(e),
			  sentence_done );
  }
  else {
    // Some text outside word, paragraphs or sentences (yet)
    string text = e->str(options.inputclass);
    cerr << "frog: " << text << endl;
    istringstream inputstream(text,istringstream::in);
    timers.tokTimer.start();
    frog_data res = tokenizer->tokenize_stream( inputstream );
    timers.tokTimer.stop();
    vector<folia::Sentence*> sents;
    while ( res.size() > 0 ){
      frog_sentence( res );
      if ( !options.noStdOut ){
	showResults( os, res );
      }
      folia::KWargs args;
      args["generate_id"] = e->id();
      folia::Sentence *s = new folia::Sentence( args, e->doc() );
      append_to_sentence( s, res );
      if  (options.debugFlag > 0){
	DBG << "created a new sentence: " << s << endl;
      }
      sents.push_back( s );
      timers.tokTimer.start();
      res = tokenizer->tokenize_stream( inputstream );
      timers.tokTimer.stop();
      ++sentence_done;
    }
    assert( sents.size() > 0 );
    if ( sents.size() > 1 ){
      // multiple sentences. We need a Paragraph.
      folia::KWargs args;
      args["generate_id"] = e->id();
      folia::Paragraph *p = new folia::Paragraph( args, e->doc() );
      e->append( p );
      for ( const auto& s : sents ){
	p->append( s );
	++sentence_done;
      }
    }
    else {
      // 1 sentence, connect directly.
      e->append( sents[0] );
      ++sentence_done;
    }
  }
}

void FrogAPI::run_folia_processor( const string& infilename,
				   ostream& output_stream,
				   const string& xmlOutFile ){
  if ( options.debugFlag > 0 ){
    DBG << "folia_processor(" << infilename << "," << xmlOutFile << ")" << endl;
  }
  if ( xmlOutFile.empty() ){
    options.noStdOut = false;
  }
  folia::TextProcessor proc( infilename );
  if ( !options.doTok ){
    proc.declare( folia::AnnotationType::TOKEN, "passthru",
		  "annotator='ucto', annotatortype='auto', datetime='now()'" );
  }
  else {
    if ( !proc.is_declared( folia::AnnotationType::LANG ) ){
      proc.declare( folia::AnnotationType::LANG,
		    ISO_SET, "annotator='ucto'" );
    }
    string languages = configuration.lookUp( "languages", "tokenizer" );
    if ( !languages.empty() ){
      vector<string> language_list;
      language_list = TiCC::split_at( languages, "," );
      proc.set_metadata( "language", language_list[0] );
      for ( const auto& l : language_list ){
	proc.declare( folia::AnnotationType::TOKEN,
		      "tokconfig-" + l,
		      "annotator='ucto', annotatortype='auto', datetime='now()'");
      }
    }
    else if ( options.language == "none" ){
      proc.declare( folia::AnnotationType::TOKEN,
		    "tokconfig-nld",
		    "annotator='ucto', annotatortype='auto', datetime='now()'");
    }
    else {
      proc.declare( folia::AnnotationType::TOKEN,
		    "tokconfig-" + options.language,
		    "annotator='ucto', annotatortype='auto', datetime='now'");
      proc.set_metadata( "language", options.language );
    }
  }
  if  (options.debugFlag > 8){
    proc.set_debug( true );
  }
  myCGNTagger->addDeclaration( proc );
  if ( options.doLemma ){
    myMblem->addDeclaration( proc );
  }
  if ( options.doMorph ){
    myMbma->addDeclaration( proc );
  }
  if ( options.doIOB ){
    myIOBTagger->addDeclaration( proc );
  }
  if ( options.doNER ){
    myNERTagger->addDeclaration( proc );
  }
  if ( options.doMwu ){
    myMwu->addDeclaration( proc );
  }
  if ( options.doParse ){
    myParser->addDeclaration( proc );
  }
  proc.setup( options.inputclass, true );
  int sentence_done = 0;
  folia::FoliaElement *p = 0;
  while ( (p = proc.next_text_parent() ) ){
    handle_one_element( output_stream, p, sentence_done );
    if ( proc.next() ){
      if  (options.debugFlag > 0){
	DBG << "looping for more ..." << endl;
      }
    }
  }
  if ( sentence_done == 0 ){
    LOG << "document contains no sentences or paragraphs!" << endl;
     LOG << "NO result!" << endl;
     return;
  }
  if ( !xmlOutFile.empty() ){
    proc.save( xmlOutFile, options.doKanon );
    LOG << "resulting FoLiA doc saved in " << xmlOutFile << endl;
  }
  else if ( options.doXMLout ){
    proc.save( output_stream, options.doKanon );
  }
}

void FrogAPI::FrogFile( const string& infilename,
			ostream& os,
			const string& xmlOutF ) {
  // stuff the whole input into one FoLiA document.
  // This is not a good idea on the long term, I think (agreed [proycon] )
  string xmlOutFile = xmlOutF;
  bool xml_in = options.doXMLin;
  if ( TiCC::match_back( infilename, ".xml.gz" )
       || TiCC::match_back( infilename, ".xml.bz2" )
       || TiCC::match_back( infilename, ".xml" ) ){
    xml_in = true;
  }
  if ( xml_in && !xmlOutFile.empty() ){
    if ( TiCC::match_back( infilename, ".gz" ) ){
      if ( !TiCC::match_back( xmlOutFile, ".gz" ) )
	xmlOutFile += ".gz";
    }
    else if ( TiCC::match_back( infilename, ".bz2" ) ){
      if ( !TiCC::match_back( xmlOutFile, ".bz2" ) )
	xmlOutFile += ".bz2";
    }
  }
  if ( xml_in ){
    timers.reset();
    run_folia_processor( infilename, os, xmlOutFile );
  }
  else {
    ifstream TEST( infilename );
    int i = 0;
    folia::Document *doc1 = 0;
    folia::FoliaElement *root = 0;
    if ( !xmlOutFile.empty() ){
      string doc_id = infilename;
      if ( options.docid != "untitled" ){
	doc_id = options.docid;
      }
      doc_id = doc_id.substr( 0, doc_id.find( ".xml" ) );
      doc_id = filter_non_NC( TiCC::basename(doc_id) );
      root = start_document( doc_id, doc1 );
      p_count = 0;
    }
    timers.reset();
    timers.tokTimer.start();
    frog_data res = tokenizer->tokenize_stream( TEST );
    timers.tokTimer.stop();
    while ( res.size() > 0 ){
      ++i;
      bool showParse = frog_sentence( res );
      if ( !options.noStdOut ){
	showResults( os, res );
      }
      if ( options.doParse && !showParse ){
	LOG << "WARNING!" << endl;
	LOG << "Sentence " << i
	    << " isn't parsed because it contains more tokens then set with the --max-parser-tokens="
	    << options.maxParserTokens << " option." << endl;
      }
      else {
	if  (options.debugFlag > 0){
	  DBG << TiCC::Timer::now() << " done with sentence[" << i << "]" << endl;
	}
	if ( !xmlOutFile.empty() ){
	  root = append_to_folia( root, res );
	}
      }
      timers.tokTimer.start();
      res = tokenizer->tokenize_stream( TEST );
      timers.tokTimer.stop();
    }
    if ( !xmlOutFile.empty() ){
      doc1->save( xmlOutFile, options.doKanon );
      LOG << "resulting FoLiA doc saved in " << xmlOutFile << endl;
      delete doc1;
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
      LOG << "Frogging in total took: " << timers.frogTimer << endl;
    }
  }
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
