/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2012
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
#include "libfolia/document.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "frog/mbma_mod.h"
#include "frog/mblem_mod.h"
#include "frog/mwu_chunker_mod.h"
#include "frog/cgn_tagger_mod.h"
#include "frog/Parser.h"

using namespace std;
using namespace folia;

Tokenizer::TokenizerClass tokenizer;

LogStream my_default_log( cerr, "frog-", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals

string TestFileName;
string testDirName;
string tmpDirName;
string outputFileName;
bool wantOUT;
string XMLoutFileName;
bool wantXML;
string outputDirName;
string xmlDirName;
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
       << "\t -X <xmlfile>      Output also to an XML file in folia format\n"
       << "\t --outputdir=<dir> Output to dir, instead of default stdout\n"
       << "\t --xmldir=<dir> Use 'dir' to output FoliA XML to.\n"      
       << "\t --keep-parser-files=[yes|no] keep intermediate parser files, (last sentence only)\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version . Show version info.\n"
       << "\t -d <debug level> (for more verbosity)\n"
       << "\t -Q               Enable quote detection in tokeniser\n";
}

//**** stuff to process commandline options *********************************

static Mbma myMbma;
static Mblem myMblem;
static Mwu myMwu;
static Parser myParser;
static MBTagger myCGNTagger;

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
#ifdef HAVE_DIRENT_H
  if ( !tmpDirName.empty() ){
    if ( !existsDir( tmpDirName ) ){
      *Log(theErrLog) << "temporary dir " << tmpDirName << " not readable" << endl;
      return false;
    }
    *Log(theErrLog) << "checking tmpdir: " << tmpDirName << " OK" << endl;
  }
#endif
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
  wantOUT = false;
  if ( Opts.Find( "outputdir", value, mood)) {
    outputDirName = value;
#ifdef HAVE_DIRENT_H
    if ( !outputDirName.empty() ){
      if ( !existsDir( outputDirName ) ){
	*Log(theErrLog) << "output dir " << outputDirName << " not readable" << endl;
	return false;
      }
    }
#endif
    wantOUT = true;
    Opts.Delete( "outputdir");
  }
  else if ( Opts.Find ('o', value, mood)) {
    wantOUT = true;
    outputFileName = value;
    Opts.Delete('o');
  };
  wantXML = false;
  if ( Opts.Find( "xmldir", value, mood)) {
    xmlDirName = value;
#ifdef HAVE_DIRENT_H
    if ( !xmlDirName.empty() ){
      if ( !existsDir( xmlDirName ) ){
	*Log(theErrLog) << "XML output dir " << xmlDirName << " not readable" << endl;
	return false;
      }
    }
#endif
    wantXML = true;
    Opts.Delete( "xmldir");
  }
  else if ( Opts.Find ('X', value, mood)) {
    wantXML = true;
    XMLoutFileName = value;
    Opts.Delete('X');
  }
  
  if ( !outputFileName.empty() && !testDirName.empty() ){
    *Log(theErrLog) << "useless -o value" << endl;
    return false;
  }
  if ( !XMLoutFileName.empty() && !testDirName.empty() ){
    *Log(theErrLog) << "useless -X value" << endl;
    return false;
  }

  if ( Opts.Find ('S', value, mood)) {
    doServer = true;
    listenport= value;
  }
  if ( !outputDirName.empty() && testDirName.empty() ){
    *Log(theErrLog) << "useless -outputdir option" << endl;
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
  return true;
}

