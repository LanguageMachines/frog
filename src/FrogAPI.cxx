/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2016
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
#include <unistd.h>
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


// individual module headers

#include "frog/FrogAPI.h" //will also include Frog.h (internals), FrogAPI.h is for public  interface

using namespace std;
using namespace folia;
using namespace TiCC;
using namespace Tagger;

string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/";
string configFileName = configDir + "frog.cfg";

string FrogAPI::defaultConfigDir(){ return configDir; }
string FrogAPI::defaultConfigFile(){ return configFileName; }

FrogOptions::FrogOptions() {
  doTok = doLemma = doMorph = doMwu = doIOB = doNER = doParse = true;
  doDeepMorph = false;
  doSentencePerLine = false;
  doQuoteDetection = false;
  doDirTest = false;
  doServer = false;
  doXMLin =  false;
  doXMLout =  false;
  doKanon =  false;
  interactive = false;

  maxParserTokens =  0; // 0 for unlimited
#ifdef HAVE_OPENMP
  numThreads = omp_get_max_threads();
#else
  numThreads = 1;
#endif
  uttmark = "<utt>";
  listenport = "void";
  docid = "untitled";
  debugFlag = 0;
}

FrogAPI::FrogAPI( FrogOptions &opt,
		  const Configuration &conf,
		  LogStream *log ):
  configuration(conf),
  options(opt),
  theErrLog(log),
  myMbma(0),
  myMblem(0),
  myMwu(0),
  myParser(0),
  myPoSTagger(0),
  myIOBTagger(0),
  myNERTagger(0),
  tokenizer(0)
{
  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  if ( options.doTok && !configuration.hasSection("tokenizer") ){
    *Log(theErrLog) << "Missing [[tokenizer]] section in config file." << endl;
    *Log(theErrLog) << "Disabled the tokenizer." << endl;
    options.doTok = false;
  }
  if ( options.doMorph && !configuration.hasSection("mbma") ){
    *Log(theErrLog) << "Missing [[mbma]] section in config file." << endl;
    *Log(theErrLog) << "Disabled the Morhological analyzer." << endl;
    options.doMorph = false;
  }
  if ( options.doIOB && !configuration.hasSection("IOB") ){
    *Log(theErrLog) << "Missing [[IOB]] section in config file." << endl;
    *Log(theErrLog) << "Disabled the IOB Chunker." << endl;
    options.doIOB = false;
  }
  if ( options.doNER && !configuration.hasSection("NER") ){
    *Log(theErrLog) << "Missing [[NER]] section in config file." << endl;
    *Log(theErrLog) << "Disabled the NER." << endl;
    options.doNER = false;
  }
  if ( options.doMwu && !configuration.hasSection("mwu") ){
    *Log(theErrLog) << "Missing [[mwu]] section in config file." << endl;
    *Log(theErrLog) << "Disabled the Multi Word Unit." << endl;
    *Log(theErrLog) << "Also disabled the parser." << endl;
    options.doMwu = false;
    options.doParse = false;
  }
  if ( options.doParse && !configuration.hasSection("parser") ){
    *Log(theErrLog) << "Missing [[parser]] section in config file." << endl;
    *Log(theErrLog) << "Disabled the parser." << endl;
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
      tokenizer->setTextClass( options.textclass );
      myPoSTagger = new CGNTagger(theErrLog);
      stat = myPoSTagger->init( configuration );
      if ( stat ){
	myPoSTagger->set_eos_mark( options.uttmark );
	if ( options.doIOB ){
	  myIOBTagger = new IOBTagger(theErrLog);
	  stat = myIOBTagger->init( configuration );
	  if ( stat ){
	    myIOBTagger->set_eos_mark( options.uttmark );
	  }
	}
	if ( stat && options.doNER ){
	  myNERTagger = new NERTagger(theErrLog);
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
      *Log(theErrLog) << "Frog initialization failed." << endl;
      exit(2);
    }
  }
  else {
#ifdef HAVE_OPENMP
    omp_set_num_threads( options.numThreads );
    int curt = omp_get_max_threads();
    if ( curt != options.numThreads ){
      *Log(theErrLog) << "attempt to set to " << options.numThreads
		      << " threads FAILED, running on " << curt
		      << " threads instead" << endl;
    }
    else if ( options.debugFlag ){
      *Log(theErrLog) << "running on " << curt
		      << " threads" << endl;
    }

#endif

    bool tokStat = true;
    bool lemStat = true;
    bool mwuStat = true;
    bool mbaStat = true;
    bool parStat = true;
    bool tagStat = true;
    bool iobStat = true;
    bool nerStat = true;

#pragma omp parallel sections
    {
#pragma omp section
      {
	tokenizer = new UctoTokenizer(theErrLog);
	tokStat = tokenizer->init( configuration );
	if ( tokStat ){
	  tokenizer->setPassThru( !options.doTok );
	  tokenizer->setDocID( options.docid );
	  tokenizer->setSentencePerLineInput( options.doSentencePerLine );
	  tokenizer->setQuoteDetection( options.doQuoteDetection );
	  tokenizer->setInputEncoding( options.encoding );
	  tokenizer->setInputXml( options.doXMLin );
	  tokenizer->setUttMarker( options.uttmark );
	  tokenizer->setTextClass( options.textclass );
	}
      }
#pragma omp section
      {
	if ( options.doLemma ){
	  myMblem = new Mblem(theErrLog);
	  lemStat = myMblem->init( configuration );
	}
      }
#pragma omp section
      {
	if ( options.doMorph ){
	  myMbma = new Mbma(theErrLog);
	  mbaStat = myMbma->init( configuration );
	  if ( options.doDeepMorph )
	    myMbma->setDeepMorph(true);
	}
      }
#pragma omp section
      {
	myPoSTagger = new CGNTagger(theErrLog);
	tagStat = myPoSTagger->init( configuration );
      }
#pragma omp section
      {
	if ( options.doIOB ){
	  myIOBTagger = new IOBTagger(theErrLog);
	  iobStat = myIOBTagger->init( configuration );
	}
      }
#pragma omp section
      {
	if ( options.doNER ){
	  myNERTagger = new NERTagger(theErrLog);
	  nerStat = myNERTagger->init( configuration );
	}
      }
#pragma omp section
      {
	if ( options.doMwu ){
	  myMwu = new Mwu(theErrLog);
	  mwuStat = myMwu->init( configuration );
	  if ( mwuStat && options.doParse ){
	    Timer initTimer;
	    initTimer.start();
	    myParser = new Parser(theErrLog);
	    parStat = myParser->init( configuration );
	    initTimer.stop();
	    *Log(theErrLog) << "init Parse took: " << initTimer << endl;
	  }
	}
      }
    }   // end omp parallel sections
    if ( ! ( tokStat && iobStat && nerStat && tagStat && lemStat
	     && mbaStat && mwuStat && parStat ) ){
      string out = "Initialization failed for: ";
      if ( !tokStat ){
	out += "[tokenizer] ";
      }
      if ( !tagStat ){
	out += "[tagger] ";
      }
      if ( !iobStat ){
	out += "[IOB] ";
      }
      if ( !nerStat ){
	out += "[NER] ";
      }
      if ( !lemStat ){
	out += "[lemmatizer] ";
      }
      if ( !mbaStat ){
	out += "[morphology] ";
      }
      if ( !mwuStat ){
	out += "[multiword unit] ";
      }
      if ( !parStat ){
	out += "[parser] ";
      }
      *Log(theErrLog) << out << endl;
      exit( EXIT_FAILURE );
    }
  }
  *Log(theErrLog) << "Initialization done." << endl;
}

FrogAPI::~FrogAPI() {
  delete myMbma;
  delete myMblem;
  delete myMwu;
  delete myPoSTagger;
  delete myIOBTagger;
  delete myNERTagger;
  delete myParser;
  delete tokenizer;
}

bool FrogAPI::TestSentence( Sentence* sent, TimerBlock& timers){
  vector<Word*> swords;
  if ( options.doQuoteDetection ){
    swords = sent->wordParts();
  }
  else {
    swords = sent->words();
  }
  bool showParse = options.doParse;
  bool all_well = true;
  string exs;
  if ( !swords.empty() ) {
#pragma omp parallel sections shared(all_well,exs)
    {
#pragma omp section
      {
	timers.tagTimer.start();
	try {
	  myPoSTagger->Classify( swords );
	}
	catch ( exception&e ){
	  all_well = false;
	  exs += string(e.what()) + " ";
	}
	timers.tagTimer.stop();
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
	if ( options.doNER ){
	  timers.nerTimer.start();
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
    } // parallel sections
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
	    if (options.debugFlag){
	      *Log(theErrLog) << "Calling mbma..." << endl;
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
	    if (options.debugFlag) {
	      *Log(theErrLog) << "Calling mblem..." << endl;
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
    } //for int i = 0 to num_words
    if ( !all_well ){
      throw runtime_error( exs );
    }

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
  return showParse;
}

void FrogAPI::FrogServer( Sockets::ServerSocket &conn ){
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
        if ( options.debugFlag ){
	  *Log(theErrLog) << "received data [" << result << "]" << endl;
	}
        Document doc;
        try {
	  doc.readFromString( result );
        }
	catch ( std::exception& e ){
	  *Log(theErrLog) << "FoLiaParsing failed:" << endl << e.what() << endl;
	  throw;
        }
        *Log(theErrLog) << "Processing... " << endl;
        tokenizer->tokenize( doc );
        FrogDoc( doc );
	if ( options.doXMLout ){
	  doc.save( outputstream, options.doKanon );
	}
	else {
	  showResults( outputstream, doc );
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
	    if ( line == "EOT" )
	      break;
	    data += line + "\n";
	  }
        }
        if ( options.debugFlag ){
	  *Log(theErrLog) << "Received: [" << data << "]" << endl;
	}
        *Log(theErrLog) << "Processing... " << endl;
        istringstream inputstream(data,istringstream::in);
        Document *doc = tokenizer->tokenize( inputstream );
        FrogDoc( *doc );
	if ( options.doXMLout ){
	  doc->save( outputstream, options.doKanon );
	}
	else {
	  showResults( outputstream, *doc );
	}
	delete doc;
      }
      if (!conn.write( (outputstream.str()) ) || !(conn.write("READY\n"))  ){
	if (options.debugFlag) {
	  *Log(theErrLog) << "socket " << conn.getMessage() << endl;
	}
	throw( runtime_error( "write to client failed" ) );
      }
    }
  }
  catch ( std::exception& e ) {
    if (options.debugFlag){
      *Log(theErrLog) << "connection lost: " << e.what() << endl;
    }
  }
  *Log(theErrLog) << "Connection closed.\n";
}

void FrogAPI::FrogStdin( bool prompt ) {
  if ( prompt ){
    cout << "frog>"; cout.flush();
  }
  string line;
  string data;
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
    Document *doc = tokenizer->tokenize( inputstream );
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
	eof = true;
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
	if ( data[data.size()-1] == '\n' ){
	  data = data.substr( 0, data.size()-1 );
	}
	cout << "Processing... '" << data << "'" << endl;
	istringstream inputstream(data,istringstream::in);
	Document *doc = tokenizer->tokenize( inputstream );
	FrogDoc( *doc, true );
	showResults( cout, *doc );
	delete doc;
      }
    }
    cout << "Done.\n";
  }
