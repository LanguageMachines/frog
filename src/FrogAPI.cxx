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

folia::Document* FrogAPI::create_folia( const frog_data& fd,
					const string& id ) const {
  folia::Document *result = new folia::Document( "id='" + id + "'" );
  if ( options.language != "none" ){
    result->set_metadata( "language", options.language );
  }
  result->addStyle( "text/xsl", "folia.xsl" );
  if ( !options.doTok ){
    result->declare( folia::AnnotationType::TOKEN, "passthru", "annotator='ucto', annotatortype='auto', datetime='now()'" );
  }
  else {
    result->declare( folia::AnnotationType::TOKEN,
		    configuration.lookUp( "rulesFile", "tokenizer" ),
		    "annotator='ucto', annotatortype='auto', datetime='now()'");
  }
  myCGNTagger->addDeclaration( *result );
  if ( options.doLemma ){
    myMblem->addDeclaration( *result );
  }
  if ( options.doMorph ){
    myMbma->addDeclaration( *result );
  }
  if ( options.doIOB ){
    myIOBTagger->addDeclaration( *result );
  }
  if ( options.doNER ){
    myNERTagger->addDeclaration( *result );
  }
  if ( options.doMwu ){
    myMwu->addDeclaration( *result );
  }
  if ( options.doParse ){
    myParser->addDeclaration( *result );
  }
  folia::KWargs args;
  args["id"] = id + ".text";
  folia::Text *text = new folia::Text( args );
  result->append( text );
  args.clear();
  args["generate_id"] = text->id();
  folia::Sentence *s = new folia::Sentence( args, result );
  text->append( s );
  vector<folia::Word*> wv; // used for easy word lookup
  for ( const auto& word : fd.units ){
    folia::KWargs args;
    args["generate_id"] = s->id();
    args["class"] = word.token_class;
    if ( word.no_space ){
      args["space"] = "no";
    }
    folia::Word *w = new folia::Word( args, result );
    w->settext( word.word );
    s->append( w );
    wv.push_back( w );
    args.clear();
    args["set"]   = myCGNTagger->getTagset();
    args["class"] = word.tag;
    args["confidence"]= TiCC::toString(word.tag_confidence);
    folia::FoliaElement *postag = w->addPosAnnotation( args );
    vector<string> hv = TiCC::split_at_first_of( word.tag, "()" );
    string head = hv[0];
    args["class"] = head;
    folia::Feature *feat = new folia::HeadFeature( args );
    postag->append( feat );
    if ( head == "SPEC" ){
      postag->confidence(1.0);
    }
    vector<string> feats;
    if ( hv.size() > 1 ){
      feats = TiCC::split_at( hv[1], "," );
    }
    for ( const auto& f : feats ){
      folia::KWargs args;
      args["set"] =  myCGNTagger->getTagset();
      args["subset"] = myCGNTagger->getSubSet( f, head, word.tag );
      args["class"]  = f;
      folia::Feature *feat = new folia::Feature( args, result );
      postag->append( feat );
    }
    if ( options.doLemma ){
      folia::KWargs args;
      args["set"] = myMblem->getTagset();
      for ( const auto& lemma : word.lemmas ){
	args["class"] = lemma;
	w->addLemmaAnnotation( args );
      }
    }
    if ( options.doMorph ){
      folia::KWargs args;
      args["set"] = myMbma->mbma_tagset;
      folia::MorphologyLayer *ml = w->addMorphologyLayer( args );
      if ( !options.doDeepMorph ){
	for ( const auto& mor : word.morphs ) {
	  for ( const auto& mt : mor ) {
	    folia::Morpheme *m = new folia::Morpheme( args, result );
	    m->settext( mt );
	    ml->append( m );
	  }
	}
      }
      else {
	LOG << "deep morpheme XML output not implemented!" << endl;
      }
    }
  }
  if ( options.doNER ){
    folia::EntitiesLayer *el = 0;
    folia::Entity *ner = 0;
    size_t i = 0;
    for ( const auto& word : fd.units ){
      if ( word.ner_tag[0] == 'B' ){
	if ( el == 0 ){
	  // create a layer, we need it
	  folia::KWargs args;
	  args["set"] = myNERTagger->getTagset();
	  args["generate_id"] = s->id();
	  el = new folia::EntitiesLayer( args, result );
	  s->append(el);
	}
	// a new entity starts here
	if ( ner != 0 ){
	  el->append( ner );
	}
	// now make new entity
	folia::KWargs args;
	args["set"] = myNERTagger->getTagset();
	args["generate_id"] = el->id();
	args["class"] = word.ner_tag.substr(2);
	args["confidence"] = TiCC::toString(word.ner_confidence);
	ner = new folia::Entity( args, result );
	ner->append( wv[i] );
      }
      else if ( word.ner_tag[0] == 'I' ){
	// continue in an entity
	assert( ner != 0 );
	ner->append( wv[i] );
      }
      else if ( word.ner_tag[0] == '0' ){
	if ( ner != 0 ){
	  el->append( ner );
	  ner = 0;
	}
      }
      ++i;
    }
    if ( ner != 0 ){
      // some leftovers
      el->append( ner );
    }
  }
  if ( options.doIOB ){
    folia::ChunkingLayer *el = 0;
    folia::Chunk *iob = 0;
    size_t i = 0;
    for ( const auto& word : fd.units ){
      if ( word.iob_tag[0] == 'B' ){
	if ( el == 0 ){
	  // create a layer, we need it
	  folia::KWargs args;
	  args["set"] = myIOBTagger->getTagset();
	  args["generate_id"] = s->id();
	  el = new folia::ChunkingLayer( args, result );
	  s->append(el);
	}
	// a new entity starts here
	if ( iob != 0 ){
	  // add this iob to the layer
	  el->append( iob );
	}
	// now make new entity
	folia::KWargs args;
	args["set"] = myIOBTagger->getTagset();
	args["generate_id"] = el->id();
	args["class"] = word.iob_tag.substr(2);
	args["confidence"] = TiCC::toString(word.iob_confidence);
	iob = new folia::Chunk( args, result );
	iob->append( wv[i] );
      }
      else if ( word.iob_tag[0] == 'I' ){
	// continue in an entity
	assert( iob != 0 );
	iob->append( wv[i] );
      }
      else if ( word.iob_tag[0] == '0' ){
	if ( iob != 0 ){
	  el->append( iob );
	  iob = 0;
	}
      }
      ++i;
    }
    if ( iob != 0 ){
      // some leftovers
      el->append( iob );
    }
  }
  if ( options.doMwu && !fd.mwus.empty() ){
    folia::KWargs args;
    args["generate_id"] = s->id();
    args["set"] = myMwu->getTagset();
    folia::EntitiesLayer *el = new folia::EntitiesLayer( args, result );
    s->append( el );
    for ( const auto& mwu : fd.mwus ){
      args["generate_id"] = el->id();
      folia::Entity *e = new folia::Entity( args, result );
      el->append( e );
      for ( size_t pos = mwu.first; pos <= mwu.second; ++pos ){
	e->append( wv[pos] );
      }
    }
  }
  if ( options.doParse ){
    folia::KWargs args;
    args["generate_id"] = s->id();
    args["set"] = myParser->getTagset();
    folia::DependenciesLayer *el = new folia::DependenciesLayer( args, result );
    s->append( el );
    for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
      if ( fd.mw_units[pos].parts.size() > 1 ){
	// a true MWU
	string cls = fd.mw_units[pos].parse_role;
	if ( cls != "ROOT" ){
	  args["generate_id"] = el->id();
	  args["class"] = cls;
	  folia::Dependency *e = new folia::Dependency( args, result );
	  el->append( e );
	  folia::Headspan *dh = new folia::Headspan();
	  size_t head_index = fd.mw_units[pos].parse_index-1;
	  for ( auto const& i : fd.mw_units[head_index].parts ){
	    dh->append( wv[i] );
	  }
	  e->append( dh );
	  folia::DependencyDependent *dd = new folia::DependencyDependent();
	  for ( auto const& i : fd.mw_units[pos].parts ){
	    dd->append( wv[i] );
	  }
	  e->append( dd );
	}
      }
      else {
	// just 1 word
	string cls = fd.mw_units[pos].parse_role;
	if ( cls != "ROOT" ){
	  args["generate_id"] = el->id();
	  args["class"] = cls;
	  folia::Dependency *e = new folia::Dependency( args, result );
	  el->append( e );
	  folia::Headspan *dh = new folia::Headspan();
	  size_t head_index = fd.mw_units[pos].parse_index-1;
	  for ( auto const& i : fd.mw_units[head_index].parts ){
	    dh->append( wv[i] );
	  }
	  e->append( dh );
	  folia::DependencyDependent *dd = new folia::DependencyDependent();
	  for ( auto const& i : fd.mw_units[pos].parts ){
	    dd->append( wv[i] );
	  }
	  e->append( dd );
	}
      }
    }
  }
  return result;
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
  try {
    while ( conn.isValid() ) {
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
        if ( options.debugFlag > 5 ){
	  LOG << "received data [" << result << "]" << endl;
	}
	folia::Document doc;
        try {
	  doc.readFromString( result );
        }
	catch ( std::exception& e ){
	  LOG << "FoLiaParsing failed:" << endl << e.what() << endl;
	  throw;
        }
        LOG << "Processing XML... " << endl;
	timers.reset();
	timers.tokTimer.start();
	tokenizer->tokenize( doc );
	timers.tokTimer.stop();
        FrogDoc( doc );
	if ( options.doXMLout ){
	  doc.save( outputstream, options.doKanon );
	}
	else {
	  showResults( outputstream, doc );
	}
	//        LOG << "Done Processing XML... " << endl;
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
	timers.reset();
	timers.tokTimer.start();
	folia::Document *doc = tokenizer->tokenize( inputstream );
	timers.tokTimer.stop();
        FrogDoc( *doc );
	if ( options.doXMLout ){
	  doc->save( outputstream, options.doKanon );
	}
	else {
	  showResults( outputstream, *doc );
	}
	delete doc;
	//	LOG << "Done Processing... " << endl;
      }
      if (!conn.write( (outputstream.str()) ) || !(conn.write("READY\n"))  ){
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
    folia::Document *doc = tokenizer->tokenize( inputstream );
    FrogDoc( *doc, true );
    showResults( cout, *doc );
    delete doc;
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
	folia::Document *doc = tokenizer->tokenize( inputstream );
	FrogDoc( *doc, true );
	showResults( cout, *doc );
	delete doc;
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

ostream& FrogAPI::showResults( ostream& os,
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
  return os;
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

bool FrogAPI::frog_sentence( frog_data& sent ){
  bool showParse = options.doParse;
  //  timers.frogTimer.start();
  if ( options.debugFlag > 5 ){
    LOG << "Frogging sentence:" << sent << endl;
  }
  bool all_well = true;
  string exs;
  // timers.tagTimer.start();
  try {
    myCGNTagger->Classify( sent );
  }
  catch ( exception&e ){
    all_well = false;
    exs += string(e.what()) + " ";
  }
  //  timers.tagTimer.stop();
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
  // timers.frogTimer.stop();
  cout << "NEW Frog result:" << endl;
  cout << sent << endl;
  return showParse;
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
    timers.tokTimer.start();
    folia::Document doc;
    try {
      doc.readFromFile( infilename );
    }
    catch ( exception &e ){
      LOG << "retrieving FoLiA from '" << infilename << "' failed with exception:" << endl;
      LOG << e.what() << endl;
      throw ( runtime_error( "read failed" ) );
    }
    tokenizer->tokenize( doc );
    timers.tokTimer.stop();
    FrogDoc( doc );
    if ( !options.noStdOut ){
      showResults( os, doc );
    }
    if ( !xmlOutFile.empty() ){
      doc.save( xmlOutFile, options.doKanon );
      LOG << "resulting FoLiA doc saved in " << xmlOutFile << endl;
    }
  }
  else {
    ifstream TEST( infilename );
    int i = 0;
    frog_data res = tokenizer->tokenize_stream( TEST );
    while ( res.size() > 0 ){
      ++i;
      bool showParse = frog_sentence( res );
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
	  string file_name = xmlOutFile;
	  string::size_type pos = file_name.rfind( ".xml" );
	  file_name = file_name.substr( 0, pos ) + "-" + TiCC::toString(i)
	    + file_name.substr( pos );
	  LOG << "file_name=" << file_name << endl;
	  folia::Document *doc = create_folia( res, file_name );
	  doc->save( file_name );
	  LOG << "stored sentence " << i << " in file: " << file_name << endl;
	}
      }
      res = tokenizer->tokenize_stream( TEST );
    }
    ifstream IN( infilename );
    timers.reset();
    timers.tokTimer.start();
    if ( options.docid == "untitled" ){
      string id = filter_non_NC( TiCC::basename(infilename) );
      tokenizer->setDocID( id );
    }
    folia::Document *doc = tokenizer->tokenize( IN );
    timers.tokTimer.stop();
    FrogDoc( *doc );
    if ( !options.noStdOut ){
      showResults( os, *doc );
    }
    if ( !xmlOutFile.empty() ){
      doc->save( xmlOutFile, options.doKanon );
      LOG << "resulting FoLiA doc saved in " << xmlOutFile << endl;
    }
    delete doc;
  }
}
