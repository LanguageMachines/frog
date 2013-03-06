/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2013
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
#include "ticcutils/Configuration.h"
#include "libfolia/document.h"
#include "libfolia/folia.h"
#include "frog/ucto_tokenizer_mod.h"
#include "frog/mbma_mod.h"
#include "frog/mblem_mod.h"
#include "frog/mwu_chunker_mod.h"
#include "frog/cgn_tagger_mod.h"
#include "frog/iob_tagger_mod.h"
#include "frog/ner_tagger_mod.h"
#include "frog/Parser.h"

using namespace std;
using namespace folia;
using namespace TiCC;

LogStream my_default_log( cerr, "frog-", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals

string TestFileName;
string testDirName;
string tmpDirName;
string outputFileName;
string docid = "untitled";
string textclass;
bool wantOUT;
string XMLoutFileName;
bool doXMLin;
bool doXMLout;
bool doKanon;
string outputDirName;
string xmlDirName;
set<string> fileNames;
string ProgName;
int tpDebug = 0; //0 for none, more for more output
unsigned int maxParserTokens = 0; // 0 for unlimited
bool doTok = true;
bool doLemma = true;
bool doMorph = true;
bool doMwu = true;
bool doIOB = true;
bool doNER = true;
bool doParse = true;
bool doDirTest = false;
bool doServer = false;
bool doSentencePerLine = false;
bool doQuoteDetection = false;
string listenport = "void";
string encoding;
string uttmark = "";

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
static string configDir = string(SYSCONF_PATH) + "/" + PACKAGE + "/";
static string configFileName = configDir + "frog.cfg";

void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -e <encoding>          specify encoding of the input (default UTF-8)\n"
       << "\t -t <testfile>          Run frog on this file\n"
       << "\t -x <testfile>          Run frog on this FoLiA XML file. Or the files form 'testdir'\n"
       << "\t --textclass=<cls>      use the specified class to for search text in the the FoLia docs.\n"
       << "\t -S <port>              Run as server instead of reading from testfile\n"
       << "\t --testdir=<directory>  All files in this dir will be tested\n"
       << "\t --uttmarker=<mark>     utterances are separated by 'mark' symbols"
       << "\t                        (default none)\n"
       << "\t -n                     Assume input file to hold one sentence per line\n"
       << "\t --max-parser-tokens=<n> inhibit parsing when a sentence contains over 'n' tokens. (default: no limit)\n"

       << "\t============= MODULE SELECTION ==========================================\n"
       << "\t --skip=[mptncla]    Skip Tokenizer (t), Lemmatizer (l), Morphological Analyzer (a), Chunker (c), Multi-Word Units (m), Named Entity Recognition (n), or Parser (p) \n"
       << "\t============= CONFIGURATION OPTIONS =====================================\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t============= OUTPUT OPTIONS ============================================\n"      
       << "\t -o <outputfile>	    Output columned output to file, instead of default stdout\n"
       << "\t -X <xmlfile>          Output also to an XML file in FoLiA format\n"
       << "\t --id=<docid>          Document ID, used in FoLiA output. (Default 'untitled')\n"
       << "\t --outputdir=<dir>     Output to dir, instead of default stdout\n"
       << "\t --xmldir=<dir>        Use 'dir' to output FoliA XML to.\n"
       << "\t --tmpdir=<directory>  (location to store intermediate files. Default /tmp )\n"      
       << "\t --keep-parser-files=[yes|no] keep intermediate parser files. (last sentence only).\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version .   Show version info.\n"
       << "\t -d <debug level>    (for more verbosity)\n"
       << "\t -Q                   Enable quote detection in tokeniser.\n";
}

//**** stuff to process commandline options *********************************

static Mbma myMbma;
static Mblem myMblem;
static Mwu myMwu;
static Parser myParser;
static CGNTagger myCGNTagger;
static IOBTagger myIOBTagger;
static NERTagger myNERTagger;
static UctoTokenizer tokenizer;


