/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2011
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

#include "timbl/TimblAPI.h"
#include "mbt/MbtAPI.h"
#include "timblserver/TimblServerAPI.h"

// individual module headers

#include "config.h"
#include "frog/Frog.h"
#include "frog/Configuration.h"
#include "ucto/tokenize.h"
#include "frog/mbma_mod.h"
#include "frog/mblem_mod.h"
#include "frog/mwu_chunker_mod.h"
#include "frog/FrogData.h"
#include "frog/Parser.h"
#include "libfolia/document.h"
#include "libfolia/folia.h"

using namespace std;
using namespace folia;

Tokenizer::TokenizerClass tokenizer;

LogStream my_default_log( cerr, "frog-", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals

string TestFileName;
string testDirName;
string tmpDirName;
string outputFileName;
string outputDirName;
set<string> fileNames;
string ProgName;
int tpDebug = 0; //0 for none, more for more output
string sep = " "; // "&= " for cgi 
bool doTok = true;
bool doMwu = true;
bool doParse = true;
bool doDirTest = false;
bool doServer = false;
bool doSentencePerLine = false;
string listenport = "void";
string encoding;
bool keepIntermediateFiles = false;

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
static string configFileName = string(SYSCONF_PATH) + '/' + PACKAGE + '/' + "frog.cfg";

void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -e <encoding>          specify encoding of the input (default UTF-8)\n"
       << "\t -t <testfile>          Run frog on this file\n"
       << "\t -S <port>              Run as server instead of reading from testfile)\n"
       << "\t --testdir=<directory>  All files in this dir will be tested)\n"
       << "\t -n                     Assume input file to hold one sentence per line\n"
       << "\t============= MODULE SELECTION ==========================================\n"
       << "\t --skip=[mpt]  Skip Tokenizer (t), Multi-Word Units (m) or Parser (p) \n"
       << "\t============= CONFIGURATION OPTIONS =====================================\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t============= OUTPUT OPTIONS ============================================\n"
       << "\t --tmpdir=<directory> (location to store intermediate files. Default /tmp )\n"
       << "\t -o <outputfile>	      Output to file, instead of default stdout\n"
       << "\t --outputdir=<outputfile> Output to directory, instead of default stdout\n"
       << "\t --keep-parser-files=[yes|no] keep intermediate parser files, (last sentence only)\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version . Show version info.\n"
       << "\t -d <debug level> (for more verbosity)\n"
       << "\t -Q               Enable quote detection in tokeniser\n";

}

//**** stuff to process commandline options *********************************

static MbtAPI *tagger;

static Mbma myMbma;
static Mblem myMblem;
static Mwu myMwu;
static Parser myParser;

