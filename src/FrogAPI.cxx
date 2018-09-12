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
		  TiCC::LogStream *log ):
  configuration(conf),
  options(opt),
  theErrLog(log),
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
    tokenizer = new UctoTokenizer(theErrLog);
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
      myCGNTagger = new CGNTagger( theErrLog );
      stat = myCGNTagger->init( configuration );
      if ( stat ){
	myCGNTagger->set_eos_mark( options.uttmark );
	myCGNTagger->set_text_redundancy( options.textredundancy );
	if ( options.doIOB ){
	  myIOBTagger = new IOBTagger( theErrLog );
	  stat = myIOBTagger->init( configuration );
	  if ( stat ){
	    myIOBTagger->set_eos_mark( options.uttmark );
	  }
	}
	if ( stat && options.doNER ){
	  myNERTagger = new NERTagger( theErrLog );
	  stat = myNERTagger->init( configuration );
	  if ( stat ){
	    myNERTagger->set_eos_mark( options.uttmark );
	  }
	}
	if ( stat && options.doLemma ){
	  myMblem = new Mblem(theErrLog);
	  stat = myMblem->init( configuration );
	}
	if ( stat && options.doMorph ){
	  myMbma = new Mbma(theErrLog);
	  stat = myMbma->init( configuration );
	  if ( stat ) {
	    if ( options.doDeepMorph )
	      myMbma->setDeepMorph(true);
	  }
	}
	if ( stat && options.doMwu ){
	  myMwu = new Mwu(theErrLog);
	  stat = myMwu->init( configuration );
	  if ( stat && options.doParse ){
	    myParser = new Parser(theErrLog);
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
      LOG << "running on " << curt
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
	  tokenizer = new UctoTokenizer(theErrLog);
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
	    myMblem = new Mblem(theErrLog);
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
	    myMbma = new Mbma(theErrLog);
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
	  myCGNTagger = new CGNTagger( theErrLog );
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
	    myIOBTagger = new IOBTagger( theErrLog );
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
	    myNERTagger = new NERTagger( theErrLog );
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
	    myMwu = new Mwu(theErrLog);
	    mwuStat = myMwu->init( configuration );
	    if ( mwuStat && options.doParse ){
	      TiCC::Timer initTimer;
	      initTimer.start();
	      try {
		myParser = new Parser(theErrLog);
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
  doc->append( text );
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
      myNERTagger->add_result( s, fd, wv );
    }
    if ( options.doIOB ){
      myIOBTagger->add_result( s, fd, wv );
    }
    if ( options.doMwu && !fd.mwus.empty() ){
      myMwu->add_result( s, fd, wv );
    }
    if ( options.doParse ){
      myParser->add_result( s, fd, wv );
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
    LOG << "fd.language = " << fd.language << endl;
    LOG << "options.language = " << options.language << endl;
    LOG << "sentence language = " << la << endl;
  }
  if ( !la.empty() && la != options.language
       && options.language != "none" ){
    // skip
    if ( options.debugFlag > 0 ){
      LOG << "append_to_sentence() SKIP a sentence: " << la << endl;
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
      myNERTagger->add_result( sent, fd, wv );
    }
    if ( options.doIOB ){
      myIOBTagger->add_result( sent, fd, wv );
    }
    if ( options.doMwu && !fd.mwus.empty() ){
      myMwu->add_result( sent, fd, wv );
    }
    if ( options.doParse ){
      myParser->add_result( sent, fd, wv );
    }
  }
}

void FrogAPI::append_to_words( const vector<folia::Word*>& wv,
			       const frog_data& fd ) const {
  if ( fd.language != "default"
       && options.language != "none"
       && fd.language != options.language ){
    if ( options.debugFlag > 0 ){
      LOG << "append_words() SKIP a sentence: " << fd.language << endl;
    }
  }
  else {
    LOG << "in append_to_words() 1" << endl;
    myCGNTagger->add_result( wv, fd );
    if ( options.doLemma ){
      myMblem->add_lemmas( wv, fd );
    }
    LOG << "in append_to_words() 2" << endl;    if ( options.doMorph ){
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

bool FrogAPI::TestSentence( folia::Sentence* sent, TimerBlock& timers ){
  vector<folia::Word*> swords;
  if ( options.doQuoteDetection ){
    swords = sent->wordParts();
  }
  else {
    swords = sent->words();
  }
  bool showParse = options.doParse;
  if ( !swords.empty() ) {
    bool all_well = true;
    string exs;
    timers.tagTimer.start();
    try {
      myCGNTagger->Classify( swords );
    }
    catch ( exception&e ){
      all_well = false;
      exs += string(e.what()) + " ";
    }
    timers.tagTimer.stop();
    if ( !all_well ){
      throw runtime_error( exs );
    }
    for ( const auto& sword : swords ) {
#pragma omp parallel sections
      {
#pragma omp section
	{
	  if ( options.doMorph ){
	    timers.mbmaTimer.start();
	    if (options.debugFlag > 1){
	      LOG << "Calling mbma..." << endl;
	    }
	    try {
	      myMbma->Classify( sword );
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
	      LOG << "Calling mblem..." << endl;
	    }
	    try {
	      myMblem->Classify( sword );
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
#pragma omp parallel sections
    {
#pragma omp section
      {
	if ( options.doNER ){
	  timers.nerTimer.start();
	  if (options.debugFlag > 1) {
	    LOG << "Calling NER..." << endl;
	  }
	  try {
	    myNERTagger->Classify( swords );
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
	    myIOBTagger->Classify( swords );
	  }
	  catch ( exception&e ){
	    all_well = false;
	    exs += string(e.what()) + " ";
	  }
	  timers.iobTimer.stop();
	}
      }
#pragma omp section
      {
	if ( options.doMwu ){
	  if ( swords.size() > 0 ){
	    timers.mwuTimer.start();
	    myMwu->Classify( swords );
	    timers.mwuTimer.stop();
	  }
	}
	if ( options.doParse ){
	  if ( options.maxParserTokens != 0
	       && swords.size() > options.maxParserTokens ){
	    showParse = false;
	  }
	  else {
	    myParser->Parse( swords, timers );
	  }
	}
      }
    }
    if ( !all_well ){
      throw runtime_error( exs );
    }
  }
  return showParse;
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
	  LOG << "received data [" << result << "]" << endl;
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
	  LOG << "Received: [" << data << "]" << endl;
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
	//	LOG << "Done Processing... " << endl;
      }
      if (!conn.write( (output_stream.str()) ) || !(conn.write("READY\n"))  ){
	if (options.debugFlag > 5 ) {
	  LOG << "socket " << conn.getMessage() << endl;
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

vector<folia::Word*> FrogAPI::lookup( folia::Word *word,
				      const vector<folia::Entity*>& entities ) const {
  for ( const auto& ent : entities ){
    vector<folia::Word*> vec = ent->select<folia::Word>();
    if ( !vec.empty() ){
      if ( vec[0]->id() == word->id() ) {
	return vec;
      }
    }
  }
  vector<folia::Word*> vec;
  vec.push_back( word ); // single unit
  return vec;
}

folia::Dependency *FrogAPI::lookupDep( const folia::Word *word,
				       const vector<folia::Dependency*>&dependencies ) const{
  if (dependencies.size() == 0 ){
    return 0;
  }
  int dbFlag = 0;
  try {
    dbFlag = TiCC::stringTo<int>( configuration.lookUp( "debug", "parser" ) );
  }
  catch (exception & e) {
    dbFlag = 0;
  }
  if ( dbFlag > 1){
    LOG << endl << "Dependency-lookup "<< word << " in " << dependencies << endl;
  }
  for ( const auto& dep : dependencies ){
    if ( dbFlag > 1) {
      LOG << "Dependency try: " << dep << endl;
    }
    try {
      vector<folia::DependencyDependent*> dv
	= dep->select<folia::DependencyDependent>();
      if ( !dv.empty() ){
	vector<folia::Word*> wv = dv[0]->select<folia::Word>();
	for ( const auto& w : wv ){
	  if ( w == word ){
	    if ( dbFlag > 1 ){
	      LOG << "Dependency found word " << w << endl;
	    }
	    return dep;
	  }
	}
      }
    }
    catch ( exception& e ){
      if (dbFlag > 0){
	LOG << "get Dependency results failed: " << e.what() << endl;
      }
    }
  }
  return 0;
}

string FrogAPI::lookupNEREntity( const vector<folia::Word *>& mwus,
				 const vector<folia::Entity*>& entities ) const {
  string endresult;
  int dbFlag = 0;
  try{
    dbFlag = TiCC::stringTo<int>( configuration.lookUp( "debug", "NER" ) );
  }
  catch (exception & e) {
    dbFlag = 0;
  }
  for ( const auto& mwu : mwus ){
    if ( dbFlag > 1 ){
      LOG << endl << "NER: lookup "<< mwu << " in " << entities << endl;
    }
    string result;
    for ( const auto& entity :entities ){
      if ( dbFlag > 1 ){
	LOG << "NER: try: " << entity << endl;
      }
      try {
	vector<folia::Word*> wv = entity->select<folia::Word>();
	bool first = true;
	for ( const auto& word : wv ){
	  if ( word == mwu ){
	    if (dbFlag > 1){
	      LOG << "NER found word " << word << endl;
	    }
	    if ( first ){
	      result += "B-" + TiCC::uppercase(entity->cls());
	    }
	    else {
	      result += "I-" + TiCC::uppercase(entity->cls());
	    }
	    break;
	  }
	  else {
	    first = false;
	  }
	}
      }
      catch ( exception& e ){
	if  (dbFlag > 0){
	  LOG << "get NER results failed: "
			  << e.what() << endl;
	}
      }
    }
    if ( result.empty() ){
      endresult += "O";
    }
    else {
      endresult += result;
    }
    if ( &mwu != &mwus.back() ){
      endresult += "_";
    }
  }
  return endresult;
}


string FrogAPI::lookupIOBChunk( const vector<folia::Word *>& mwus,
				const vector<folia::Chunk*>& chunks ) const{
  string endresult;
  int dbFlag = 0;
  try {
    dbFlag = TiCC::stringTo<int>( configuration.lookUp( "debug", "IOB" ) );
  }
  catch (exception & e) {
    dbFlag = 0;
  }
  for ( const auto& mwu : mwus ){
    if ( dbFlag > 1 ){
      LOG << "IOB lookup "<< mwu << " in " << chunks << endl;
    }
    string result;
    for ( const auto& chunk : chunks ){
      if ( dbFlag > 1){
	LOG << "IOB try: " << chunk << endl;
      }
      try {
	vector<folia::Word*> wv = chunk->select<folia::Word>();
	bool first = true;
	for ( const auto& word : wv ){
	  if ( word == mwu ){
	    if (dbFlag > 1){
	      LOG << "IOB found word " << word << endl;
	    }
	    if ( first ) {
	      result += "B-" + chunk->cls();
	    }
	    else {
	      result += "I-" + chunk->cls();
	    }
	    break;
	  }
	  else {
	    first = false;
	  }
	}
      }
      catch ( exception& e ){
	if  (dbFlag > 0 ) {
	  LOG << "get Chunks results failed: "
			  << e.what() << endl;
	}
      }
    }
    if ( result.empty() ) {
      endresult += "O";
    }
    else {
      endresult += result;
    }
    if ( &mwu != &mwus.back() ){
      endresult += "_";
    }
  }
  return endresult;
}

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

vector<string> get_full_morph_analysis( folia::Word* w, bool flat ){
  return get_full_morph_analysis( w, "current", flat );
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

void FrogAPI::displayMWU( ostream& os,
			  size_t index,
			  const vector<folia::Word*>& mwu ) const {
  string wrd;
  string pos;
  string lemma;
  string morph;
  string comp;
  double conf = 1;
  for ( const auto& word : mwu ){
    try {
      wrd += word->str( options.outputclass );
      folia::PosAnnotation *postag
	= word->annotation<folia::PosAnnotation>( myCGNTagger->getTagset() );
      pos += postag->cls();
      if ( &word != &mwu.back() ){
	wrd += "_";
	pos += "_";
      }
      conf *= postag->confidence();
    }
    catch ( exception& e ){
      if ( options.debugFlag > 2 ){
	LOG << "get Postag results failed: "
			<< e.what() << endl;
      }
    }
    if ( options.doLemma ){
      try {
	lemma += word->lemma(myMblem->getTagset());
	if ( &word != &mwu.back() ){
	  lemma += "_";
	}
      }
      catch ( exception& e ){
	if ( options.debugFlag > 2 ){
	  LOG << "get Lemma results failed: "
			  << e.what() << endl;
	}
      }
    }
    if ( options.doMorph ){
      // also covers doDeepMorph
      try {
	vector<string> morphs = get_full_morph_analysis( word, options.outputclass );
	for ( const auto& m : morphs ){
	  morph += m;
	  if ( &m != &morphs.back() ){
	    morph += "/";
	  }
	}
	if ( &word != &mwu.back() ){
	  morph += "_";
	}
      }
      catch ( exception& e ){
	if  (options.debugFlag > 2){
	  LOG << "get Morph results failed: "
			  << e.what() << endl;
	}
      }
    }
    if ( options.doDeepMorph ){
      try {
	vector<string> cpv = get_compound_analysis( word );
	for ( const auto& cp : cpv ){
	  if ( cp.empty() ){
	    comp += "0";
	  }
	  else {
	    comp += cp+"-compound";
	  }
	  if ( &cp != &cpv.back() ){
	    morph += "/";
	  }
	}
	if ( &word != &mwu.back() ){
	  comp += "_";
	}
      }
      catch ( exception& e ){
	if  (options.debugFlag > 2){
	  LOG << "get Morph results failed: "
			  << e.what() << endl;
	}
      }
    }
  }
  os << index << "\t" << wrd << "\t" << lemma << "\t" << morph;
  if ( options.doDeepMorph ){
    if ( comp.empty() ){
      comp = "0";
    }
    os << "\t" << comp;
  }
  os << "\t" << pos << "\t" << std::fixed << conf;
}

void FrogAPI::showResults( ostream& os,
			   folia::Document& doc ) const {
  vector<folia::Sentence*> sentences = doc.sentences();
  for ( auto const& sentence : sentences ){
    vector<folia::Word*> words = sentence->words();
    vector<folia::Entity*> mwu_entities;
    if (myMwu){
      mwu_entities = sentence->select<folia::Entity>( myMwu->getTagset() );
    }
    vector<folia::Dependency*> dependencies;
    if (myParser){
      dependencies = sentence->select<folia::Dependency>( myParser->getTagset() );
    }
    vector<folia::Chunk*> iob_chunking;
    if ( myIOBTagger ){
      iob_chunking = sentence->select<folia::Chunk>( myIOBTagger->getTagset() );
    }
    vector<folia::Entity*> ner_entities;
    if (myNERTagger){
      ner_entities =  sentence->select<folia::Entity>( myNERTagger->getTagset() );
    }
    static set<folia::ElementType> excludeSet;
    vector<folia::Sentence*> parts = sentence->select<folia::Sentence>( excludeSet );
    if ( !options.doQuoteDetection ){
      assert( parts.size() == 0 );
    }
    for ( auto const& part : parts ){
      vector<folia::Entity*> ents;
      if (myMwu){
	ents = part->select<folia::Entity>( myMwu->getTagset() );
      }
      mwu_entities.insert( mwu_entities.end(), ents.begin(), ents.end() );
      vector<folia::Dependency*> deps = part->select<folia::Dependency>();
      dependencies.insert( dependencies.end(), deps.begin(), deps.end() );
      vector<folia::Chunk*> chunks = part->select<folia::Chunk>();
      iob_chunking.insert( iob_chunking.end(), chunks.begin(), chunks.end() );
      vector<folia::Entity*> ners ;
      if (myNERTagger) {
	ners = part->select<folia::Entity>( myNERTagger->getTagset() );
      }
      ner_entities.insert( ner_entities.end(), ners.begin(), ners.end() );
    }

    size_t index = 1;
    unordered_map<folia::FoliaElement*, int> enumeration;
    vector<vector<folia::Word*> > mwus;
    for ( size_t i=0; i < words.size(); ++i ){
      folia::Word *word = words[i];
      vector<folia::Word*> mwu = lookup( word, mwu_entities );
      for ( size_t j=0; j < mwu.size(); ++j ){
	enumeration[mwu[j]] = index;
      }
      mwus.push_back( mwu );
      i += mwu.size()-1;
      ++index;
    }
    index = 0;
    for ( const auto& mwu : mwus ){
      displayMWU( os, ++index, mwu );
      if ( options.doNER ){
	string s = lookupNEREntity( mwu, ner_entities );
	os << "\t" << s;
      }
      else {
	os << "\t\t";
      }
      if ( options.doIOB ){
	string s = lookupIOBChunk( mwu, iob_chunking);
	os << "\t" << s;
      }
      else {
	os << "\t\t";
      }
      if ( options.doParse ){
	folia::Dependency *dep = lookupDep( mwu[0], dependencies);
	if ( dep ){
	  vector<folia::Headspan*> w = dep->select<folia::Headspan>();
	  size_t num;
	  if ( w[0]->index(0)->isinstance( folia::PlaceHolder_t ) ){
	    string indexS = w[0]->index(0)->str();
	    folia::FoliaElement *pnt = w[0]->index(0)->doc()->index(indexS);
	    num = enumeration.find(pnt->index(0))->second;
	  }
	  else {
	    num = enumeration.find(w[0]->index(0))->second;
	  }
	  os << "\t" << num << "\t" << dep->cls();
	}
	else {
	  os << "\t"<< 0 << "\tROOT";
	}
      }
      else {
	os << "\t\t";
      }
      os << endl;
    }
    if ( words.size() ){
      os << endl;
    }
  }
}

string FrogAPI::Frogtostring( const string& s ){
  folia::Document *doc = tokenizer->tokenizestring( s );
  stringstream ss;
  FrogDoc( *doc, true );
  if ( options.doXMLout ){
    doc->save( ss, options.doKanon );
  }
  else {
    showResults( ss, *doc );
  }
  delete doc;
  return ss.str();
}

string FrogAPI::Frogtostringfromfile( const string& name ){
  stringstream ss;
  FrogFile( name, ss, "" );
  return ss.str();
}

void FrogAPI::FrogDoc( folia::Document& doc,
		       bool hidetimers ){
  timers.frogTimer.start();
  // first we make sure that the doc will accept our annotations, by
  // declaring them in the doc
  if (myCGNTagger){
    myCGNTagger->addDeclaration( doc );
  }
  if ( options.doLemma && myMblem ) {
    myMblem->addDeclaration( doc );
  }
  if ( options.doMorph && myMbma ) {
    myMbma->addDeclaration( doc );
  }
  if ( options.doIOB && myIOBTagger ){
    myIOBTagger->addDeclaration( doc );
  }
  if ( options.doNER && myNERTagger ){
    myNERTagger->addDeclaration( doc );
  }
  if ( options.doMwu && myMwu ){
    myMwu->addDeclaration( doc );
  }
  if ( options.doParse && myParser ){
    myParser->addDeclaration( doc );
  }
  if ( options.debugFlag > 5 ){
    LOG << "Testing document :" << doc << endl;
  }
  vector<folia::Sentence*> sentences;
  if ( options.doQuoteDetection ){
    sentences = doc.sentenceParts();
  }
  else {
    sentences = doc.sentences();
  }
  size_t numS = sentences.size();
  if ( numS > 0 ) { //process sentences
    LOG << TiCC::Timer::now() << " process " << numS << " sentences" << endl;
    for ( size_t i = 0; i < numS; ++i ) {
      //NOTE- full sentences are passed (which may span multiple lines) (MvG)
      string lan = sentences[i]->language();
      if ( !options.language.empty()
	   && options.language != "none"
	   && !lan.empty()
	   && lan != options.language ){
	if  (options.debugFlag >= 0){
	  LOG << "Not processing sentence " << i+1 << endl
			  << " different language: " << lan << endl
			  << " --language=" << options.language << endl;
	}
	continue;
      }
      //      LOG << sentences[i]->text() << endl;
      bool showParse = TestSentence( sentences[i], timers );
      if ( options.doParse && !showParse ){
	LOG << "WARNING!" << endl;
	LOG << "Sentence " << i+1
	    << " isn't parsed because it contains more tokens then set with the --max-parser-tokens="
	    << options.maxParserTokens << " option." << endl;
      }
      else {
	if  (options.debugFlag > 0){
	  LOG << TiCC::Timer::now() << " done with sentence[" << i+1 << "]" << endl;
	}
      }
    }
  }
  else {
    if  (options.debugFlag > 0){
      LOG << "No sentences found in document. " << endl;
    }
  }

  timers.frogTimer.stop();
  if ( !hidetimers ){
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
  return;
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
       LOG << "skipping sentence (different language: " << lan
	   << " --language=" << options.language << ")" << endl;
    }
    return false;
  }
  else {
    bool showParse = options.doParse;
    timers.frogTimer.start();
    if ( options.debugFlag > 5 ){
      LOG << "Frogging sentence:" << sent << endl;
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
	      LOG << "Calling mbma..." << endl;
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
	      LOG << "Calling mblem..." << endl;
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
	    LOG << "Calling NER..." << endl;
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
      frog_record rec;
      rec.word = w->str(options.inputclass);
      folia::KWargs atts = w->collectAttributes();
      if ( atts.find( "space" ) != atts.end() ){
	rec.no_space = true;
      }
      if ( atts.find( "class" ) != atts.end() ){
	rec.token_class = atts["class"];
      }
      else {
	rec.token_class = "WORD";
      }
      res.units.push_back( rec );
    }
    if  (options.debugFlag > 0){
      LOG << "before frog_sentence() 1" << endl;
    }
    frog_sentence( res );
    if ( !options.noStdOut ){
      showResults( os, res );
    }
    if  (options.debugFlag > 0){
      LOG << "before append_to_words()" << endl;
    }
    append_to_words( wv, res );
    if (options.debugFlag > 0){
      LOG << "after append_to_words()" << endl;
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
	LOG << "before frog_sentence() LOOP" << endl;
      }
      frog_sentence( res );
      if ( !options.noStdOut ){
	showResults( os, res );
      }
      if  (options.debugFlag > 0){
	LOG << "before append_to_sentence() A" << endl;
      }
      append_to_sentence( s, res );
      if  (options.debugFlag > 0){
	LOG << "after append_to_sentence() A" << endl;
      }
      timers.tokTimer.start();
      res = tokenizer->tokenize_stream( inputstream );
      timers.tokTimer.stop();
    }
  }
}

void FrogAPI::handle_one_paragraph( ostream& os, folia::Paragraph *p ){
  vector<folia::Word*> wv;
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
      LOG << "before append_to_sentence() B" << endl;
    }
    append_to_sentence( s, res );
    if  (options.debugFlag > 0){
      LOG << "after append_to_sentence() B" << endl;
    }
    timers.tokTimer.start();
    res = tokenizer->tokenize_stream( inputstream );
    timers.tokTimer.stop();
  }
}

void FrogAPI::run_folia_processor( const string& infilename,
				   ostream& output_stream,
				   const string& xmlOutFile ){
  folia::Processor proc( infilename );
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
  //      proc.set_debug( true );
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
  folia::FoliaElement *p = 0;
  while ( (p = proc.get_node( "s|p" ) ) ){
    if ( p->xmltag() == "s" ){
      if  (options.debugFlag > 0){
	LOG << "found sentence A " << p << endl;
      }
      handle_one_sentence( output_stream, dynamic_cast<folia::Sentence*>(p) );
      if  (options.debugFlag > 0){
	LOG << "after handle_one_sentence() A" << endl;
      }
    }
    else {
      vector<folia::Sentence*> sv = p->select<folia::Sentence>();
      if ( sv.empty() ){
	cerr << " No sentences!, take paragraph text" << endl;
	handle_one_paragraph( output_stream, dynamic_cast<folia::Paragraph*>(p) );
      }
      else {
	if  (options.debugFlag > 0){
	  LOG << "found paragraph " << p << "   with " << sv.size() << " sentences " << endl;
	}
	for ( const auto& s : sv ){
	  if  (options.debugFlag > 0){
	    LOG << "found sentence B " << s << endl;
	  }
	  handle_one_sentence( output_stream, s );
	  if  (options.debugFlag > 0){
	    LOG << "after handle_one_sentence() B" << endl;
	  }
	}
      }
    }
    if ( proc.next() ){
      if  (options.debugFlag > 0){
	LOG << "looping for more ..." << endl;
      }
    }
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
	  LOG << TiCC::Timer::now() << " done with sentence[" << i << "]" << endl;
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
    if ( true ){ //!hidetimers ){
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