#endif
}

vector<Word*> FrogAPI::lookup( Word *word,
			       const vector<Entity*>& entities ) const {
  for ( const auto& ent : entities ){
    vector<Word*> vec = ent->select<Word>();
    if ( !vec.empty() ){
      if ( vec[0]->id() == word->id() ) {
	return vec;
      }
    }
  }
  vector<Word*> vec;
  vec.push_back( word ); // single unit
  return vec;
}

Dependency *FrogAPI::lookupDep( const Word *word,
				const vector<Dependency*>&dependencies ) const{
  if (dependencies.size() == 0 ){
    return 0;
  }
  int dbFlag = 0;
  try {
    dbFlag = stringTo<int>( configuration.lookUp( "debug", "parser" ) );
  }
  catch (exception & e) {
    dbFlag = 0;
  }
  if ( dbFlag ){
    using TiCC::operator<<;
    *Log( theErrLog ) << "\nDependency-lookup "<< word << " in " << dependencies << endl;
  }
  for ( const auto& dep : dependencies ){
    if ( dbFlag ){
      *Log( theErrLog ) << "Dependency try: " << dep << endl;
    }
    try {
      vector<DependencyDependent*> dv = dep->select<DependencyDependent>();
      if ( !dv.empty() ){
	vector<Word*> wv = dv[0]->select<Word>();
	for ( const auto& w : wv ){
	  if ( w == word ){
	    if ( dbFlag ){
	      *Log(theErrLog) << "\nDependency found word " << w << endl;
	    }
	    return dep;
	  }
	}
      }
    }
    catch ( exception& e ){
      if (dbFlag > 0){
	*Log(theErrLog) << "get Dependency results failed: "
			<< e.what() << endl;
      }
    }
  }
  return 0;
}