bool parse_args( TimblOpts& Opts ) {
  string value;
  bool mood;
  if ( Opts.Find('V', value, mood ) ||
       Opts.Find("version", value, mood ) ){
    // we alreade did that
    exit( EXIT_SUCCESS );
  }
  if ( Opts.Find ('h', value, mood)) {
    usage();
    exit( EXIT_SUCCESS );
  };
  // is a config file specified?
  if ( Opts.Find( 'c',  value, mood ) ) {
    configFileName = value;
    Opts.Delete( 'c' );
  };

  if ( configuration.fill( configFileName ) ){
    cerr << "config read from: " << configFileName << endl;
    //    configuration.dump( cout );
  }
  else {
    cerr << "failed te read configuration from! '" << configFileName << "'" << endl;
    return false;
  }

  // debug opts
  if ( Opts.Find ('d', value, mood)) {
    tpDebug = atoi(value.c_str());
    Opts.Delete('d');
  };
  if ( Opts.Find ('n', value, mood)) {
    doSentencePerLine = true;
  };
  if ( Opts.Find( "skip", value, mood)) {
    string skip = value;
    if ( skip.find_first_of("tT") != string::npos )
      doTok = false;
    if ( skip.find_first_of("mM") != string::npos )
      doMwu = false;
    if ( skip.find_first_of("pP") != string::npos )
      doParse = false;
    Opts.Delete("skip");
  };

  if ( Opts.Find( "e", value, mood)) {
    encoding = value;
  }
  
  if ( Opts.Find( "keep-parser-files", value, mood ) ){
    if ( value.empty() ||
	 value == "true" || value == "TRUE" || value =="yes" || value == "YES" )
      keepIntermediateFiles = true;
  }
  tmpDirName = configuration.lookUp( "tmpdir", "global" );
  if ( Opts.Find ( "tmpdir", value, mood)) {
    tmpDirName = value;
    Opts.Delete("tmpdir");
  }
  if ( tmpDirName.empty() ){
    tmpDirName = "/tmp/";
  }
  else {
    tmpDirName += "/";
  }
  if ( !tmpDirName.empty() ){
    if ( !existsDir( tmpDirName ) ){
      *Log(theErrLog) << "temporary dir " << tmpDirName << " not readable" << endl;
      return false;
    }
    *Log(theErrLog) << "checking tmpdir: " << tmpDirName << " OK" << endl;
  }
  if ( Opts.Find ( "testdir", value, mood)) {
#ifdef HAVE_DIRENT_H
    doDirTest = true;
    testDirName = value;
    if ( !testDirName.empty() ){
      if ( !existsDir( testDirName ) ){
	*Log(theErrLog) << "input dir " << testDirName << " not readable" << endl;
	return false;
      }
      getFileNames( testDirName, fileNames );
      if ( fileNames.empty() ){
	*Log(theErrLog) << "error: couln't find any files in directory: " 
	     << testDirName << endl;
	return false;
      }
    }
    else {
      *Log(theErrLog) << "empty testdir name!" << endl;
      return false;
    }
#else
      *Log(theErrLog) << "--testdir option not supported!" << endl;
#endif
    Opts.Delete("testdir");
  }
  else if ( Opts.Find( 't', value, mood )) {
    TestFileName = value;
    ifstream is( value.c_str() );
    if ( !is ){
      *Log(theErrLog) << "input stream " << value << " is not readable" << endl;
      return false;
    }
    Opts.Delete('t');
  };
  if ( Opts.Find( "outputdir", value, mood)) {
    outputDirName = value;
    if ( !outputDirName.empty() ){
      if ( !existsDir( outputDirName ) ){
	*Log(theErrLog) << "output dir " << outputDirName << " not readable" << endl;
	return false;
      }
    }
    Opts.Delete( "outputdir");
  }
  else if ( Opts.Find ('o', value, mood)) {
    outputFileName = value;
    Opts.Delete('o');
  };
  if ( Opts.Find ('S', value, mood)) {
    doServer = true;
    listenport= value;
  }
  if ( !outputDirName.empty() && testDirName.empty() ){
    *Log(theErrLog) << "useless -outputdir option" << endl;
    return false;
  }
  if ( !outputFileName.empty() && !testDirName.empty() ){
    *Log(theErrLog) << "useless -o option" << endl;
    return false;
  }

  string tagset = configuration.lookUp( "settings", "tagger" );
  if ( tagset.empty() ){
    *Log(theErrLog) << "Unable to find settings for Tagger" << endl;
    return false;
  }

  string rulesName = configuration.lookUp( "rulesFile", "tokenizer" );
  if ( rulesName.empty() ){
    *Log(theErrLog) << "no rulesFile found in configuration" << endl;
    return false;
  }
  else {
    tokenizer.setErrorLog( theErrLog );
    if ( !tokenizer.init( rulesName ) )
      return false;
  }
  string debug = configuration.lookUp( "debug", "tokenizer" );
  if ( debug.empty() )
    tokenizer.setDebug( tpDebug );
  else
    tokenizer.setDebug( Timbl::stringTo<int>(debug) );

  if ( Opts.Find ('Q', value, mood)) {
      tokenizer.setQuoteDetection(true);
  };

  if ( !doServer && TestFileName.empty() && fileNames.empty() ){
    *Log(theErrLog) << "no frogging without input!" << endl;
    return false;
  }
  
  if ( doServer ){
    // we use fork(). omp (GCC version) doesn't do well when omp is used
    // before the fork!
    // see: http://bisqwit.iki.fi/story/howto/openmp/#OpenmpAndFork
    string init = "-s " + configuration.configDir() + tagset;
    if ( Tagger::Version() == "3.2.6" )
      init += " -vc";
    else
      init += " -vcf";
    tagger = new MbtAPI( init, *theErrLog );
    bool stat = myMblem.init( configuration );
    if ( stat ){
      stat = myMbma.init( configuration );
      if ( stat ) {
	if ( doMwu ){
	  stat = myMwu.init( configuration );
	  if ( stat && doParse )
	    stat = myParser.init( configuration );
	}
	else {
	  if ( doParse )
	    *Log(theErrLog) << " Parser disabled, because MWU is deselected" << endl;
	  doParse = false;;
	}
      }
    }
    if ( !stat ){
      *Log(theErrLog) << "Initialization failed." << endl;
      return false;
    }
  }
  else {
    bool lemStat = true;
    bool mwuStat = true;
    bool mbaStat = true;
    bool parStat = true;
#pragma omp parallel sections
    {
#pragma omp section
      lemStat = myMblem.init( configuration );
#pragma omp section
      mbaStat = myMbma.init( configuration );
#pragma omp section 
      {
	string init = "-s " + configuration.configDir() + tagset;
	if ( Tagger::Version() == "3.2.6" )
	  init += " -vc";
	else
	  init += " -vcf";
	tagger = new MbtAPI( init, *theErrLog );
      }
#pragma omp section
      {
	if ( doMwu ){
	  mwuStat = myMwu.init( configuration );
	  if ( mwuStat && doParse ){
	    Common::Timer initTimer;
	    initTimer.start();
	    parStat = myParser.init( configuration );
	    initTimer.stop();
	    *Log(theErrLog) << "init Parse took: " << initTimer << endl;
	  }
	}
	else {
	  if ( doParse )
	    *Log(theErrLog) << " Parser disabled, because MWU is deselected" << endl;
	  doParse = false;
	}
      }
    }   // end omp parallel sections
    if ( ! ( tagger->isInit() && lemStat && mbaStat && mwuStat && parStat ) ){
      *Log(theErrLog) << "Initialization failed: ";
      if ( ! ( tagger->isInit() ) ){
	*Log(theErrLog) << "[tagger] ";
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
      return false;
    }
  }
  *Log(theErrLog) << "Initialization done." << endl;
  return true;
}

bool splitOneWT( const string& inp, string& word, string& tag, string& confidence ){
  bool isKnown = true;
  string in = inp;
  //     split word and tag, and store num of slashes
  if (tpDebug)
    cout << "split Classify starting with " << in << endl;
  string::size_type pos = in.rfind("/");
  if ( pos == string::npos ) {
    *Log(theErrLog) << "no word/tag/confidence triple in this line: " << in << endl;
    exit( EXIT_FAILURE );
  }
  else {
    confidence = in.substr( pos+1 );
    in.erase( pos );
  }
  pos = in.rfind("//");
  if ( pos != string::npos ) {
    // double slash: lets's hope is is an unknown word
    if ( pos == 0 ){
      // but this is definitely something like //LET() 
      word = "/";
      tag = in.substr(pos+2);
    }
    else {
      word = in.substr( 0, pos );
      tag = in.substr( pos+2 );
    }
    isKnown = false;
  } 
  else {
    pos = in.rfind("/");
    if ( pos != string::npos ) {
      word = in.substr( 0, pos );
      tag = in.substr( pos+1 );
    }
    else {
      *Log(theErrLog) << "no word/tag/confidence triple in this line: " << in << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( tpDebug){
    if ( isKnown )
      cout << "known";
    else
      cout << "unknown";
    cout << " word: " << word << "\ttag: " << tag << "\tconfidence: " << confidence << endl;
  }
  return isKnown;
}

int splitWT( const string& tagged, const vector<string>& words,
	     vector<string>& tags, vector<bool>& known, vector<double>& conf ){
  vector<string> tagwords;
  tags.clear();
  known.clear();
  conf.clear();
  size_t num_words = Timbl::split_at( tagged, tagwords, sep );
  num_words--; // the last "word" is <utt> which gets added by the tagger
  for( size_t i = 0; i < num_words; ++i ) {
    string word, tag, confs;
    bool isKnown = splitOneWT( tagwords[i], word, tag, confs );
    if ( word != words[i] ){
      *Log(theErrLog) << "tagger out of sync: word[" << i << "] ≠ tagword["
		      << i << "] " << word << " vs. " 
		      << words[i] << endl;
      exit( EXIT_FAILURE );
    }
    double confidence;
    if ( !stringTo<double>( confs, confidence ) ){
      *Log(theErrLog) << "tagger confused. Expected a double, got '" << confs << "'" << endl;
      exit( EXIT_FAILURE );
    }
    tags.push_back( tag );
    known.push_back( isKnown );
    conf.push_back( confidence );
  }
  return num_words;
}

UnicodeString decap( const UnicodeString &W ) {
  // decapitalize the whole word 
  UnicodeString w = W;
  if (tpDebug)
    cout << "Decapping " << w;
  for ( int i = 0; i < w.length(); ++i )
    w.replace( i, 1, u_tolower( w[i] ) );
  if (tpDebug)
    cout << " to : " << w << endl;
  return w;
}

vector<FrogData *> TestSentence( AbstractElement* sent,
				 const string& tmpDir,
				 TimerBlock& timers ){
  vector<AbstractElement*> swords = sent->words();
  vector<FrogData *> solutions;
  if ( !swords.empty() ) {
    if (tpDebug) {
      // don't mangle debug output, so run 1 thread then
      omp_set_num_threads( 1 );
    }
    vector<string> words;
    string sentence; // the tagger needs the whole sentence
    for ( size_t w = 0; w < swords.size(); ++w ) {
      sentence += swords[w]->str();
      words.push_back( swords[w]->str() );
      if ( w < swords.size()-1 )
	sentence += " ";
    }
    if (tpDebug > 0 ) *Log(theErrLog) << "[DEBUG] SENTENCE: " << sentence << endl;
    
    if (tpDebug) 
      cout << "in: " << sentence << endl;
    timers.tagTimer.start();
    string tagged = tagger->Tag(sentence);
    timers.tagTimer.stop();
    if (tpDebug) {
      cout << "sentence: " << sentence
	   <<"\ntagged: "<< tagged
	   << endl;
    }
    vector<string> tags;
    vector<double> confidences;
    vector<bool> known;
    int num_words = splitWT( tagged, words, tags, known, confidences );
    if (tpDebug) {
      cout << "#tagged_words: " << num_words << endl;
      for( int i = 0; i < num_words; i++) 
	cout   << "\ttagged word[" << i <<"]: " << words[i] << (known[i]?"/":"//")
	       << tags[i] << " <" << confidences[i] << ">" << endl;
    }
    myMwu.reset();
    for ( int i = 0; i < num_words; ++i ) {
      KWargs args = getArgs( "set='mbt-pos', cls='" + escape( tags[i] )
			     + "', annotator='MBT', confidence='" 
			     + toString(confidences[i]) + "'" );
      swords[i]->addPosAnnotation( args );
      //process each word and dump every ana for now
      string mbmaLemma;
      string mblemLemma;
#pragma omp parallel sections
      {
#pragma omp section
	{
	  timers.mbmaTimer.start();
	  if (tpDebug) cout << "Calling mbma..." << endl;
	  mbmaLemma = myMbma.Classify( swords[i], words[i], tags[i] );
	  timers.mbmaTimer.stop();
	}
#pragma omp section
	{
	  timers.mblemTimer.start();
	  if (tpDebug) cout << "Calling mblem..." << endl;
	  mblemLemma = myMblem.Classify( swords[i], words[i], tags[i] );
	  timers.mblemTimer.stop();
	}
      }
      if (tpDebug) {
	cout   << "tagged word[" << i <<"]: " << words[i] << (known[i]?"/":"//")
	       << tags[i] << endl;
	cout << "analysis: " << mblemLemma << " " << mbmaLemma << endl;
      }
      myMwu.add( swords[i], words[i], tags[i], confidences[i], mblemLemma, mbmaLemma );  
    } //for int i = 0 to num_words

    if ( doMwu ){
      if ( num_words > 0 ){
	if (tpDebug)
	  cout << "starting mwu Chunking ... \n";
	timers.mwuTimer.start();
	myMwu.Classify( sent );
	timers.mwuTimer.stop();
      }
      if (tpDebug) {
	cout << "\n\nfinished mwu chunking!\n";
	cout << myMwu << endl;
      }
    }
    FrogData *pd = new FrogData( myMwu );
    if ( doParse ){  
      myParser.Parse( pd, sent, tmpDir, timers );
    }
    solutions.push_back( pd );
  }
  return solutions;
}

vector<AbstractElement *> lookup( AbstractElement *word, 
				  const vector<AbstractElement*>& entities ){
  vector<AbstractElement*> vec;
  for ( size_t p=0; p < entities.size(); ++p ){
    vec = entities[p]->select(Word_t);
    if ( !vec.empty() ){
      if ( vec[0]->id() == word->id() ) {
	// using folia::operator<<;
	// cerr << "found " << vec << endl;
	return vec;
      }
    }
  }
  vec.clear();
  return vec;
}

void displayWord( ostream& os, size_t index, const AbstractElement *word ){
  string pos;
  string lemma;
  string morph;
  double conf;
  try { 
    AbstractElement *postag = word->annotation(Pos_t);
    pos = postag->cls();
    conf = postag->confidence();
  }
  catch (... ){
  }
  try { 
    lemma = word->lemma();
  }
  catch (... ){
  }
  try { 
    vector<AbstractElement*> ml = word->annotations(Morphology_t);
    for ( size_t p=0; p < ml.size(); ++p ){
      vector<AbstractElement*> m = ml[p]->annotations(Morpheme_t);
      for ( size_t q=0; q < m.size(); ++q ){
	morph += "[" + UnicodeToUTF8( m[q]->text() ) + "]";
      }
      if ( p < ml.size()-1 )
	morph += "/";
    }
  }
  catch (... ){
  }
  os << index << "\t" << word->str() << "\t" << lemma << "\t" << morph << "\t" << pos << "\t" << std::fixed << conf << "\t\t" << endl;
}  

void displayMWU( ostream& os, size_t index, 
		 const vector<AbstractElement *> mwu ){
  string wrd;
  string pos;
  string lemma;
  string morph;
  double conf = 1;
  for ( size_t p=0; p < mwu.size(); ++p ){
    AbstractElement *word = mwu[p];
    try { 
      wrd += word->str();
      AbstractElement *postag = word->annotation(Pos_t);
      pos += postag->cls();
      if ( p < mwu.size() -1 ){
	wrd += "_";
	pos += "_";
      }
      conf *= postag->confidence();
    }
    catch (... ){
    }
    try { 
      lemma += word->lemma();
      if ( p < mwu.size() -1 ){
	lemma += "_";
      }
    }
    catch (... ){
    }
    try { 
      vector<AbstractElement*> m = word->annotations(Morpheme_t);
      for ( size_t t=0; t < m.size(); ++t ){
	morph += "[" + UnicodeToUTF8( m[t]->text() ) + "]";
      }
      if ( p < mwu.size() -1 ){
	morph += "_";
      }
    }
    catch (... ){
    }
  }
  os << index << "\t" << wrd << "\t" << lemma << "\t" << morph << "\t" << pos << "\t" << std::fixed << conf <<"\t\t" << endl;
}  

ostream &showResults( ostream& os, const AbstractElement* sentence ){
  vector<AbstractElement *> words = sentence->words();
  vector<AbstractElement *> entities = sentence->select( Entity_t );
  size_t index = 1;
  for( size_t i=0; i < words.size(); ++i ){
    AbstractElement *word = words[i];
    vector<AbstractElement *> mwu = lookup( word, entities );
    if ( !mwu.empty() ){
      displayMWU( os, index, mwu );
      i += mwu.size()-1;
    }
    else {
      displayWord( os, index, word );
    }
    ++index;
  }
  if ( words.size() )
    os << endl;
  return os;
}



void Test( istream& IN,
	   ostream& outStream,
	   TimerBlock& timers,
	   Common::Timer &frogTimer,
	   const string& tmpDir ) {

  string eosmarker = "";
  tokenizer.setEosMarker( eosmarker );
  tokenizer.setVerbose( false );
  tokenizer.setSentenceDetection( true ); //detection of sentences
  tokenizer.setParagraphDetection( false ); //detection of paragraphs
  tokenizer.setSentencePerLineInput( doSentencePerLine );
  if ( !encoding.empty() )
    tokenizer.setInputEncoding( encoding );
  
  tokenizer.setXMLOutput( true, "frog" );
  string line;  
  Document doc = tokenizer.tokenize( IN ); 
  doc.declare( AnnotationType::POS, "mbt-pos");
  doc.declare( AnnotationType::LEMMA, "mbt-lemma");

  // Tokenize the whole input into one FoLiA document.
  // This is nog a good idea on the long term, I think

  vector<AbstractElement*> sentences = doc.sentences();
  size_t numS = sentences.size();
  if ( numS > 0 ) { //process sentences 
    if  (tpDebug > 0) *Log(theErrLog) << "[tokenize] " << numS << " sentence(s) in buffer, processing..." << endl;
    for ( size_t i = 0; i < numS; i++) {
      /* ******* Begin process sentence  ********** */
      vector<FrogData*> solutions = TestSentence( sentences[i],  tmpDir, timers ); 
      //NOTE- full sentences are passed (which may span multiple lines) (MvG)         
      const size_t solution_size = solutions.size();
      for ( size_t j = 0; j < solution_size; ++j ) {
	showResults( outStream, *solutions[j] ); 
	delete solutions[j];
      }
      //      showResults( outStream, sentences[i] ); 
    }
  } else {
    if  (tpDebug > 0) *Log(theErrLog) << "[tokenize] No sentences yet, reading on..." << endl;
  }
  
  frogTimer.stop();  
  
  *Log(theErrLog) << "tokenisation took:" << timers.tokTimer << endl;
  *Log(theErrLog) << "tagging took:     " << timers.tagTimer << endl;
  *Log(theErrLog) << "MBA took:         " << timers.mbmaTimer << endl;
  *Log(theErrLog) << "Mblem took:       " << timers.mblemTimer << endl;
  if ( doMwu )
    *Log(theErrLog) << "MWU resolving took: " << timers.mwuTimer << endl;
  if ( doParse ){
    *Log(theErrLog) << "Parsing (prepare) took: " << timers.prepareTimer << endl;
    *Log(theErrLog) << "Parsing (pairs)   took: " << timers.pairsTimer << endl;
    *Log(theErrLog) << "Parsing (rels)    took: " << timers.relsTimer << endl;
    *Log(theErrLog) << "Parsing (dir)     took: " << timers.dirTimer << endl;
    *Log(theErrLog) << "Parsing (csi)     took: " << timers.csiTimer << endl;
    *Log(theErrLog) << "Parsing (total)   took: " << timers.parseTimer << endl;
  }
  *Log(theErrLog) << "Frogging took:      " << frogTimer << endl;
  doc.save( "frog-folia.xml" );
  *Log(theErrLog) << "debug:: resulting FoLiA doc saved in frog-folia.xml" << endl;
  return;
}

void TestFile( const string& infilename, const string& outFileName) {
  // init's are done
  TimerBlock timers;
  Common::Timer frogTimer;
  frogTimer.start();

  string TestFileName = ""; //untokenised!

  ofstream outStream;
  if ( !outFileName.empty() ){
    if ( outStream.open( outFileName.c_str() ), outStream.bad() ){
      *Log(theErrLog) << "unable to open outputfile: " << outFileName << endl;
      exit( EXIT_FAILURE );
    }
  }
  TestFileName = infilename; //untokenised!

  ifstream IN( TestFileName.c_str() );

  if (!outFileName.empty()) {
	  Test(IN, outStream, timers, frogTimer, tmpDirName );
  } else {
	  Test(IN, cout, timers, frogTimer, tmpDirName );
  }

  if ( !outFileName.empty() )
    *Log(theErrLog) << "results stored in " << outFileName << endl;
}


void TestServer( Sockets::ServerSocket &conn) {
  //by Maarten van Gompel

  TimerBlock timers;
  Common::Timer frogTimer;
  frogTimer.start();

  try {
    while (true) {
      string data = "";      
      while (true) {
	string tmpdata;
	if ( !conn.read( tmpdata ) )	 //read data from client
	    throw( runtime_error( "read failed" ) );
	
	if (data.length() < 2048) { /* Todo: get TCP_BUFFER_SIZE dynamically from TimblServer and OS */
	    data = data + tmpdata;
	    break;
	}
      }
          	
      if (tpDebug)
	std::cerr << "Received: [" << data << "]" << "\n";
      
      istringstream inputstream(data,istringstream::in);
      ostringstream outputstream;

      *Log(theErrLog) << "Processing... " << endl;
      
      Test(inputstream, outputstream, timers, frogTimer, tmpDirName );
      if (!conn.write( (outputstream.str()) ) || !(conn.write("READY\n"))  )
	  throw( runtime_error( "write to client failed" ) );

    }
  }
  catch ( std::exception& e ) {
    if (tpDebug)
      *Log(theErrLog) << "connection lost: " << e.what() << endl;
  } 
  *Log(theErrLog) << "Connection closed.\n";
}


int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  cerr << "frog " << VERSION << " (c) ILK 1998 - 2011" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  ProgName = argv[0];
  cerr << "based on [" << Tokenizer::VersionName() << ", "
       << Timbl::VersionName() << ", "
       << TimblServer::VersionName() << ", "
       << Tagger::VersionName() << "]" << endl;
  try {
    TimblOpts Opts(argc, argv);
    if ( parse_args(Opts) ){
      //gets a settingsfile for each component, 
      //and starts init for that mod
      
      if ( !fileNames.empty() ) {
	string outPath;
	if ( !outputDirName.empty() )
	  outPath = outputDirName + "/";
	set<string>::const_iterator it = fileNames.begin();
	while ( it != fileNames.end() ){
	  TestFile( testDirName +"/" + *it, outPath + *it + ".out" );
	  ++it;
	}
      }
      else if ( doServer ) {  
	//first set up some things to deal with zombies
	struct sigaction action;
	action.sa_handler = SIG_IGN;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_NOCLDWAIT;
	sigaction(SIGCHLD, &action, NULL); 
	
	srand((unsigned)time(0));
	
	std::cerr << "Listening on port " << listenport << "\n";
	
	try
	  {
	    // Create the socket
	    Sockets::ServerSocket server;
	    if ( !server.connect( listenport ) )
	      throw( runtime_error( "starting server on port " + listenport + " failed" ) );
	    if ( !server.listen( 5 ) ) {
	      // maximum of 5 pending requests
	      throw( runtime_error( "listen(5) failed" ) ); 
	    }
	    while ( true ) {
	      
	      Sockets::ServerSocket conn;
	      if ( server.accept( conn ) ){
		std::cerr << "New connection...\n";
		int pid = fork();				
		if (pid < 0) {
		  std::cerr << "ERROR on fork\n";
		  exit(EXIT_FAILURE);
		} else if (pid == 0)  {
		  //		  server = NULL;
		  TestServer(conn );
		  exit(EXIT_SUCCESS);
		} 
	      }
	      else {
		throw( runtime_error( "Accept failed" ) );
	      }
	    }
	  } catch ( std::exception& e )
	  {
	    std::cerr << "Server error:" << e.what() << "\nExiting.\n";
	  }
	
      } else {
	TestFile( TestFileName, outputFileName );
      }
    }
    else {
      return EXIT_FAILURE;
    }
  }
  catch ( const exception& e ){
    *Log(theErrLog) << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  delete tagger;
  
  return EXIT_SUCCESS;
}