bool froginit(){  
  if ( doServer ){
    // we use fork(). omp (GCC version) doesn't do well when omp is used
    // before the fork!
    // see: http://bisqwit.iki.fi/story/howto/openmp/#OpenmpAndFork
    bool stat = myCGNTagger.init( configuration );
    if ( stat ){
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
    bool tagStat = true;
#pragma omp parallel sections
    {
#pragma omp section
      lemStat = myMblem.init( configuration );
#pragma omp section
      mbaStat = myMbma.init( configuration );
#pragma omp section 
      tagStat = myCGNTagger.init( configuration );
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
    if ( ! ( tagStat && lemStat && mbaStat && mwuStat && parStat ) ){
      *Log(theErrLog) << "Initialization failed for: ";
      if ( ! ( tagStat ) ){
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

void TestSentence( FoliaElement* sent,
		   const string& tmpDir,
		   TimerBlock& timers ){
  vector<Word*> swords = sent->words();
  if ( !swords.empty() ) {
    timers.tagTimer.start();
    string tagged = myCGNTagger.Classify( sent );
    timers.tagTimer.stop();
    for ( size_t i = 0; i < swords.size(); ++i ) {
      string mbmaLemma;
      string mblemLemma;
#pragma omp parallel sections
      {
#pragma omp section
	{
	  timers.mbmaTimer.start();
	  if (tpDebug) cout << "Calling mbma..." << endl;
	  mbmaLemma = myMbma.Classify( swords[i] );
	  timers.mbmaTimer.stop();
	}
#pragma omp section
	{
	  timers.mblemTimer.start();
	  if (tpDebug) cout << "Calling mblem..." << endl;
	  mblemLemma = myMblem.Classify( swords[i] );
	  timers.mblemTimer.stop();
	}
      }
      if (tpDebug) {
	cout   << "tagged word[" << i <<"]: " << swords[i]->str() << " tag "
	       << swords[i]->pos() << endl;
	cout << "analysis: " << mblemLemma << " " << mbmaLemma << endl;
      }
    } //for int i = 0 to num_words

    if ( doMwu ){
      if ( swords.size() > 0 ){
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
    if ( doParse ){  
      myParser.Parse( sent, tmpDir, timers );
    }
  }
}

vector<Word*> lookup( Word *word, const vector<Entity*>& entities ){
  vector<Word*> vec;
  for ( size_t p=0; p < entities.size(); ++p ){
    vec = entities[p]->select<Word>();
    if ( !vec.empty() ){
      if ( vec[0]->id() == word->id() ) {
	// cerr << "found " << vec << endl;
	return vec;
      }
    }
  }
  vec.clear();
  vec.push_back( word ); // single unit
  return vec;
}

Dependency *lookupDep( const Word *word, 
		       const vector<Dependency*>&dependencies ){
  //  cerr << "\nlookup "<< word << " in " << dependencies << endl;
  for ( size_t i=0; i < dependencies.size(); ++i ){
    //    cerr << "\nprobeer " << dependencies[i] << endl;
    try {
      vector<DependencyDependent*> dv = dependencies[i]->select<DependencyDependent>();
      if ( !dv.empty() ){
	vector<Word*> v = dv[0]->select<Word>();
	for ( size_t j=0; j < v.size(); ++j ){
	  if ( v[j] == word ){
	    //	    cerr << "\nfound word " << v[j] << endl;
	    return dependencies[i];
	  }
	}
      }
    }
    catch ( exception& e ){
      if  (tpDebug > 0) 
	*Log(theErrLog) << "get Dependency results failed: " 
			<< e.what() << endl;      
    }
  }
  return 0;
}

void displayMWU( ostream& os, size_t index, 
		 const vector<Word*> mwu ){
  string wrd;
  string pos;
  string lemma;
  string morph;
  double conf = 1;
  for ( size_t p=0; p < mwu.size(); ++p ){
    Word *word = mwu[p];
    try { 
      wrd += word->str();
      PosAnnotation *postag = word->annotation<PosAnnotation>();
      pos += postag->cls();
      if ( p < mwu.size() -1 ){
	wrd += "_";
	pos += "_";
      }
      conf *= postag->confidence();
    }
    catch ( exception& e ){
      if  (tpDebug > 0) 
	*Log(theErrLog) << "get Postag results failed: " 
			<< e.what() << endl;            
    }
    try { 
      lemma += word->lemma();
      if ( p < mwu.size() -1 ){
	lemma += "_";
      }
    }
    catch ( exception& e ){
      if  (tpDebug > 0) 
	*Log(theErrLog) << "get Lemma results failed: " 
			<< e.what() << endl;      
    }
    try { 
      vector<MorphologyLayer*> ml = word->annotations<MorphologyLayer>();
      for ( size_t q=0; q < ml.size(); ++q ){
	vector<Morpheme*> m = ml[q]->select<Morpheme>();
	for ( size_t t=0; t < m.size(); ++t ){
	  morph += "[" + UnicodeToUTF8( m[t]->text() ) + "]";
	}
	if ( q < ml.size()-1 )
	  morph += "/";
      }
      if ( p < mwu.size() -1 ){
	morph += "_";
      }
    }
    catch ( exception& e ){
      if  (tpDebug > 0) 
	*Log(theErrLog) << "get Morph results failed: " 
			<< e.what() << endl;      
    }
  }
  os << index << "\t" << wrd << "\t" << lemma << "\t" << morph << "\t" << pos << "\t" << std::fixed << conf;
}  

ostream &showResults( ostream& os, const FoliaElement* sentence ){
  vector<Word*> words = sentence->words();
  vector<Entity*> entities = sentence->select<Entity>();
  vector<Dependency*> dependencies = sentence->select<Dependency>();
  size_t index = 1;
  map<FoliaElement*, int> enumeration;
  vector<vector<Word*> > mwus;
  for( size_t i=0; i < words.size(); ++i ){
    Word *word = words[i];
    vector<Word*> mwu = lookup( word, entities );
    for ( size_t j=0; j < mwu.size(); ++j ){
      enumeration[mwu[j]] = index;
    }
    mwus.push_back( mwu );
    i += mwu.size()-1;
    ++index;
  }
  for( size_t i=0; i < mwus.size(); ++i ){
    displayMWU( os, i+1, mwus[i] );
    if ( doParse ){
      string cls;
      Dependency *dep = lookupDep( mwus[i][0], dependencies );
      if ( dep ){
	vector<DependencyHead*> w = dep->select<DependencyHead>();
	os << "\t" << enumeration.find(w[0]->index(0))->second << "\t" << dep->cls();
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



void Test( istream& IN,
	   ostream& outStream,
	   TimerBlock& timers,
	   Common::Timer &frogTimer,
	   const string& xmlOutFile,
	   const string& tmpDir ) {

  string eosmarker = "";
  tokenizer.setEosMarker( eosmarker );
  tokenizer.setVerbose( false );
  tokenizer.setPassThru( !doTok );
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

  vector<Sentence*> sentences = doc.sentences();
  size_t numS = sentences.size();
  if ( numS > 0 ) { //process sentences 
    if  (tpDebug > 0) *Log(theErrLog) << "[tokenize] " << numS << " sentence(s) in buffer, processing..." << endl;
    for ( size_t i = 0; i < numS; i++) {
      /* ******* Begin process sentence  ********** */
      TestSentence( sentences[i],  tmpDir, timers ); 
      //NOTE- full sentences are passed (which may span multiple lines) (MvG)         
      showResults( outStream, sentences[i] ); 
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
  if ( !xmlOutFile.empty() ){
    doc.save( xmlOutFile );
    *Log(theErrLog) << "resulting FoLiA doc saved in " << xmlOutFile << endl;
  }
  return;
}

void TestFile( const string& infilename, 
	       const string& outFileName,
	       const string& xmlOutFile ) {
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
    Test(IN, outStream, timers, frogTimer, xmlOutFile, tmpDirName );
  } else {
    Test(IN, cout, timers, frogTimer, xmlOutFile, tmpDirName );
  }
  
  if ( !outFileName.empty() )
    *Log(theErrLog) << "results stored in " << outFileName << endl;
}


void TestServer( Sockets::ServerSocket &conn) {
  //by Maarten van Gompel

  
  

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
      
      TimerBlock timers;
  	  Common::Timer frogTimer;
  	  frogTimer.start();
      Test(inputstream, outputstream, timers, frogTimer, "", tmpDirName );
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
  cerr << "frog " << VERSION << " (c) ILK 1998 - 2012" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  ProgName = argv[0];
  cerr << "based on [" << Tokenizer::VersionName() << ", "
       << folia::VersionName() << ", "
       << Timbl::VersionName() << ", "
       << TimblServer::VersionName() << ", "
       << Tagger::VersionName() << "]" << endl;
  try {
    TimblOpts Opts(argc, argv);
        
    if ( parse_args(Opts) ){
      if (  !froginit() ){
	cerr << "terminated." << endl;
	return EXIT_FAILURE;
      }
      
      if ((tpDebug) || (doServer)) {
      	//don't mangle debug output, so run 1 thread then.. also run in one thread in server mode, forking is too expensive for lots of small snippets
      	omp_set_num_threads( 1 );
      }
      if ( !fileNames.empty() ) {
	string outPath;
	string xmlPath;
	if ( !outputDirName.empty() )
	  outPath = outputDirName + "/";
	if ( !xmlDirName.empty() )
	  xmlPath = xmlDirName + "/";
	set<string>::const_iterator it = fileNames.begin();
	while ( it != fileNames.end() ){
	  string testName = testDirName +"/" + *it;
	  string outName = outPath + *it + ".out";
	  string xmlName;
	  if ( !xmlDirName.empty() )
	    xmlName = xmlPath + *it + ".xml";
	  else if ( wantXML )
	    xmlName = *it + ".xml"; // do not clobber the inputdir!
	  TestFile( testName, outName, xmlName );
	  ++it;
	}
      }
      else if ( doServer ) {  
	//first set up some things to deal with zombies
	struct sigaction action;
	action.sa_handler = SIG_IGN;
	sigemptyset(&action.sa_mask);
#ifdef SA_NOCLDWAIT
	action.sa_flags = SA_NOCLDWAIT;
#endif	
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
	    exit(EXIT_FAILURE);
	  }
	
      } else {
	if ( wantXML && XMLoutFileName.empty() )
	  XMLoutFileName = TestFileName + ".xml";
	if ( wantOUT && outputFileName.empty() )
	  outputFileName = TestFileName + ".out";
	TestFile( TestFileName, outputFileName, XMLoutFileName );
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
  //  delete tagger;
  
  return EXIT_SUCCESS;
}
