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

#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "frog/Frog.h"
#include "frog/ner_tagger_mod.h"

using namespace std;
using namespace folia;

NERTagger::NERTagger(){
  tagger = 0;
  nerLog = new LogStream( theErrLog, "ner-" );
}

NERTagger::~NERTagger(){
  delete tagger;
  delete nerLog;
}
 
bool NERTagger::init( const Configuration& conf ){
  debug = tpDebug;
  string db = conf.lookUp( "debug", "NER" );
  if ( !db.empty() )
    debug = folia::stringTo<int>( db );
  switch ( debug ){
  case 0:
  case 1:
    nerLog->setlevel(LogNormal);
    break;
  case 2:
  case 3:
  case 4:
    nerLog->setlevel(LogDebug);
    break;
  case 5:
  case 6:
  case 7:
    nerLog->setlevel(LogHeavy);
    break;
  default:
    nerLog->setlevel(LogExtreme);
  }    
  if (debug) 
    *Log(nerLog) << "NER Tagger Init" << endl;
  if ( tagger != 0 ){
    *Log(nerLog) << "NER Tagger is already initialized!" << endl;
    return false;
  }  
  string tagset = conf.lookUp( "settings", "NER" );
  if ( tagset.empty() ){
    *Log(nerLog) << "Unable to find settings for NER" << endl;
    return false;
  }
  string init = "-s " + configuration.configDir() + tagset;
  if ( Tagger::Version() == "3.2.6" )
    init += " -vc";
  else
    init += " -vcf";
  tagger = new MbtAPI( init, *nerLog );
  return tagger != 0;
}

bool NERTagger::splitOneWT( const string& inp, string& word, 
			    string& tag, string& confidence ){
  bool isKnown = true;
  string in = inp;
  //     split word and tag, and store num of slashes
  if (debug)
    *Log(nerLog) << "split Classify starting with " << in << endl;
  string::size_type pos = in.rfind("/");
  if ( pos == string::npos ) {
    *Log(nerLog) << "no word/tag/confidence triple in this line: " << in << endl;
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
      *Log(nerLog) << "no word/tag/confidence triple in this line: " << in << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( debug){
    if ( isKnown )
      *Log(nerLog) << "known";
    else
      *Log(nerLog) << "unknown";
    *Log(nerLog) << " word: " << word << "\ttag: " << tag << "\tconfidence: " << confidence << endl;
  }
  return isKnown;
}

int NERTagger::splitWT( const string& tagged,
			vector<string>& tags, 
			vector<double>& conf ){
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
      *Log(nerLog) << "tagger confused. Expected a double, got '" << confs << "'" << endl;
      exit( EXIT_FAILURE );
    }
    words.push_back( word );
    tags.push_back( tag );
    known.push_back( isKnown );
    conf.push_back( confidence );
  }
  if (debug) {
    *Log(nerLog) << "#tagged_words: " << num_words << endl;
    for( size_t i = 0; i < num_words; i++) 
      *Log(nerLog)   << "\ttagged word[" << i <<"]: " << words[i] << (known[i]?"/":"//")
	     << tags[i] << " <" << conf[i] << ">" << endl;
  }
  return num_words;
}

static void addEntity( EntitiesLayer *entities, 
		       const vector<Word*>& words,
		       const vector<double>& confs,
		       const string& NER ){
  double c = 1;
  for ( size_t i=0; i < confs.size(); ++i )
    c *= confs[i];
  KWargs args;
  args["class"] = NER;
  args["confidence"] =  toString(c);
  args["set"] = "http://ilk.uvt.nl/folia/sets/frog-ner-nl";
  Entity *e = 0;
#pragma omp critical(foliaupdate)
  {
    e = new Entity(entities->doc(), args);
    entities->append( e );
  }
  for ( size_t i=0; i < words.size(); ++i ){
#pragma omp critical(foliaupdate)
    {
      e->append( words[i] );
    }
  }
}

void NERTagger::addNERTags( Sentence *sent, 
			    const vector<Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  EntitiesLayer *el = 0;
  try {
    el = sent->annotation<EntitiesLayer>();
  }
  catch(...){
#pragma omp critical(foliaupdate)
    {
      el = new EntitiesLayer();
      sent->append( el );
    }
  }
  vector<Word*> stack;
  vector<double> dstack;
  string curNER;
  for ( size_t i=0; i < tags.size(); ++i ){
    if (debug) 
      *Log(nerLog) << "NER = " << tags[i] << endl;
    vector<string> ner;
    if ( tags[i] == "O" ){
      if ( !stack.empty() ){
	if (debug) {
	  *Log(nerLog) << "O spit out " << curNER << endl;
	  using folia::operator<<;
	  *Log(nerLog) << "spit out " << stack << endl;	
	}
	addEntity( el, stack, dstack, curNER );
	dstack.clear();
	stack.clear();
      }
      continue;
    }
    else {
      size_t num_words = Timbl::split_at( tags[i], ner, "-" );
      if ( num_words != 2 ){
	*Log(nerLog) << "expected <NER>-tag, got: " << tags[i] << endl;
	exit( EXIT_FAILURE );
      }
    }
    if ( ner[0] == "B" ||
	 ( ner[0] == "I" && stack.empty() ) ||
	 ( ner[0] == "I" && ner[1] != curNER ) ){
      // an I without preceding B is handled as a B
      // an I with a different TAG is also handled as a B
      if ( !stack.empty() ){
	if ( debug ){
	  *Log(nerLog) << "B spit out " << curNER << endl;
	  using folia::operator<<;
	  *Log(nerLog) << "spit out " << stack << endl;	
	}
	addEntity( el, stack, dstack, curNER );
	dstack.clear();
	stack.clear();
      }
      curNER = ner[1];
    }
    dstack.push_back( confs[i] );
    stack.push_back( words[i] );
  }
  if ( !stack.empty() ){
    if ( debug ){
      *Log(nerLog) << "END spit out " << curNER << endl;
      using folia::operator<<;
      *Log(nerLog) << "spit out " << stack << endl;	
    }
    addEntity( el, stack, dstack, curNER );
  }
}

string NERTagger::Classify( Sentence *sent ){
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
    if (debug) 
      *Log(nerLog) << "NER in: " << sentence << endl;
    tagged = tagger->Tag(sentence);
    if (debug) {
      *Log(nerLog) << "sentence: " << sentence << endl
		   << "NER tagged: "<< tagged
		   << endl;
    }
    vector<double> conf;
    vector<string> tags;
    splitWT( tagged, tags, conf );
    addNERTags( sent, swords, tags, conf );
  }
  return tagged;
}


