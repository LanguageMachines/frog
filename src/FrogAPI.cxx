/* ex: set tabstop=8 expandtab: */
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




// individual module headers

#include "frog/FrogAPI.h" //will also include Frog.h (internals), FrogAPI.h is for public  interface

using namespace std;
using namespace folia;
using namespace TiCC;


FrogAPI::FrogAPI(FrogOptions * options, Configuration * configuration, LogStream * theErrLog){

  this->theErrLog = new LogStream(theErrLog);
  this->options = options;
  this->configuration = configuration;
  myMbma = NULL;
  myMblem = NULL;
  myMwu = NULL;
  myParser = NULL;
  myCGNTagger = NULL;
  myIOBTagger = NULL;
  myNERTagger = NULL;
  tokenizer = NULL;

  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  if ( options->doTok && !configuration->hasSection("tokenizer") ){
    *Log(theErrLog) << "Missing [[tokenizer]] section in config file." << endl;
    exit(2);
  }
  if ( options->doIOB && !configuration->hasSection("IOB") ){
    *Log(theErrLog) << "Missing [[IOB]] section in config file." << endl;
    exit(2);
  }
  if ( options->doNER && !configuration->hasSection("NER") ){
    *Log(theErrLog) << "Missing [[NER]] section in config file." << endl;
    exit(2);
  }
  if ( options->doMwu ){
    if ( !configuration->hasSection("mwu") ){
      *Log(theErrLog) << "Missing [[mwu]] section in config file." << endl;
      exit(2);
    }
  }
  else if ( options->doParse ){
    *Log(theErrLog) << " Parser disabled, because MWU is deselected" << endl;
    options->doParse = false;
  }

  if ( options->doServer ){
    // we use fork(). omp (GCC version) doesn't do well when omp is used
    // before the fork!
    // see: http://bisqwit.iki.fi/story/howto/openmp/#OpenmpAndFork
    tokenizer = new UctoTokenizer(theErrLog);
    bool stat = tokenizer->init( *configuration, options->docid, !options->doTok );
    if ( stat ){
      tokenizer->setSentencePerLineInput( options->doSentencePerLine );
      tokenizer->setQuoteDetection( options->doQuoteDetection );
      tokenizer->setInputEncoding( options->encoding );
      tokenizer->setInputXml( options->doXMLin );
      tokenizer->setUttMarker( options->uttmark );
      tokenizer->setTextClass( options->textclass );
      myCGNTagger = new CGNTagger(theErrLog);
      stat = myCGNTagger->init( *configuration );
      if ( stat ){
	if ( options->doIOB ){
      myIOBTagger = new IOBTagger(theErrLog);
	  stat = myIOBTagger->init( *configuration );
	}
	if ( stat && options->doNER ){
      myNERTagger = new NERTagger(theErrLog);
	  stat = myNERTagger->init( *configuration );
	}
	if ( stat && options->doLemma ){
      myMblem = new Mblem(theErrLog);
	  stat = myMblem->init( *configuration );
	}
	if ( stat && options->doMorph ){
      myMbma = new Mbma(theErrLog);
	  stat = myMbma->init( *configuration );
	  if ( stat ) {
	    if ( options->doDaringMorph )
	      myMbma->setDaring(true);
	    if ( options->doMwu ){
          myMwu = new Mwu(theErrLog);
	      stat = myMwu->init( *configuration );
	      if ( stat && options->doParse ){
          myParser = new Parser(theErrLog);
		stat = myParser->init( *configuration );
	      }
	    }
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
	tokStat = tokenizer->init( *configuration, options->docid, !options->doTok );
	if ( tokStat ){
	  tokenizer->setSentencePerLineInput( options->doSentencePerLine );
	  tokenizer->setQuoteDetection( options->doQuoteDetection );
	  tokenizer->setInputEncoding( options->encoding );
	  tokenizer->setInputXml( options->doXMLin );
	  tokenizer->setUttMarker( options->uttmark );
	  tokenizer->setTextClass( options->textclass );
	}
      }
#pragma omp section
      {
	if ( options->doLemma ){
      myMblem = new Mblem(theErrLog);
	  lemStat = myMblem->init( *configuration );
	}
      }
#pragma omp section
      {
	if ( options->doMorph ){
      myMbma = new Mbma(theErrLog);
	  mbaStat = myMbma->init( *configuration );
	  if ( options->doDaringMorph )
	    myMbma->setDaring(true);
	}
      }
#pragma omp section
      {
    myCGNTagger = new CGNTagger(theErrLog);
	tagStat = myCGNTagger->init( *configuration );
      }
#pragma omp section
      {
	if ( options->doIOB ){
        myIOBTagger = new IOBTagger(theErrLog);
	  iobStat = myIOBTagger->init( *configuration );
	}
      }
#pragma omp section
      {
	if ( options->doNER ){
        myNERTagger = new NERTagger(theErrLog);
	  nerStat = myNERTagger->init( *configuration );
	}
      }
#pragma omp section
      {
	if ( options->doMwu ){
      myMwu = new Mwu(theErrLog);
	  mwuStat = myMwu->init( *configuration );
	  if ( mwuStat && options->doParse ){
	    Timer initTimer;
	    initTimer.start();
        myParser = new Parser(theErrLog);
	    parStat = myParser->init( *configuration );
	    initTimer.stop();
	    *Log(theErrLog) << "init Parse took: " << initTimer << endl;
	  }
	}
      }
    }   // end omp parallel sections
    if ( ! ( tokStat && iobStat && nerStat && tagStat && lemStat
	     && mbaStat && mwuStat && parStat ) ){
      *Log(theErrLog) << "Initialization failed for: ";
      if ( ! ( tokStat ) ){
	*Log(theErrLog) << "[tokenizer] ";
      }
      if ( ! ( tagStat ) ){
	*Log(theErrLog) << "[tagger] ";
      }
      if ( ! ( iobStat ) ){
	*Log(theErrLog) << "[IOB] ";
      }
      if ( ! ( nerStat ) ){
	*Log(theErrLog) << "[NER] ";
      }
      if ( ! ( lemStat ) ){
	*Log(theErrLog) << "[lemmatizer] ";
      }
      if ( ! ( mbaStat ) ){
	*Log(theErrLog) << "[morphology] ";
      }
      if ( ! ( mwuStat ) ){
	*Log(theErrLog) << "[multiword unit] ";
      }
      if ( ! ( parStat ) ){
	*Log(theErrLog) << "[parser] ";
      }
      *Log(theErrLog) << endl;
      exit(2);
    }
  }
  *Log(theErrLog) << "Initialization done." << endl;
}

FrogAPI::~FrogAPI() {
    if (myMbma != NULL) delete myMbma;
    if (myMblem != NULL) delete myMblem;
    if (myMwu != NULL) delete myMwu;
    if (myCGNTagger != NULL) delete myCGNTagger;
    if (myIOBTagger != NULL) delete myIOBTagger;
    if (myNERTagger != NULL) delete myNERTagger;
    if (myParser != NULL) delete myParser;
    if (tokenizer != NULL) delete tokenizer;
}

bool FrogAPI::TestSentence( Sentence* sent, TimerBlock& timers){
  vector<Word*> swords;
  if ( options->doQuoteDetection )
    swords = sent->wordParts();
  else
    swords = sent->words();
  bool showParse = options->doParse;
  if ( !swords.empty() ) {
#pragma omp parallel sections
    {
#pragma omp section
      {
	timers.tagTimer.start();
	myCGNTagger->Classify( swords );
	timers.tagTimer.stop();
      }
#pragma omp section
      {
	if ( options->doIOB ){
	  timers.iobTimer.start();
	  myIOBTagger->Classify( swords );
	  timers.iobTimer.stop();
	}
      }
#pragma omp section
      {
	if ( options->doNER ){
	  timers.nerTimer.start();
	  myNERTagger->Classify( swords );
	  timers.nerTimer.stop();
	}
      }
    } // parallel sections
    for ( size_t i = 0; i < swords.size(); ++i ) {
#pragma omp parallel sections
      {
#pragma omp section
	{
	  if ( options->doMorph ){
	    timers.mbmaTimer.start();
	    if (options->debugFlag)
	      *Log(theErrLog) << "Calling mbma..." << endl;
	    myMbma->Classify( swords[i] );
	    timers.mbmaTimer.stop();
	  }
	}
#pragma omp section
	{
	  if ( options->doLemma ){
	    timers.mblemTimer.start();
	    if (options->debugFlag)
	      *Log(theErrLog) << "Calling mblem..." << endl;
	    myMblem->Classify( swords[i] );
	    timers.mblemTimer.stop();
	  }
	}
      } // omp parallel sections
    } //for int i = 0 to num_words

    if ( options->doMwu ){
      if ( swords.size() > 0 ){
	timers.mwuTimer.start();
	myMwu->Classify( swords );
	timers.mwuTimer.stop();
      }
    }
    if ( options->doParse ){
      if ( options->maxParserTokens != 0 && swords.size() > options->maxParserTokens ){
	showParse = false;
      }
      else {
        myParser->Parse( swords, myMwu->getTagset(), options->tmpDirName, timers );
      }
    }
  }
  return showParse;
}


vector<Word*> FrogAPI::lookup( Word *word, const vector<Entity*>& entities ){
  vector<Word*> vec;
  for ( size_t p=0; p < entities.size(); ++p ){
    vec = entities[p]->select<Word>();
    if ( !vec.empty() ){
      if ( vec[0]->id() == word->id() ) {
	return vec;
      }
    }
  }
  vec.clear();
  vec.push_back( word ); // single unit
  return vec;
}

Dependency * FrogAPI::lookupDep( const Word *word, const vector<Dependency*>&dependencies ){
  if (dependencies.size() == 0 ){
    return 0;
  }
  int dbFlag = stringTo<int>( configuration->lookUp( "debug", "parser" ) );
  if ( dbFlag ){
    using TiCC::operator<<;
    *Log( theErrLog ) << "\nDependency-lookup "<< word << " in " << dependencies << endl;
  }
  for ( size_t i=0; i < dependencies.size(); ++i ){
    if ( dbFlag ){
      *Log( theErrLog ) << "Dependency try: " << dependencies[i] << endl;
    }
    try {
      vector<DependencyDependent*> dv = dependencies[i]->select<DependencyDependent>();
      if ( !dv.empty() ){
	vector<Word*> v = dv[0]->select<Word>();
	for ( size_t j=0; j < v.size(); ++j ){
	  if ( v[j] == word ){
	    if ( dbFlag ){
	      *Log(theErrLog) << "\nDependency found word " << v[j] << endl;
	    }
	    return dependencies[i];
	  }
	}
      }
    }
    catch ( exception& e ){
      if (dbFlag > 0)
	*Log(theErrLog) << "get Dependency results failed: "
			<< e.what() << endl;
    }
  }
  return 0;
}

string FrogAPI::lookupNEREntity( const vector<Word *>& mwu, const vector<Entity*>& entities){
  string endresult;
  int dbFlag = stringTo<int>( configuration->lookUp( "debug", "NER" ) );
  for ( size_t j=0; j < mwu.size(); ++j ){
    if ( dbFlag ){
      using TiCC::operator<<;
      *Log(theErrLog) << "\nNER: lookup "<< mwu[j] << " in " << entities << endl;
    }
    string result;
    for ( size_t i=0; i < entities.size(); ++i ){
      if ( dbFlag ){
	*Log(theErrLog) << "NER try: " << entities[i] << endl;
      }
      try {
	vector<Word*> v = entities[i]->select<Word>();
	bool first = true;
	for ( size_t k=0; k < v.size(); ++k ){
	  if ( v[k] == mwu[j] ){
	    if (dbFlag){
	      *Log(theErrLog) << "NER found word " << v[k] << endl;
	    }
	    if ( first )
	      result += "B-" + uppercase(entities[i]->cls());
	    else
	      result += "I-" + uppercase(entities[i]->cls());
	    break;
	  }
	  else
	    first = false;
	}
      }
      catch ( exception& e ){
	if  (dbFlag > 0)
	  *Log(theErrLog) << "get NER results failed: "
			  << e.what() << endl;
      }
    }
    if ( result.empty() )
      endresult += "O";
    else
      endresult += result;
    if ( j < mwu.size()-1 )
      endresult += "_";
  }
  return endresult;
}


string FrogAPI::lookupIOBChunk( const vector<Word *>& mwu, const vector<Chunk*>& chunks ){
  string endresult;
  int dbFlag = stringTo<int>( configuration->lookUp( "debug", "IOB" ) );
  for ( size_t j=0; j < mwu.size(); ++j ){
    if ( dbFlag ){
      using TiCC::operator<<;
      *Log(theErrLog) << "IOB lookup "<< mwu[j] << " in " << chunks << endl;
    }
    string result;
    for ( size_t i=0; i < chunks.size(); ++i ){
      if ( dbFlag ){
	*Log(theErrLog) << "IOB try: " << chunks[i] << endl;
      }
      try {
	vector<Word*> v = chunks[i]->select<Word>();
	bool first = true;
	for ( size_t k=0; k < v.size(); ++k ){
	  if ( v[k] == mwu[j] ){
	    if (dbFlag){
	      *Log(theErrLog) << "IOB found word " << v[k] << endl;
	    }
	    if ( first )
	      result += "B-" + chunks[i]->cls();
	    else
	      result += "I-" + chunks[i]->cls();
	    break;
	  }
	  else
	    first = false;
	}
      }
      catch ( exception& e ){
	if  (dbFlag > 0)
	  *Log(theErrLog) << "get Chunks results failed: "
			  << e.what() << endl;
      }
    }
    if ( result.empty() )
      endresult += "O";
    else
      endresult += result;
    if ( j < mwu.size()-1 )
      endresult += "_";
  }
  return endresult;
}

void FrogAPI::displayMWU( ostream& os, size_t index, const vector<Word*> mwu){
  string wrd;
  string pos;
  string lemma;
  string morph;
  double conf = 1;
  for ( size_t p=0; p < mwu.size(); ++p ){
    Word *word = mwu[p];
    try {
      wrd += word->str();
      PosAnnotation *postag = word->annotation<PosAnnotation>( );
      pos += postag->cls();
      if ( p < mwu.size() -1 ){
	wrd += "_";
	pos += "_";
      }
      conf *= postag->confidence();
    }
    catch ( exception& e ){
      if  (options->debugFlag > 0)
	*Log(theErrLog) << "get Postag results failed: "
			<< e.what() << endl;
    }
    if ( options->doLemma ){
      try {
	lemma += word->lemma();
	if ( p < mwu.size() -1 ){
	  lemma += "_";
	}
      }
      catch ( exception& e ){
	if  (options->debugFlag > 0)
	  *Log(theErrLog) << "get Lemma results failed: "
			  << e.what() << endl;
      }
    }
    if ( options->doDaringMorph ){
      try {
	vector<MorphologyLayer*> ml = word->annotations<MorphologyLayer>();
	for ( size_t q=0; q < ml.size(); ++q ){
	  vector<Morpheme*> m = ml[q]->select<Morpheme>( false );
	  assert( m.size() == 1 ); // top complex layer
	  string desc = m[0]->description();
	  if ( q < ml.size()-1 )
	    morph += "/";
	}
	if ( p < mwu.size() -1 ){
	  morph += "_";
	}
      }
      catch ( exception& e ){
	if  (options->debugFlag > 0)
	  *Log(theErrLog) << "get Morph results failed: "
			  << e.what() << endl;
      }
    }
    else if ( options->doMorph ){
      try {
	vector<MorphologyLayer*> ml = word->annotations<MorphologyLayer>();
	for ( size_t q=0; q < ml.size(); ++q ){
	  vector<Morpheme*> m = ml[q]->select<Morpheme>();
	  for ( size_t t=0; t < m.size(); ++t ){
	    string txt = UnicodeToUTF8( m[t]->text() );
	    morph += "[" + txt + "]";
	  }
	  if ( q < ml.size()-1 )
	    morph += "/";
	}
	if ( p < mwu.size() -1 ){
	  morph += "_";
	}
      }
      catch ( exception& e ){
	if  (options->debugFlag > 0)
	  *Log(theErrLog) << "get Morph results failed: "
			  << e.what() << endl;
      }
    }
  }
  os << index << "\t" << wrd << "\t" << lemma << "\t" << morph << "\t" << pos << "\t" << std::fixed << conf;
}

ostream & FrogAPI::showResults( ostream& os, const Sentence* sentence, bool showParse){
  vector<Word*> words = sentence->words();
  vector<Entity*> mwu_entities;
  if (myMwu) mwu_entities = sentence->select<Entity>( myMwu->getTagset() );
  vector<Dependency*> dependencies;
  if (myParser) dependencies = sentence->select<Dependency>();
  vector<Chunk*> iob_chunking = sentence->select<Chunk>();
  vector<Entity*> ner_entities;
  if (myNERTagger)ner_entities =  sentence->select<Entity>( myNERTagger->getTagset() );
  static set<ElementType> excludeSet;
  vector<Sentence*> parts = sentence->select<Sentence>( excludeSet );
  if ( !options->doQuoteDetection )
    assert( parts.size() == 0 );
  for ( size_t i=0; i < parts.size(); ++i ){
    vector<Entity*> ents;
    if (myMwu) ents = parts[i]->select<Entity>( myMwu->getTagset() );
    mwu_entities.insert( mwu_entities.end(), ents.begin(), ents.end() );
    vector<Dependency*> deps = parts[i]->select<Dependency>();
    dependencies.insert( dependencies.end(), deps.begin(), deps.end() );
    vector<Chunk*> chunks = parts[i]->select<Chunk>();
    iob_chunking.insert( iob_chunking.end(), chunks.begin(), chunks.end() );
    vector<Entity*> ners ;
    if (myNERTagger) ners = parts[i]->select<Entity>( myNERTagger->getTagset() );
    ner_entities.insert( ner_entities.end(), ners.begin(), ners.end() );
  }

  size_t index = 1;
  map<FoliaElement*, int> enumeration;
  vector<vector<Word*> > mwus;
  for( size_t i=0; i < words.size(); ++i ){
    Word *word = words[i];
    vector<Word*> mwu = lookup( word, mwu_entities );
    for ( size_t j=0; j < mwu.size(); ++j ){
      enumeration[mwu[j]] = index;
    }
    mwus.push_back( mwu );
    i += mwu.size()-1;
    ++index;
  }
  for( size_t i=0; i < mwus.size(); ++i ){
    displayMWU( os, i+1, mwus[i] );
    if ( options->doNER ){
      string cls;
      string s = lookupNEREntity( mwus[i], ner_entities );
      os << "\t" << s;
    }
    else {
      os << "\t\t";
    }
    if ( options->doIOB ){
      string cls;
      string s = lookupIOBChunk( mwus[i], iob_chunking);
      os << "\t" << s;
    }
    else {
      os << "\t\t";
    }
    if ( showParse ){
      string cls;
      Dependency *dep = lookupDep( mwus[i][0], dependencies);
      if ( dep ){
	vector<Headwords*> w = dep->select<Headwords>();
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
    ++index;
  }
  if ( words.size() )
    os << endl;
  return os;
}

string FrogAPI::Test( folia::Document& doc, bool hidetimers) {
    //converts results to string (useful for external bindings)
    stringstream ss;
    Test(doc, ss, hidetimers);
    return ss.str();
}

void FrogAPI::Test( Document& doc, ostream& outStream,  bool hidetimers, const string& xmlOutFile) {
  TimerBlock timers;
  timers.frogTimer.start();
  // first we make sure that the doc will accept our annotations, by
  // declaring them in the doc
  if (myCGNTagger) 
      myCGNTagger->addDeclaration( doc );
  if (( options->doLemma ) && (myMblem))
    myMblem->addDeclaration( doc );
  if (( options->doMorph ) && (myMbma))
    myMbma->addDeclaration( doc );
  if ((options->doIOB) && (myIOBTagger))
    myIOBTagger->addDeclaration( doc );
  if ((options->doNER) && (myNERTagger))
    myNERTagger->addDeclaration( doc );
  if ((options->doMwu) && (myMwu))
    myMwu->addDeclaration( doc );
  if ((options->doParse) && (myParser))
    myParser->addDeclaration( doc );

  if ( options->debugFlag > 5 )
    *Log(theErrLog) << "Testing document :" << doc << endl;

  vector<Sentence*> topsentences = doc.sentences();
  vector<Sentence*> sentences;
  if ( options->doQuoteDetection )
    sentences = doc.sentenceParts();
  else
    sentences = topsentences;
  size_t numS = sentences.size();
  if ( numS > 0 ) { //process sentences
    if  (options->debugFlag > 0)
      *Log(theErrLog) << "found " << numS << " sentence(s) in document." << endl;
    for ( size_t i = 0; i < numS; i++) {
      /* ******* Begin process sentence  ********** */
      //NOTE- full sentences are passed (which may span multiple lines) (MvG)
      bool showParse = TestSentence( sentences[i], timers );
      if ( options->doParse && !showParse ){
	*Log(theErrLog) << "WARNING!" << endl;
	*Log(theErrLog) << "Sentence " << i+1 << " isn't parsed because it contains more tokens then set with the --max-parser-tokens=" << options->maxParserTokens << " option." << endl;
      }
    }
    for ( size_t i = 0; i < topsentences.size(); ++i ) {
      if ( !(options->doServer && options->doXMLout) )
        showResults( outStream, topsentences[i], options->doParse);
    }
  }
  else {
    if  (options->debugFlag > 0)
      *Log(theErrLog) << "No sentences found in document. " << endl;
  }
  if ( options->doServer && options->doXMLout )
    outStream << doc << endl;
  if ( !xmlOutFile.empty() ){
    doc.save( xmlOutFile, options->doKanon );
    *Log(theErrLog) << "resulting FoLiA doc saved in " << xmlOutFile << endl;
  }

  timers.frogTimer.stop();
  if ( !hidetimers ){
    *Log(theErrLog) << "tokenisation took:  " << timers.tokTimer << endl;
    *Log(theErrLog) << "CGN tagging took:   " << timers.tagTimer << endl;
    if ( options->doIOB)
      *Log(theErrLog) << "IOB chunking took:  " << timers.iobTimer << endl;
    if ( options->doNER)
      *Log(theErrLog) << "NER took:           " << timers.nerTimer << endl;
    if ( options->doMorph )
      *Log(theErrLog) << "MBA took:           " << timers.mbmaTimer << endl;
    if ( options->doLemma )
      *Log(theErrLog) << "Mblem took:         " << timers.mblemTimer << endl;
    if ( options->doMwu )
      *Log(theErrLog) << "MWU resolving took: " << timers.mwuTimer << endl;
    if ( options->doParse ){
      *Log(theErrLog) << "Parsing (prepare) took: " << timers.prepareTimer << endl;
      *Log(theErrLog) << "Parsing (pairs)   took: " << timers.pairsTimer << endl;
      *Log(theErrLog) << "Parsing (rels)    took: " << timers.relsTimer << endl;
      *Log(theErrLog) << "Parsing (dir)     took: " << timers.dirTimer << endl;
      *Log(theErrLog) << "Parsing (csi)     took: " << timers.csiTimer << endl;
      *Log(theErrLog) << "Parsing (total)   took: " << timers.parseTimer << endl;
    }
  }
  *Log(theErrLog) << "Frogging in total took: " << timers.frogTimer << endl;
  return;
}

void FrogAPI::Test( const string& infilename, ostream &os,  const string& xmlOutF ) {
  // stuff the whole input into one FoLiA document.
  // This is not a good idea on the long term, I think (agreed [proycon] )

  string xmlOutFile = xmlOutF;
  if ( options->doXMLin && !xmlOutFile.empty() ){
    if ( match_back( infilename, ".gz" ) ){
      if ( !match_back( xmlOutFile, ".gz" ) )
	xmlOutFile += ".gz";
    }
    else if ( match_back( infilename, ".bz2" ) ){
      if ( !match_back( xmlOutFile, ".bz2" ) )
	xmlOutFile += ".bz2";
    }
  }
  if ( options->doXMLin ){
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
    Test( doc, os,  false, xmlOutFile );
  }
  else {
    ifstream IN( infilename.c_str() );
    Document doc = tokenizer->tokenize( IN );
    Test( doc, os,  false, xmlOutFile );
  }
}
