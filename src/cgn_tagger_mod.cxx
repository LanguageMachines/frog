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

static multimap<string,string> cgnSubSets;
static multimap<string,string> cgnConstraints;

void fillSubSetTable(){
  // should become a config file!
  cgnSubSets.insert( make_pair("soort", "ntype" ));
  cgnSubSets.insert( make_pair("eigen", "ntype" ));
  
  cgnSubSets.insert( make_pair("ev", "getal" ));
  cgnSubSets.insert( make_pair("mv", "getal" ));
  cgnSubSets.insert( make_pair("getal", "getal" ));
  
  cgnSubSets.insert( make_pair("zijd", "genus" ));
  cgnSubSets.insert( make_pair("onz", "genus" ));
  cgnSubSets.insert( make_pair("masc", "genus" ));
  cgnSubSets.insert( make_pair("fem", "genus" ));
  cgnSubSets.insert( make_pair("genus", "genus" ) );
  
  cgnSubSets.insert( make_pair("stan", "naamval" ));
  cgnSubSets.insert( make_pair("gen", "naamval" ));
  cgnSubSets.insert( make_pair("dat", "naamval" ));
  cgnSubSets.insert( make_pair("nomin", "naamval" ));
  cgnSubSets.insert( make_pair("obl", "naamval" ));
  cgnSubSets.insert( make_pair("bijz", "naamval" ));
    
  cgnSubSets.insert( make_pair("afgebr", "spectype" ));
  cgnSubSets.insert( make_pair("afk", "spectype" ));
  cgnSubSets.insert( make_pair("deeleigen", "spectype" ));
  cgnSubSets.insert( make_pair("symb", "spectype" ));
  cgnSubSets.insert( make_pair("vreemd", "spectype" ));
  cgnSubSets.insert( make_pair("enof", "spectype" ));
  cgnSubSets.insert( make_pair("meta", "spectype" ));
  cgnSubSets.insert( make_pair("achter", "spectype" ));
  cgnSubSets.insert( make_pair("comment", "spectype" ));
  cgnSubSets.insert( make_pair("onverst", "spectype" ));
  
  cgnSubSets.insert( make_pair("neven", "conjtype" ));
  cgnSubSets.insert( make_pair("onder", "conjtype" ));
    
  cgnSubSets.insert( make_pair("init", "vztype" ));
  cgnSubSets.insert( make_pair("versm", "vztype" ));
  cgnSubSets.insert( make_pair("fin", "vztype" ));
    
  cgnSubSets.insert( make_pair("agr", "npagr" ));
  cgnSubSets.insert( make_pair("evon", "npagr" ));
  cgnSubSets.insert( make_pair("rest", "npagr" ));
  cgnSubSets.insert( make_pair("evz", "npagr" ));
  cgnSubSets.insert( make_pair( "agr3", "npagr" ));
  cgnSubSets.insert( make_pair( "evmo", "npagr" ));
  cgnSubSets.insert( make_pair( "rest3", "npagr" ));
  cgnSubSets.insert( make_pair("evf", "npagr" ));
  
  cgnSubSets.insert( make_pair("bep", "lwtype" ));
  cgnSubSets.insert( make_pair("onbep", "lwtype" ));
  
  cgnSubSets.insert( make_pair("pers", "vwtype" ));
  cgnSubSets.insert( make_pair("pr", "vwtype" ));
  cgnSubSets.insert( make_pair( "refl1", "vwtype" ));
  cgnSubSets.insert( make_pair("recip", "vwtype" ));
  cgnSubSets.insert( make_pair("bez", "vwtype" ));
  cgnSubSets.insert( make_pair("vb", "vwtype" ));
  cgnSubSets.insert( make_pair("vrag", "vwtype" ));
  cgnSubSets.insert( make_pair("betr", "vwtype" ));
  cgnSubSets.insert( make_pair("excl", "vwtype" ));
  cgnSubSets.insert( make_pair("aanw", "vwtype" ));
  cgnSubSets.insert( make_pair("onbep", "vwtype" ));
    
  cgnSubSets.insert( make_pair("adv-pron", "pdtype" ));
  cgnSubSets.insert( make_pair("pron", "pdtype" ));
  cgnSubSets.insert( make_pair("det", "pdtype" ));
  cgnSubSets.insert( make_pair("grad", "pdtype" ));
  
  cgnSubSets.insert( make_pair("vol", "status" ));
  cgnSubSets.insert( make_pair("red", "status" ));
  cgnSubSets.insert( make_pair("nadr", "status" ));
  
  cgnSubSets.insert( make_pair("1", "persoon" ));
  cgnSubSets.insert( make_pair("2", "persoon" ));
  cgnSubSets.insert( make_pair("2v", "persoon" ));
  cgnSubSets.insert( make_pair("2b", "persoon" ));
  cgnSubSets.insert( make_pair("3", "persoon" ));
  cgnSubSets.insert( make_pair("3p", "persoon" ));
  cgnSubSets.insert( make_pair("3m", "persoon" ));
  cgnSubSets.insert( make_pair("3v", "persoon" ));
  cgnSubSets.insert( make_pair("3o", "persoon" ));
  cgnSubSets.insert( make_pair("persoon", "persoon" ));
  
  cgnSubSets.insert( make_pair("prenom", "positie" ));
  cgnSubSets.insert( make_pair("postnom", "positie" ));
  cgnSubSets.insert( make_pair("nom", "positie" ));
  cgnSubSets.insert( make_pair("vrij", "positie" ));
  
  cgnSubSets.insert( make_pair("zonder", "buiging" ));
  cgnSubSets.insert( make_pair("met-e", "buiging" ));
  cgnSubSets.insert( make_pair("met-s", "buiging" ));
    
  cgnSubSets.insert( make_pair("zonder-v", "getal-n" ));
  cgnSubSets.insert( make_pair("mv-n", "getal-n" ));
  cgnSubSets.insert( make_pair("zonder-n", "getal-n" ));
    
  cgnSubSets.insert( make_pair("basis", "graad" ));
  cgnSubSets.insert( make_pair("comp", "graad" ));
  cgnSubSets.insert( make_pair("sup", "graad" ));
  cgnSubSets.insert( make_pair("dim", "graad" ));
  
  cgnSubSets.insert( make_pair("pv", "wvorm" ));
  cgnSubSets.insert( make_pair("inf", "wvorm" ));
  cgnSubSets.insert( make_pair("vd", "wvorm" ));
  cgnSubSets.insert( make_pair("od", "wvorm" ));
    
  cgnSubSets.insert( make_pair("tgw", "pvtijd" ));
  cgnSubSets.insert( make_pair("verl", "pvtijd" ));
  cgnSubSets.insert( make_pair("conj", "pvtijd" ));
  
  cgnSubSets.insert( make_pair("ev", "pvagr" ));
  cgnSubSets.insert( make_pair("mv", "pvagr" ));
  cgnSubSets.insert( make_pair( "met-t", "pvagr" ));
    
  cgnSubSets.insert( make_pair("hoofd", "numtype" ));
  cgnSubSets.insert( make_pair("rang", "numtype" ));
  
  cgnSubSets.insert( make_pair("dial", "dial" ));

  cgnConstraints.insert( make_pair( "getal", "N" ) );
  cgnConstraints.insert( make_pair( "getal", "VNW" ) );
  cgnConstraints.insert( make_pair( "pvagr", "WW" ) );
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
  fillSubSetTable();
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

string getSubSet( const string& val, const string& head ){
  typedef multimap<string,string>::const_iterator mm_iter;
  mm_iter it = cgnSubSets.find( val );
  if ( it == cgnSubSets.end() )
    throw folia::ValueError( "unknown cgn subset for class: '" + val + "'" );
  string result;
  while ( it != cgnSubSets.upper_bound(val) ){
    result = it->second;
    mm_iter cit = cgnConstraints.find( result );
    if ( cit == cgnConstraints.end() ){
      // no constraints on this value
      return result;
    }
    else {
      while ( cit != cgnConstraints.upper_bound( result ) ){
	if ( cit->second == head ) {
	  // allowed
	  return result;
	}
	++cit;
      }
    }
    ++it;
  }
  throw folia::ValueError( "unable to find cgn subset for class: '" + val + 
			   "' whithin the constraints for '" + head + "'" );
}

void addTag( FoliaElement *word, const string& inputTag, double confidence ){
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
  else if ( ucto_class == "SMILEY" ||
	    ucto_class == "URL-WWW" ||
	    ucto_class == "E-MAIL" ){
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
  FoliaElement *pos = word->addPosAnnotation( args );
  vector<string> tagParts;
  size_t numParts = Timbl::split_at( tagPartS, tagParts, "," );
  for ( size_t i=0; i < numParts; ++i ){
    string arg = "set='mbt-pos', subset='" + getSubSet( tagParts[i], mainTag ) 
      + "', cls='" + tagParts[i] + "', annotator='MBT'";
    FoliaElement *feat = new folia::Feature( arg );
    pos->append( feat );
  }
}

string MBTagger::Classify( FoliaElement *sent ){
  string tagged;
  vector<Word*> swords = sent->words();
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
    vector<string> tags;
    int num_words = splitWT( tagged, tags, conf );
    for ( int i = 0; i < num_words; ++i ) {
      addTag( swords[i], tags[i], conf[i] );
    }
  }
  return tagged;
}


