/*
  $Id: Frog.cxx 13560 2011-11-22 13:51:45Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/Frog/trunk/src/Frog.cxx $

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

#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "frog/Frog.h"
#include "frog/cgn_tagger_mod.h"

using namespace std;
using namespace folia;

MBTagger::MBTagger(){
  tagger = 0;
}

MBTagger::~MBTagger(){
  delete tagger;
}

bool MBTagger::init( const Configuration& conf ){
  if ( tagger != 0 ){
    *Log(theErrLog) << "MBTagger is already initialized!" << endl;
    return false;
  }  
  string tagset = conf.lookUp( "settings", "tagger" );
  if ( tagset.empty() ){
    *Log(theErrLog) << "Unable to find settings for Tagger" << endl;
    return false;
  }
  string init = "-s " + configuration.configDir() + tagset;
  if ( Tagger::Version() == "3.2.6" )
    init += " -vc";
  else
    init += " -vcf";
  tagger = new MbtAPI( init, *theErrLog );
  return tagger != 0;
}

static bool splitOneWT( const string& inp, string& word, string& tag, string& confidence ){
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

static int splitWT( const string& tagged,
	     vector<string>& tags, vector<double>& conf ){
  vector<string> words;
  vector<string> tagwords;
  vector<bool> known;
  tags.clear();
  conf.clear();
  size_t num_words = Timbl::split_at( tagged, tagwords, " " );
  num_words--; // the last "word" is <utt> which gets added by the tagger
  for( size_t i = 0; i < num_words; ++i ) {
    string word, tag, confs;
    bool isKnown = splitOneWT( tagwords[i], word, tag, confs );
    double confidence;
    if ( !stringTo<double>( confs, confidence ) ){
      *Log(theErrLog) << "tagger confused. Expected a double, got '" << confs << "'" << endl;
      exit( EXIT_FAILURE );
    }
    words.push_back( word );
    tags.push_back( tag );
    known.push_back( isKnown );
    conf.push_back( confidence );
  }
  if (tpDebug) {
    cout << "#tagged_words: " << num_words << endl;
    for( size_t i = 0; i < num_words; i++) 
      cout   << "\ttagged word[" << i <<"]: " << words[i] << (known[i]?"/":"//")
	     << tags[i] << " <" << conf[i] << ">" << endl;
  }
  return num_words;
}

string getSubSet( const string& val, const string& tag ){
  if ( val == "soort" ||
       val == "eigen" )
    return "ntype";
  else if ( ( tag == "N" || tag == "VNW" ) &&
	    ( val == "ev" ||
	      val == "mv" ||
	      val == "getal" ) )
    return "getal";
  else if ( val == "zijd" ||
	    val == "onz" ||
	    val == "masc" ||
	    val == "fem" ||
	    val == "genus" )
    return "genus";
  else if ( val == "stan" ||
	    val == "gen" ||
	    val == "dat" ||
	    val == "nomin" ||
	    val == "obl" ||
	    val == "bijz" )
    return "naamval";
  else if ( val == "afgebr" ||
	    val == "afk" ||
	    val == "deeleigen" ||
	    val == "symb" ||
	    val == "vreemd" ||
	    val == "enof" ||
	    val == "meta" ||
	    val == "achter" ||
	    val == "comment" ||
	    val == "onverst" )
    return "spectype";
  else if ( val == "neven" ||
	    val == "onder" )
    return "conjtype";
  else if ( val == "init" ||
	    val == "versm" ||
	    val == "fin" )
    return "vztype";
  else if ( val == "agr" ||
	    val == "evon" ||
	    val == "rest" ||
	    val == "evz" ||
	    val == "agr3" ||
	    val == "evmo" ||
	    val == "rest3" ||
	    val == "evf" )
    return "npagr";
  else if ( val == "bep" ||
	    val == "onbep" )
    return "lwtype";
  else if ( val == "pers" ||
	    val == "pr" ||
	    val == "refl1" ||
	    val == "recip" ||
	    val == "bez" ||
	    val == "vb" ||
	    val == "vrag" ||
	    val == "betr" ||
	    val == "excl" ||
	    val == "aanw" ||
	    val == "onbep" )
    return "vwtype";
  else if ( val == "adv-pron" ||
	    val == "pron" ||
	    val == "det" ||
	    val == "grad" )
    return "pdtype";
  else if ( val == "vol" ||
	    val == "red" ||
	    val == "nadr" )
    return "status";
  else if ( val == "1" ||
	    val == "2" ||
	    val == "2v" ||
	    val == "2b" ||
	    val == "3" ||
	    val == "3p" ||
	    val == "3m" ||
	    val == "3v" ||
	    val == "3o" ||
	    val == "persoon" )
    return "persoon";
  else if ( val == "prenom" ||
	    val == "postnom" ||
	    val == "nom" ||
	    val == "vrij" )
    return "positie";
  else if ( val == "zonder" ||
	    val == "met-e" ||
	    val == "met-s" )
    return "buiging";
  else if ( val == "zonder-v" ||
	    val == "mv-n" ||
	    val == "zonder-n" )
    return "getal-n";
  else if ( val == "basis" ||
	    val == "comp" ||
	    val == "sup" ||
	    val == "dim" )
    return "graad";
  else if ( val == "pv" ||
	    val == "inf" ||
	    val == "vd" ||
	    val == "od" )
    return "wvorm";
  else if ( val == "tgw" ||
	    val == "verl" ||
	    val == "conj" )
    return "pvtijd";
  else if ( tag == "WW" &&
	    ( val == "ev" ||
	      val == "mv" ||
	      val == "met-t" ) )
    return "pvagr";
  else if ( val == "hoofd" ||
	    val == "rang" )
    return "numtype";
  else if ( val == "dial" )
    return "dial";
  else
    throw folia::ValueError( "unknown cgn subset for tag: '" + val + "'" );
}

void addTag( AbstractElement *word, const string& inputTag, double confidence ){
  string mainTag;
  string tagPartS;
  string annotator = "MBT";
  string ucto_class = word->cls();
  if ( ucto_class == "ABBREVIATION-KNOWN" ){
    annotator = "ucto";
    mainTag = "SPEC";
    tagPartS = "afk";
    confidence = 1.0;
  }
  else if ( ucto_class == "SMILEY" ){
    annotator = "ucto";
    mainTag = "SPEC";
    tagPartS = "symb";
    confidence = 1.0;
  }
  else {
    string::size_type openH = inputTag.find( '(' );
    string::size_type closeH = inputTag.find( ')' );
    if ( openH == string::npos || closeH == string::npos ){
      *Log(theErrLog) << "tagger_mod: main tag without subparts: impossible: " << inputTag << endl;
      exit(-1);
    }
    mainTag = inputTag.substr( 0, openH );
    if ( mainTag == "SPEC" )
      confidence = 1.0;
    tagPartS = inputTag.substr( openH+1, closeH-openH-1 );
  }
  KWargs args = getArgs( "set='mbt-pos', cls='" + mainTag
			 + "', annotator='" + annotator + "', confidence='" 
			 + toString(confidence) + "'" );
  AbstractElement *pos = word->addPosAnnotation( args );
  vector<string> tagParts;
  size_t numParts = Timbl::split_at( tagPartS, tagParts, "," );
  for ( size_t i=0; i < numParts; ++i ){
    string arg = "set='mbt-pos', subset='" + getSubSet( tagParts[i], mainTag ) 
      + "', cls='" + tagParts[i] + "', annotator='MBT'";
    AbstractElement *feat = new folia::Feature( arg );
    pos->append( feat );
  }
}

string MBTagger::Classify( AbstractElement *sent, vector<string>& tags ){
  tags.clear();
  string tagged;
  vector<AbstractElement*> swords = sent->words();
  if ( !swords.empty() ) {
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
    tagged = tagger->Tag(sentence);
    if (tpDebug) {
      cout << "sentence: " << sentence
	   <<"\ntagged: "<< tagged
	   << endl;
    }
    vector<double> conf;
    int num_words = splitWT( tagged, tags, conf );
    for ( int i = 0; i < num_words; ++i ) {
      addTag( swords[i], tags[i], conf[i] );
    }
  }
  return tagged;
}