bool parse_args( TimblOpts& Opts ) {
  string value;
  bool mood;
  if ( Opts.Find('V', value, mood ) ||
       Opts.Find("version", value, mood ) ){
    // we already did show what we wanted.
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
    *Log(theErrLog) << "config read from: " << configFileName << endl;
  }
  else {
    cerr << "failed te read configuration from! '" << configFileName << "'" << endl;
    cerr << "did you correctly install the frogdata package?" << endl;
    return false;
  }

  // debug opts
  if ( Opts.Find ('d', value, mood)) {
    if ( !Timbl::stringTo<int>( value, tpDebug ) ){
      *Log(theErrLog) << "-d value should be an integer" << endl;
      return false;
    }
    Opts.Delete('d');
  };
  if ( Opts.Find ('n', value, mood)) {
    doSentencePerLine = true;
  };
  if ( Opts.Find ('Q', value, mood)) {
    doQuoteDetection = true;
  };
  if ( Opts.Find( "skip", value, mood)) {
    string skip = value;
    if ( skip.find_first_of("tT") != string::npos )
      doTok = false;
    if ( skip.find_first_of("lL") != string::npos )
      doLemma = false;
    if ( skip.find_first_of("aA") != string::npos )
      doMorph = false;
    if ( skip.find_first_of("mM") != string::npos )
      doMwu = false;
    if ( skip.find_first_of("cC") != string::npos )
      doIOB = false;
    if ( skip.find_first_of("nN") != string::npos )
      doNER = false;
    if ( skip.find_first_of("pP") != string::npos )
      doParse = false;
    Opts.Delete("skip");
  };

  if ( Opts.Find( "e", value, mood)) {
    encoding = value;
  }
  
  if ( Opts.Find( "max-parser-tokens", value, mood ) ){
    if ( value.empty() ){
      *Log(theErrLog) << "max-parser-tokens option without value " << endl;
      return false;
    }
    else {
      if ( !Timbl::stringTo<unsigned int>( value, maxParserTokens ) ){
	*Log(theErrLog) << "max-parser-tokens value should be an integer" << endl;
	return false;
      }
    }
    Opts.Delete("max-parser-tokens");
  }
  if ( Opts.Find( "keep-parser-files", value, mood ) ){
    if ( value.empty() ||
	 value == "true" || value == "TRUE" || value =="yes" || value == "YES" )
      configuration.setatt( "keepIntermediateFiles", "true", "parser" );
    Opts.Delete("keep-parser-files");
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
  doXMLout = false;
  if ( Opts.Find ( "id", value, mood)) {
    docid = value;
    Opts.Delete( "id");
  }  
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
    doXMLout = true;
    Opts.Delete( "xmldir");
  }
  else if ( Opts.Find ('X', value, mood)) {
    doXMLout = true;
    XMLoutFileName = value;
    Opts.Delete('X');
  }
  if ( Opts.Find ("KANON", value, mood) ){
    doKanon = true;
    Opts.Delete( "KANON" );
  }
  doXMLin = false;
  if ( Opts.Find ('x', value, mood)) {
    doXMLin = true;
    if ( !value.empty() ){
      if ( ! (xmlDirName.empty() && 
	      testDirName.empty() && 
	      TestFileName.empty() ) ){
	*Log(theErrLog) << "-x may not provide a value when --testdir or --xmldir is provided" << endl;
	return false;
      }
      TestFileName = value;
      ifstream is( value.c_str() );
      if ( !is ){
	*Log(theErrLog) << "input stream " << value << " is not readable" << endl;
	return false;
      }
    }
    Opts.Delete('x');
  }
  if ( Opts.Find ( "textclass", value, mood)) {
    if ( !doXMLin ){
      *Log(theErrLog) << "--textclass is only valid when -x is also present" << endl;
      return false;
    }
    textclass = value;
    Opts.Delete( "textclass");
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

  if ( Opts.Find ("uttmarker", value, mood)) {
    uttmark = value;
  }
  if ( !outputDirName.empty() && testDirName.empty() ){
    *Log(theErrLog) << "useless -outputdir option" << endl;
    return false;
  }
  if ( !testDirName.empty() ){
    if ( doXMLin )
      getFileNames( testDirName, ".xml", fileNames );
    else
      getFileNames( testDirName, "", fileNames );
    if ( fileNames.empty() ){
      *Log(theErrLog) << "error: couln't find any files in directory: " 
		      << testDirName << endl;
      return false;
    }
  }
  if ( !doServer && TestFileName.empty() && fileNames.empty() ){
    *Log(theErrLog) << "no frogging without input!" << endl;
    return false;
  }
  return true;
}

bool froginit(){  
  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  if ( doTok && !configuration.hasSection("tokenizer") ){
    *Log(theErrLog) << "Missing [[tokenizer]] section in config file." << endl;
    return false;
  }
  if ( doIOB && !configuration.hasSection("IOB") ){
    *Log(theErrLog) << "Missing [[IOB]] section in config file." << endl;
    return false;
  }
  if ( doNER && !configuration.hasSection("NER") ){
    *Log(theErrLog) << "Missing [[NER]] section in config file." << endl;
    return false;
  }
  if ( doMwu ){
    if ( !configuration.hasSection("mwu") ){
      *Log(theErrLog) << "Missing [[mwu]] section in config file." << endl;
      return false;
    }
  }
  else if ( doParse ){
    *Log(theErrLog) << " Parser disabled, because MWU is deselected" << endl;
    doParse = false;
  }
  
  if ( doServer ){
    // we use fork(). omp (GCC version) doesn't do well when omp is used
    // before the fork!
    // see: http://bisqwit.iki.fi/story/howto/openmp/#OpenmpAndFork
    bool stat = tokenizer.init( configuration, docid, !doTok );
    if ( stat ){
      tokenizer.setSentencePerLineInput( doSentencePerLine );
      tokenizer.setQuoteDetection( doQuoteDetection );
      tokenizer.setInputEncoding( encoding );
      tokenizer.setInputXml( doXMLin );
      tokenizer.setUttMarker( uttmark );
      tokenizer.setTextClass( textclass );
      stat = myCGNTagger.init( configuration );
      if ( stat ){
	if ( doIOB ){
	  stat = myIOBTagger.init( configuration );
	}
	if ( stat && doNER ){
	  stat = myNERTagger.init( configuration );
	}
	if ( stat && doLemma ){
	  stat = myMblem.init( configuration );
	}
	if ( stat && doMorph ){
	  stat = myMbma.init( configuration );
	  if ( stat ) {
	    if ( doMwu ){
	      stat = myMwu.init( configuration );
	      if ( stat && doParse ){
		stat = myParser.init( configuration );
	      }
	    }
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
	tokStat = tokenizer.init( configuration, docid, !doTok );
	if ( tokStat ){
	  tokenizer.setSentencePerLineInput( doSentencePerLine );
	  tokenizer.setQuoteDetection( doQuoteDetection );
	  tokenizer.setInputEncoding( encoding );
	  tokenizer.setInputXml( doXMLin );
	  tokenizer.setUttMarker( uttmark );
	  tokenizer.setTextClass( textclass );
	}
      }
#pragma omp section
      {
	if ( doLemma ){
	  lemStat = myMblem.init( configuration );
	}
      }
#pragma omp section
      {
	if ( doMorph ){
	  mbaStat = myMbma.init( configuration );
	}
      }
#pragma omp section 
      {
	tagStat = myCGNTagger.init( configuration );
      }
#pragma omp section 
      {
	if ( doIOB ){
	  iobStat = myIOBTagger.init( configuration );
	}
      }
#pragma omp section 
      {
	if ( doNER ){
	  nerStat = myNERTagger.init( configuration );
	}
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
      return false;
    }
  }
  *Log(theErrLog) << "Initialization done." << endl;
  return true;
}

vector<Word*> lookup( Word *word, const vector<Entity*>& entities ){
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

Dependency *lookupDep( const Word *word, 
		       const vector<Dependency*>&dependencies ){
  if ( tpDebug ){
    using TiCC::operator<<;
    *Log( theErrLog ) << "lookup "<< word << " in " << dependencies << endl;
  }
  for ( size_t i=0; i < dependencies.size(); ++i ){
    if ( tpDebug ){
      *Log( theErrLog ) << "probeer " << dependencies[i] << endl;
    }
    try {
      vector<DependencyDependent*> dv = dependencies[i]->select<DependencyDependent>();
      if ( !dv.empty() ){
	vector<Word*> v = dv[0]->select<Word>();
	for ( size_t j=0; j < v.size(); ++j ){
	  if ( v[j] == word ){
	    if ( tpDebug ){
	      *Log(theErrLog) << "\nfound word " << v[j] << endl;
	    }
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

string lookupNEREntity( const vector<Word *>& mwu, 
			const vector<Entity*>& entities ){
  string endresult;
  for ( size_t j=0; j < mwu.size(); ++j ){
    if ( tpDebug ){
      using TiCC::operator<<;
      *Log(theErrLog) << "lookup "<< mwu[j] << " in " << entities << endl;
    }
    string result;
    for ( size_t i=0; i < entities.size(); ++i ){
      if ( tpDebug ){
	*Log(theErrLog) << "probeer " << entities[i] << endl;
      }
      try {
	vector<Word*> v = entities[i]->select<Word>();
	bool first = true;
	for ( size_t k=0; k < v.size(); ++k ){
	  if ( v[k] == mwu[j] ){
	    if (tpDebug){
	      *Log(theErrLog) << "found word " << v[k] << endl;
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
	if  (tpDebug > 0) 
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


string lookupIOBChunk( const vector<Word *>& mwu, 
		       const vector<Chunk*>& chunks ){
  string endresult;
  for ( size_t j=0; j < mwu.size(); ++j ){
    if ( tpDebug ){
      using TiCC::operator<<;
      *Log(theErrLog) << "lookup "<< mwu[j] << " in " << chunks << endl;
    }
    string result;
    for ( size_t i=0; i < chunks.size(); ++i ){
      if ( tpDebug ){
	*Log(theErrLog) << "probeer " << chunks[i] << endl;
      }
      try {
	vector<Word*> v = chunks[i]->select<Word>();
	bool first = true;
	for ( size_t k=0; k < v.size(); ++k ){
	  if ( v[k] == mwu[j] ){
	    if (tpDebug){
	      *Log(theErrLog) << "found word " << v[k] << endl;
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
	if  (tpDebug > 0) 
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
      PosAnnotation *postag = word->annotation<PosAnnotation>( );
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

ostream &showResults( ostream& os, 
		      const Sentence* sentence,
		      bool showParse ){
  vector<Word*> words = sentence->words();
  vector<Entity*> mwu_entities = sentence->select<Entity>( myMwu.getTagset() );
  vector<Dependency*> dependencies = sentence->select<Dependency>();
  vector<Chunk*> iob_chunking = sentence->select<Chunk>();
  vector<Entity*> ner_entities = sentence->select<Entity>( myNERTagger.getTagset() );
  static set<ElementType> excludeSet;
  vector<Sentence*> parts = sentence->select<Sentence>( excludeSet );
  if ( !doQuoteDetection )
    assert( parts.size() == 0 );
  for ( size_t i=0; i < parts.size(); ++i ){
    vector<Entity*> ents = parts[i]->select<Entity>( myMwu.getTagset() );
    mwu_entities.insert( mwu_entities.end(), ents.begin(), ents.end() );
    vector<Dependency*> deps = parts[i]->select<Dependency>();
    dependencies.insert( dependencies.end(), deps.begin(), deps.end() );
    vector<Chunk*> chunks = parts[i]->select<Chunk>();
    iob_chunking.insert( iob_chunking.end(), chunks.begin(), chunks.end() );
    vector<Entity*> ners = parts[i]->select<Entity>( myNERTagger.getTagset() );
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
    if ( doNER ){
      string cls;
      string s = lookupNEREntity( mwus[i], ner_entities );
      os << "\t" << s;
    }
    else {
      os << "\t\t";
    }
    if ( doIOB ){
      string cls;
      string s = lookupIOBChunk( mwus[i], iob_chunking );
      os << "\t" << s;
    }
    else {
      os << "\t\t";
    }
    if ( showParse ){
      string cls;
      Dependency *dep = lookupDep( mwus[i][0], dependencies );
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

bool TestSentence( Sentence* sent,
		   ostream& outStream,
		   TimerBlock& timers ){
  vector<Word*> swords;
  if ( doQuoteDetection )
    swords = sent->wordParts();
  else
    swords = sent->words();
  bool showParse = doParse;
  if ( !swords.empty() ) {
#pragma omp parallel sections
    {
#pragma omp section
      {
	timers.tagTimer.start();
	myCGNTagger.Classify( swords );
	timers.tagTimer.stop();
      }
#pragma omp section
      {
	if ( doIOB ){
	  timers.iobTimer.start();
	  myIOBTagger.Classify( swords );
	  timers.iobTimer.stop();
	}
      }
#pragma omp section
      {
	if ( doNER ){
	  timers.nerTimer.start();
	  myNERTagger.Classify( swords );
	  timers.nerTimer.stop();
	}
      }
    } // parallel sections
    for ( size_t i = 0; i < swords.size(); ++i ) {
#pragma omp parallel sections
      {
#pragma omp section
	{
	  if ( doMorph ){
	    timers.mbmaTimer.start();
	    if (tpDebug) 
	      *Log(theErrLog) << "Calling mbma..." << endl;
	    myMbma.Classify( swords[i] );
	    timers.mbmaTimer.stop();
	  }
	}
#pragma omp section
	{
	  if ( doLemma ){
	    timers.mblemTimer.start();
	    if (tpDebug) 
	      *Log(theErrLog) << "Calling mblem..." << endl;
	    myMblem.Classify( swords[i] );
	    timers.mblemTimer.stop();
	  }
	}
      } // omp parallel sections
    } //for int i = 0 to num_words
    
    if ( doMwu ){
      if ( swords.size() > 0 ){
	timers.mwuTimer.start();
	myMwu.Classify( swords );
	timers.mwuTimer.stop();
      }
    }
    if ( doParse ){
      if ( maxParserTokens != 0 && swords.size() > maxParserTokens ){
	showParse = false;
      }
      else {
	myParser.Parse( swords, myMwu.getTagset(), tmpDirName, timers );
      }
    }
  }
  return showParse;
}

void Test( Document& doc,
	   ostream& outStream,
	   const string& xmlOutFile = "" ) {
  TimerBlock timers;
  timers.frogTimer.start();
  // first we make sure that the doc will accept our annotations, by
  // declaring them in the doc
  myCGNTagger.addDeclaration( doc );
  if ( doLemma )
    myMblem.addDeclaration( doc );
  if ( doMorph )
    myMbma.addDeclaration( doc );
  if (doIOB)
    myIOBTagger.addDeclaration( doc );
  if (doNER)
    myNERTagger.addDeclaration( doc );
  if (doMwu) 
    myMwu.addDeclaration( doc );
  if (doParse) 
    myParser.addDeclaration( doc );

  if ( tpDebug > 5 )
    *Log(theErrLog) << "Testing document :" << doc << endl;
  
  vector<Sentence*> topsentences = doc.sentences();
  vector<Sentence*> sentences;
  if ( doQuoteDetection )
    sentences = doc.sentenceParts();
  else
    sentences = topsentences;
  size_t numS = sentences.size();
  if ( numS > 0 ) { //process sentences 
    if  (tpDebug > 0) 
      *Log(theErrLog) << "found " << numS << " sentence(s) in document." << endl;
    for ( size_t i = 0; i < numS; i++) {
      /* ******* Begin process sentence  ********** */
      //NOTE- full sentences are passed (which may span multiple lines) (MvG)
      bool showParse = TestSentence( sentences[i], outStream, timers ); 
      if ( doParse && !showParse ){
	*Log(theErrLog) << "WARNING!" << endl;
	*Log(theErrLog) << "Sentence " << i+1 << " isn't parsed because it contains more tokens then set with the --max-parser-tokens=" << maxParserTokens << " option." << endl;
      }
    }
    for ( size_t i = 0; i < topsentences.size(); ++i ) {
      if ( !(doServer && doXMLout) )
	showResults( outStream, topsentences[i], doParse );
    }
    if ( doServer && doXMLout )
      outStream << doc << endl;
    if ( !xmlOutFile.empty() ){
      doc.save( xmlOutFile, doKanon );
      *Log(theErrLog) << "resulting FoLiA doc saved in " << xmlOutFile << endl;
    }
  } 
  else {
    if  (tpDebug > 0) 
      *Log(theErrLog) << "No sentences found in document. " << endl;
  }
  
  timers.frogTimer.stop();  
  
  *Log(theErrLog) << "tokenisation took:  " << timers.tokTimer << endl;
  *Log(theErrLog) << "CGN tagging took:   " << timers.tagTimer << endl;
  if ( doIOB)
    *Log(theErrLog) << "IOB chunking took:  " << timers.iobTimer << endl;
  if ( doNER)
    *Log(theErrLog) << "NER took:           " << timers.nerTimer << endl;
  if ( doMorph )
    *Log(theErrLog) << "MBA took:           " << timers.mbmaTimer << endl;
  if ( doLemma )
    *Log(theErrLog) << "Mblem took:         " << timers.mblemTimer << endl;
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
  *Log(theErrLog) << "Frogging in total took: " << timers.frogTimer << endl;
  return;
}

void Test( const string& infilename,
	   const string& outName,
	   const string& xmlOutFile ) {
  // stuff the whole input into one FoLiA document.
  // This is not a good idea on the long term, I think (agreed [proycon] )
  ostream *os;
  if ( outName.empty() ){
    os = &cout;
  }
  else {
    os = new ofstream( outName.c_str() );
    if ( os->bad() ){
      *Log(theErrLog) << "unable to open outputfile: " << outName << endl;
      exit( EXIT_FAILURE );
    }
  }
  *Log(theErrLog) << "Frogging " << infilename << endl;
  if ( doXMLin ){
    Document doc;
    try {
      doc.readFromFile( infilename );
    }
    catch ( exception &e ){
      *Log(theErrLog) << "retrieving FoLiA from '" << infilename << "' failed" << endl;
      if ( !outName.empty() ){
	delete os;
      }
      return;
    }
    tokenizer.tokenize( doc );
    Test( doc, *os, xmlOutFile );
  }
  else {
    ifstream IN( infilename.c_str() );
    Document doc = tokenizer.tokenize( IN );
    Test( doc, *os, xmlOutFile );
  }
  if ( !outName.empty() ){
    *Log(theErrLog) << "results stored in " << outName << endl;
    delete os;
  }
}

void TestServer( Sockets::ServerSocket &conn) {
  //by Maarten van Gompel
  
  try {
    while (true) {
      ostringstream outputstream;
      if ( doXMLin ){
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
	if ( tpDebug )
	  *Log(theErrLog) << "received data [" << result << "]" << endl;	
	Document doc;
	try {
	  doc.readFromString( result );
	}
	catch ( std::exception& e ){
	  *Log(theErrLog) << "FoLiaParsing failed:" << endl
			  << e.what() << endl;	  
	  throw;
	}
	*Log(theErrLog) << "Processing... " << endl;
	tokenizer.tokenize( doc );
	Test( doc, outputstream );
      }
      else {
	string data = "";
	if ( doSentencePerLine ){
	  if ( !conn.read( data ) )	 //read data from client
	    throw( runtime_error( "read failed" ) );
	}
	else {
	  string line;
	  while( conn.read(line) ){
	    if ( line == "EOT" )
	      break;
	    data += line + "\n";
	  }
	}
	if (tpDebug)
	  *Log(theErrLog) << "Received: [" << data << "]" << endl;
	*Log(theErrLog) << "Processing... " << endl;
	istringstream inputstream(data,istringstream::in);
	Document doc = tokenizer.tokenize( inputstream ); 
	Test( doc, outputstream );
      }
      if (!conn.write( (outputstream.str()) ) || !(conn.write("READY\n"))  ){
	if (tpDebug)
	  *Log(theErrLog) << "socket " << conn.getMessage() << endl;	
	throw( runtime_error( "write to client failed" ) );
      }
      
    }
  }
  catch ( std::exception& e ) {
    if (tpDebug)
      *Log(theErrLog) << "connection lost: " << e.what() << endl;
  } 
  *Log(theErrLog) << "Connection closed.\n";
}


int main(int argc, char *argv[]) {
  cerr << "frog " << VERSION << " (c) ILK 1998 - 2013" << endl;
  cerr << "Induction of Linguistic Knowledge Research Group, Tilburg University" << endl;
  ProgName = argv[0];
  cerr << "based on [" << Tokenizer::VersionName() << ", "
       << folia::VersionName() << ", "
       << Timbl::VersionName() << ", "
       << TimblServer::VersionName() << ", "
       << Tagger::VersionName() << "]" << endl;
  //  cout << "configdir: " << configDir << endl;
  std::ios_base::sync_with_stdio(false);
  try {
    TimblOpts Opts(argc, argv);
        
    if ( parse_args(Opts) ){
      if (  !froginit() ){
	throw runtime_error( "init failed" );
      }
      
#ifdef HAVE_OPENMP      
      if ( doServer ) {
      	// run in one thread in server mode, forking is too expensive for lots of small snippets
      	omp_set_num_threads( 1 );
      }
#endif
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
	  string outName;
	  if ( doXMLin ){
	    if ( !outPath.empty() )
	      outName = outPath + *it + ".out";
	  }
	  else
	    outName = outPath + *it + ".out";
	  string xmlName;
	  if ( !xmlDirName.empty() ){
	    if ( it->rfind(".xml") == string::npos )
	      xmlName = xmlPath + *it + ".xml";
	    else
	      xmlName = xmlPath + *it;
	  }
	  else if ( doXMLout )
	    xmlName = *it + ".xml"; // do not clobber the inputdir!
	  Test( testName, outName, xmlName );
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
	
	*Log(theErrLog) << "Listening on port " << listenport << "\n";
	
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
		*Log(theErrLog) << "New connection..." << endl;
		int pid = fork();				
		if (pid < 0) {
		  *Log(theErrLog) << "ERROR on fork" << endl;
		  throw runtime_error( "FORK failed" );
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
	    *Log(theErrLog) << "Server error:" << e.what() << " Exiting." << endl;
	    throw;
	  }
	
      } else {
	if ( doXMLout && XMLoutFileName.empty() )
	  XMLoutFileName = TestFileName + ".xml";
	if ( wantOUT && outputFileName.empty() )
	  outputFileName = TestFileName + ".out";
	Test( TestFileName, outputFileName, XMLoutFileName );
      }
    }
    else {
      throw runtime_error( "invalid arguments" );
    }
  }
  catch ( const exception& e ){
    *Log(theErrLog) << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