string FrogAPI::lookupNEREntity( const vector<Word *>& mwus,
				 const vector<Entity*>& entities ) const {
  string endresult;
  int dbFlag = 0;
  try{
    dbFlag = stringTo<int>( configuration.lookUp( "debug", "NER" ) );
  }
  catch (exception & e) {
    dbFlag = 0;
  }
  for ( const auto& mwu : mwus ){
    if ( dbFlag ){
      using TiCC::operator<<;
      *Log(theErrLog) << "\nNER: lookup "<< mwu << " in " << entities << endl;
    }
    string result;
    for ( const auto& entity :entities ){
      if ( dbFlag ){
	*Log(theErrLog) << "NER: try: " << entity << endl;
      }
      try {
	vector<Word*> wv = entity->select<Word>();
	bool first = true;
	for ( const auto& word : wv ){
	  if ( word == mwu ){
	    if (dbFlag){
	      *Log(theErrLog) << "NER found word " << word << endl;
	    }
	    if ( first ){
	      result += "B-" + uppercase(entity->cls());
	    }
	    else {
	      result += "I-" + uppercase(entity->cls());
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
	  *Log(theErrLog) << "get NER results failed: "
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


string FrogAPI::lookupIOBChunk( const vector<Word *>& mwus,
				const vector<Chunk*>& chunks ) const{
  string endresult;
  int dbFlag = 0;
  try {
   dbFlag = stringTo<int>( configuration.lookUp( "debug", "IOB" ) );
  }
  catch (exception & e) {
    dbFlag = 0;
  }
  for ( const auto& mwu : mwus ){
    if ( dbFlag ){
      using TiCC::operator<<;
      *Log(theErrLog) << "IOB lookup "<< mwu << " in " << chunks << endl;
    }
    string result;
    for ( const auto& chunk : chunks ){
      if ( dbFlag ){
	*Log(theErrLog) << "IOB try: " << chunk << endl;
      }
      try {
	vector<Word*> wv = chunk->select<Word>();
	bool first = true;
	for ( const auto& word : wv ){
	  if ( word == mwu ){
	    if (dbFlag){
	      *Log(theErrLog) << "IOB found word " << word << endl;
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
	if  (dbFlag > 0) {
	  *Log(theErrLog) << "get Chunks results failed: "
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
  vector<MorphologyLayer*> layers
    = word->annotations<MorphologyLayer>( Mbma::mbma_tagset );
  for ( const auto& layer : layers ){
    vector<Morpheme*> m =
      layer->select<Morpheme>( Mbma::mbma_tagset, false );
    if ( m.size() == 1 ) {
      // check for top layer compound
      PosAnnotation *postag
	= m[0]->annotation<PosAnnotation>( Mbma::clex_tagset );
      if ( postag ){
	//	cerr << "found a clex postag!" << endl;
	result.push_back( postag->feat( "compound" ) ); // might be empty
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
  vector<string> result;
  vector<MorphologyLayer*> layers
    = w->annotations<MorphologyLayer>( Mbma::mbma_tagset );
  for ( const auto& layer : layers ){
    vector<Morpheme*> m =
      layer->select<Morpheme>( Mbma::mbma_tagset, false );
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
      vector<Morpheme*> m = layer->select<Morpheme>( Mbma::mbma_tagset );
      for ( const auto& mor : m ){
	string txt = UnicodeToUTF8( mor->text() );
	morph += "[" + txt + "]";
      }
      result.push_back( morph );
    }
  }
  return result;
}

void FrogAPI::displayMWU( ostream& os,
			  size_t index,
			  const vector<Word*>& mwu ) const {
  string wrd;
  string pos;
  string lemma;
  string morph;
  string comp;
  double conf = 1;
  for ( const auto& word : mwu ){
    try {
      wrd += word->str();
      PosAnnotation *postag = word->annotation<PosAnnotation>( myPoSTagger->getTagset() );
      pos += postag->cls();
      if ( &word != &mwu.back() ){
	wrd += "_";
	pos += "_";
      }
      conf *= postag->confidence();
    }
    catch ( exception& e ){
      if  (options.debugFlag > 0){
	*Log(theErrLog) << "get Postag results failed: "
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
	if  (options.debugFlag > 0){
	  *Log(theErrLog) << "get Lemma results failed: "
			  << e.what() << endl;
	}
      }
    }
    if ( options.doMorph ){
      // also covers doDeepMorph
      try {
	vector<string> morphs = get_full_morph_analysis( word );
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
	if  (options.debugFlag > 0){
	  *Log(theErrLog) << "get Morph results failed: "
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
	if  (options.debugFlag > 0){
	  *Log(theErrLog) << "get Morph results failed: "
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
			       Document& doc ) const {
  vector<Sentence*> sentences = doc.sentences();
  for ( auto const& sentence : sentences ){
    vector<Word*> words = sentence->words();
    vector<Entity*> mwu_entities;
    if (myMwu){
      mwu_entities = sentence->select<Entity>( myMwu->getTagset() );
    }
    vector<Dependency*> dependencies;
    if (myParser){
      dependencies = sentence->select<Dependency>( myParser->getTagset() );
    }
    vector<Chunk*> iob_chunking;
    if ( myIOBTagger ){
      iob_chunking = sentence->select<Chunk>( myIOBTagger->getTagset() );
    }
    vector<Entity*> ner_entities;
    if (myNERTagger){
      ner_entities =  sentence->select<Entity>( myNERTagger->getTagset() );
    }
    static set<ElementType> excludeSet;
    vector<Sentence*> parts = sentence->select<Sentence>( excludeSet );
    if ( !options.doQuoteDetection ){
      assert( parts.size() == 0 );
    }
    for ( auto const& part : parts ){
      vector<Entity*> ents;
      if (myMwu){
	ents = part->select<Entity>( myMwu->getTagset() );
      }
      mwu_entities.insert( mwu_entities.end(), ents.begin(), ents.end() );
      vector<Dependency*> deps = part->select<Dependency>();
      dependencies.insert( dependencies.end(), deps.begin(), deps.end() );
      vector<Chunk*> chunks = part->select<Chunk>();
      iob_chunking.insert( iob_chunking.end(), chunks.begin(), chunks.end() );
      vector<Entity*> ners ;
      if (myNERTagger) {
	ners = part->select<Entity>( myNERTagger->getTagset() );
      }
      ner_entities.insert( ner_entities.end(), ners.begin(), ners.end() );
    }

    size_t index = 1;
    map<FoliaElement*, int> enumeration;
    vector<vector<Word*> > mwus;
    for ( size_t i=0; i < words.size(); ++i ){
      Word *word = words[i];
      vector<Word*> mwu = lookup( word, mwu_entities );
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
	string cls;
	string s = lookupNEREntity( mwu, ner_entities );
	os << "\t" << s;
      }
      else {
	os << "\t\t";
      }
      if ( options.doIOB ){
	string cls;
	string s = lookupIOBChunk( mwu, iob_chunking);
	os << "\t" << s;
      }
      else {
	os << "\t\t";
      }
      if ( options.doParse ){
	string cls;
	Dependency *dep = lookupDep( mwu[0], dependencies);
	if ( dep ){
	  vector<Headspan*> w = dep->select<Headspan>();
	  size_t num;
	  if ( w[0]->index(0)->isinstance( PlaceHolder_t ) ){
	    string indexS = w[0]->index(0)->str();
	    FoliaElement *pnt = w[0]->index(0)->doc()->index(indexS);
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
  Document *doc = tokenizer->tokenizestring( s );
  stringstream ss;
  FrogDoc( *doc, true );
  showResults( ss, *doc );
  delete doc;
  return ss.str();
}

string FrogAPI::Frogtostringfromfile( const string& name ){
  stringstream ss;
  FrogFile( name, ss, "" );
  return ss.str();
}

void FrogAPI::FrogDoc( Document& doc,
		       bool hidetimers ){
  TimerBlock timers;
  timers.frogTimer.start();
  // first we make sure that the doc will accept our annotations, by
  // declaring them in the doc
  if (myPoSTagger){
    myPoSTagger->addDeclaration( doc );
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
    *Log(theErrLog) << "Testing document :" << doc << endl;
  }

  vector<Sentence*> sentences;
  if ( options.doQuoteDetection ){
    sentences = doc.sentenceParts();
  }
  else {
    sentences = doc.sentences();
  }
  size_t numS = sentences.size();
  if ( numS > 0 ) { //process sentences
    if  (options.debugFlag > 0) {
      *Log(theErrLog) << "found " << numS
		      << " sentence(s) in document." << endl;
    }
    for ( size_t i = 0; i < numS; ++i ) {
      //NOTE- full sentences are passed (which may span multiple lines) (MvG)
      bool showParse = TestSentence( sentences[i], timers );
      if ( options.doParse && !showParse ){
	*Log(theErrLog) << "WARNING!" << endl;
	*Log(theErrLog) << "Sentence " << i+1
			<< " isn't parsed because it contains more tokens then set with the --max-parser-tokens="
			<< options.maxParserTokens << " option." << endl;
      }
    }
  }
  else {
    if  (options.debugFlag > 0){
      *Log(theErrLog) << "No sentences found in document. " << endl;
    }
  }

  timers.frogTimer.stop();
  if ( !hidetimers ){
    *Log(theErrLog) << "tokenisation took:  " << timers.tokTimer << endl;
    *Log(theErrLog) << "CGN tagging took:   " << timers.tagTimer << endl;
    if ( options.doIOB){
      *Log(theErrLog) << "IOB chunking took:  " << timers.iobTimer << endl;
    }
    if ( options.doNER){
      *Log(theErrLog) << "NER took:           " << timers.nerTimer << endl;
    }
    if ( options.doMorph ){
      *Log(theErrLog) << "MBMA took:          " << timers.mbmaTimer << endl;
    }
    if ( options.doLemma ){
      *Log(theErrLog) << "Mblem took:         " << timers.mblemTimer << endl;
    }
    if ( options.doMwu ){
      *Log(theErrLog) << "MWU resolving took: " << timers.mwuTimer << endl;
    }
    if ( options.doParse ){
      *Log(theErrLog) << "Parsing (prepare) took: " << timers.prepareTimer << endl;
      *Log(theErrLog) << "Parsing (pairs)   took: " << timers.pairsTimer << endl;
      *Log(theErrLog) << "Parsing (rels)    took: " << timers.relsTimer << endl;
      *Log(theErrLog) << "Parsing (dir)     took: " << timers.dirTimer << endl;
      *Log(theErrLog) << "Parsing (csi)     took: " << timers.csiTimer << endl;
      *Log(theErrLog) << "Parsing (total)   took: " << timers.parseTimer << endl;
    }
   *Log(theErrLog) << "Frogging in total took: " << timers.frogTimer << endl;
  }
  return;
}

void FrogAPI::FrogFile( const string& infilename,
			ostream &os,
			const string& xmlOutF ) {
  // stuff the whole input into one FoLiA document.
  // This is not a good idea on the long term, I think (agreed [proycon] )
  string xmlOutFile = xmlOutF;
  if ( options.doXMLin && !xmlOutFile.empty() ){
    if ( match_back( infilename, ".gz" ) ){
      if ( !match_back( xmlOutFile, ".gz" ) )
	xmlOutFile += ".gz";
    }
    else if ( match_back( infilename, ".bz2" ) ){
      if ( !match_back( xmlOutFile, ".bz2" ) )
	xmlOutFile += ".bz2";
    }
  }
  if ( options.doXMLin ){
    Document doc;
    try {
      doc.readFromFile( infilename );
    }
    catch ( exception &e ){
      *Log(theErrLog) << "retrieving FoLiA from '" << infilename << "' failed with exception:" << endl;
      cerr << e.what() << endl;
      return;
    }
    tokenizer->tokenize( doc );
    FrogDoc( doc, false );
    if ( !xmlOutFile.empty() ){
      doc.save( xmlOutFile, options.doKanon );
      *Log(theErrLog) << "resulting FoLiA doc saved in " << xmlOutFile << endl;
    }
    showResults( os, doc );
  }
  else {
    ifstream IN( infilename );
    Document *doc = tokenizer->tokenize( IN );
    FrogDoc( *doc, false );
    if ( !xmlOutFile.empty() ){
      doc->save( xmlOutFile, options.doKanon );
      *Log(theErrLog) << "resulting FoLiA doc saved in " << xmlOutFile << endl;
    }
    showResults( os, *doc );
    delete doc;
  }
}
