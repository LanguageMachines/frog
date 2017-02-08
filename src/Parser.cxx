/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2017
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

#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"
#include "ticcutils/Configuration.h"
#include "timbl/TimblAPI.h"
#include "ucto/unicode.h"
#include "frog/Frog.h"
#include "frog/Parser.h"
#include "frog/csidp.h"

using namespace std;
using namespace folia;

using TiCC::operator<<;

#define LOG *TiCC::Log(parseLog)

TiCC::Timer prepareTimer;
TiCC::Timer relsTimer;
TiCC::Timer pairsTimer;
TiCC::Timer dirTimer;
TiCC::Timer csiTimer;

struct parseData {
  void clear() { words.clear(); heads.clear(); mods.clear(); mwus.clear(); }
  vector<string> words;
  vector<string> heads;
  vector<string> mods;
  vector<vector<Word*> > mwus;
};

ostream& operator<<( ostream& os, const parseData& pd ){
  os << "pd words " << pd.words << endl;
  os << "pd heads " << pd.heads << endl;
  os << "pd mods " << pd.mods << endl;
  os << "pd mwus ";
  for ( const auto& mw : pd.mwus ){
    for ( const auto& w : mw ){
      os << w->id();
    }
    os << endl;
  }
  return os;
}

bool Parser::init( const TiCC::Configuration& configuration ){
  filter = 0;
  string pairsFileName;
  string pairsOptions = "-a1 +D -G0 +vdb+di";
  string dirFileName;
  string dirOptions = "-a1 +D -G0 +vdb+di";
  string relsFileName;
  string relsOptions = "-a1 +D -G0 +vdb+di";
  maxDepSpanS = "20";
  maxDepSpan = 20;
  bool problem = false;
  LOG << "initiating parser ... " << endl;
  string cDir = configuration.configDir();
  string val = configuration.lookUp( "version", "parser" );
  if ( val.empty() ){
    version = "1.0";
  }
  else {
    version = val;
  }
  val = configuration.lookUp( "set", "parser" );
  if ( val.empty() ){
    dep_tagset = "http://ilk.uvt.nl/folia/sets/frog-depparse-nl";
  }
  else {
    dep_tagset = val;
  }
  val = configuration.lookUp( "set", "tagger" );
  if ( val.empty() ){
    POS_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
  }
  else {
    POS_tagset = val;
  }
  val = configuration.lookUp( "set", "mwu" );
  if ( val.empty() ){
    MWU_tagset = "http://ilk.uvt.nl/folia/sets/frog-mwu-nl";
  }
  else {
    MWU_tagset = val;
  }
  string charFile = configuration.lookUp( "char_filter_file", "tagger" );
  if ( charFile.empty() )
    charFile = configuration.lookUp( "char_filter_file" );
  if ( !charFile.empty() ){
    charFile = prefix( configuration.configDir(), charFile );
    filter = new Tokenizer::UnicodeFilter();
    filter->fill( charFile );
  }
  val = configuration.lookUp( "maxDepSpan", "parser" );
  if ( !val.empty() ){
    size_t gs = TiCC::stringTo<size_t>( val );
    if ( gs < 50 ){
      maxDepSpanS = val;
      maxDepSpan = gs;
    }
    else {
      LOG << "invalid maxDepSpan value in config file" << endl;
      LOG << "keeping default " << maxDepSpan << endl;
      problem = true;
    }
  }
  val = configuration.lookUp( "pairsFile", "parser" );
  if ( !val.empty() ){
    pairsFileName = prefix( cDir, val );
  }
  else {
    LOG << "missing pairsFile option" << endl;
    problem = true;
  }
  val = configuration.lookUp( "pairsOptions", "parser" );
  if ( !val.empty() ){
    pairsOptions = val;
  }
  val = configuration.lookUp( "dirFile", "parser" );
  if ( !val.empty() ){
    dirFileName = prefix( cDir, val );
  }
  else {
    LOG << "missing dirFile option" << endl;
    problem = true;
  }
  val = configuration.lookUp( "dirOptions", "parser" );
  if ( !val.empty() ){
    dirOptions = val;
  }
  val = configuration.lookUp( "relsFile", "parser" );
  if ( !val.empty() ){
    relsFileName = prefix( cDir, val );
  }
  else {
    LOG << "missing relsFile option" << endl;
    problem = true;
  }
  val = configuration.lookUp( "relsOptions", "parser" );
  if ( !val.empty() ){
    relsOptions = val;
  }
  if ( problem ) {
    return false;
  }

  string cls = configuration.lookUp( "textclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }
  bool happy = true;
  pairs = new Timbl::TimblAPI( pairsOptions );
  if ( pairs->Valid() ){
    LOG << "reading " <<  pairsFileName << endl;
    happy = pairs->GetInstanceBase( pairsFileName );
  }
  else {
    LOG << "creating Timbl for pairs failed:"
		   << pairsOptions << endl;
    happy = false;
  }
  if ( happy ){
    dir = new Timbl::TimblAPI( dirOptions );
    if ( dir->Valid() ){
      LOG << "reading " <<  dirFileName << endl;
      happy = dir->GetInstanceBase( dirFileName );
    }
    else {
      LOG << "creating Timbl for dir failed:" << dirOptions << endl;
      happy = false;
    }
    if ( happy ){
      rels = new Timbl::TimblAPI( relsOptions );
      if ( rels->Valid() ){
	LOG << "reading " <<  relsFileName << endl;
	happy = rels->GetInstanceBase( relsFileName );
      }
      else {
	LOG << "creating Timbl for rels failed:"
		       << relsOptions << endl;
	happy = false;
      }
    }
  }
  isInit = happy;
  return happy;
}

Parser::~Parser(){
  delete rels;
  delete dir;
  delete pairs;
  delete parseLog;
  delete filter;
}

static vector<Word *> lookup( Word *word,
			      const vector<Entity*>& entities ){
  vector<Word*> vec;
  for ( const auto& ent : entities ){
    vec = ent->select<Word>();
    if ( !vec.empty() ){
      if ( vec[0]->id() == word->id() ) {
	// cerr << "found " << vec << endl;
	return vec;
      }
    }
  }
  vec.clear();
  return vec;
}

vector<string> Parser::createPairInstances( const parseData& pd ){
  vector<string> instances;
  const vector<string>& words = pd.words;
  const vector<string>& heads = pd.heads;
  const vector<string>& mods = pd.mods;
  if ( words.size() == 1 ){
    string inst =
      "__ " + words[0] + " __ ROOT ROOT ROOT __ " + heads[0]
      + " __ ROOT ROOT ROOT "+ words[0] +"^ROOT ROOT ROOT ROOT^"
      + heads[0] + " _";
    instances.push_back( inst );
  }
  else {
    for ( size_t i=0 ; i < words.size(); ++i ){
      string word_1, word0, word1;
      string tag_1, tag0, tag1;
      string mods0;
      if ( i == 0 ){
	word_1 = "__";
	tag_1 = "__";
      }
      else {
	word_1 = words[i-1];
	tag_1 = heads[i-1];
      }
      word0 = words[i];
      tag0 = heads[i];
      mods0 = mods[i];
      if ( i == words.size() - 1 ){
	word1 = "__";
	tag1 = "__";
      }
      else {
	word1 = words[i+1];
	tag1 = heads[i+1];
      }
      string inst = word_1 + " " + word0 + " " + word1
	+ " ROOT ROOT ROOT " + tag_1 + " "
	+ tag0 + " " + tag1 + " ROOT ROOT ROOT " + tag0
	+ "^ROOT ROOT ROOT ROOT^" + mods0 + " _";
      instances.push_back( inst );
    }
    //
    for ( size_t wPos=0; wPos < words.size(); ++wPos ){
      string w_word_1, w_word0, w_word1;
      string w_tag_1, w_tag0, w_tag1;
      string w_mods0;
      if ( wPos == 0 ){
	w_word_1 = "__";
	w_tag_1 = "__";
      }
      else {
	w_word_1 = words[wPos-1];
	w_tag_1 = heads[wPos-1];
      }
      w_word0 = words[wPos];
      w_tag0 = heads[wPos];
      w_mods0 = mods[wPos];
      if ( wPos == words.size()-1 ){
	w_word1 = "__";
	w_tag1 = "__";
      }
      else {
	w_word1 = words[wPos+1];
	w_tag1 = heads[wPos+1];
      }
      for ( size_t pos=0; pos < words.size(); ++pos ){
	if ( pos > wPos + maxDepSpan ){
	  break;
	}
	if ( pos == wPos ){
	  continue;
	}
	if ( pos + maxDepSpan < wPos ){
	  continue;
	}
	string inst = w_word_1 + " " + w_word0 + " " + w_word1;

	if ( pos == 0 ){
	  inst += " __";
	}
	else {
	  inst += " " + words[pos-1];
	}
	if ( pos < words.size() ){
	  inst += " " + words[pos];
	}
	else {
	  inst += " __";
	}
	if ( pos < words.size()-1 ){
	  inst += " " + words[pos+1];
	}
	else {
	  inst += " __";
	}
	inst += " " + w_tag_1 + " " + w_tag0 + " " + w_tag1;
	if ( pos == 0 ){
	  inst += " __";
	}
	else {
	  inst += " " + heads[pos-1];
	}
	if ( pos < words.size() ){
	  inst += " " + heads[pos];
	}
	else {
	  inst += " __";
	}
	if ( pos < words.size()-1 ){
	  inst += " " + heads[pos+1];
	}
	else {
	  inst += " __";
	}

	inst += " " + w_tag0 + "^";
	if ( pos < words.size() ){
	  inst += heads[pos];
	}
	else {
	  inst += "__";
	}

	if ( wPos > pos ){
	  inst += " LEFT " + TiCC::toString( wPos - pos );
	}
	else {
	  inst += " RIGHT "+ TiCC::toString( pos - wPos );
	}
	if ( pos >= words.size() ){
	  inst += " __";
	}
	else {
	  inst += " " + mods[pos];
	}
	inst += "^" + w_mods0 + " __";
	instances.push_back( inst );
      }
    }
  }
  return instances;
}

vector<string> Parser::createDirInstances( const parseData& pd ){
  vector<string> d_instances;
  const vector<string>& words = pd.words;
  const vector<string>& heads = pd.heads;
  const vector<string>& mods = pd.mods;

  if ( words.size() == 1 ){
    string word0 = words[0];
    string tag0 = heads[0];
    string mod0 = mods[0];
    string inst = "__ __ " + word0 + " __ __ __ __ " + tag0
      + " __ __ __ __ " + word0 + "^" + tag0
      + " __ __ __^" + tag0 + " " + tag0 +"^__ __ " + mod0
      + " __ ROOT";
    d_instances.push_back( inst );
  }
  else if ( words.size() == 2 ){
    string word0 = words[0];
    string tag0 = heads[0];
    string mod0 = mods[0];
    string word1 = words[1];
    string tag1 = heads[1];
    string mod1 = mods[1];
    string inst = string("__ __")
      + " " + word0
      + " " + word1
      + " __ __ __"
      + " " + tag0
      + " " + tag1
      + " __ __ __"
      + " " + word0 + "^" + tag0
      + " " + word1 + "^" + tag1
      + " __ __^" + tag0
      + " " + tag0 + "^" + tag1
      + " __"
      + " " + mod0
      + " " + mod1
      + " ROOT";
    d_instances.push_back( inst );
    inst = string("__")
      + " " + word0
      + " " + word1
      + " __ __ __"
      + " " + tag0
      + " " + tag1
      + " __ __ __"
      + " " + word0 + "^" + tag0
      + " " + word1 + "^" + tag1
      + " __ __"
      + " " + tag0 + "^" + tag1
      + " " + tag1 + "^__"
      + " " + mod0
      + " " + mod1
      + " __"
      + " ROOT";
    d_instances.push_back( inst );
  }
  else if ( words.size() == 3 ) {
    string word0 = words[0];
    string tag0 = heads[0];
    string mod0 = mods[0];
    string word1 = words[1];
    string tag1 = heads[1];
    string mod1 = mods[1];
    string word2 = words[2];
    string tag2 = heads[2];
    string mod2 = mods[2];
    string inst = string("__ __")
      + " " + word0
      + " " + word1
      + " " + word2
      + " __ __"
      + " " + tag0
      + " " + tag1
      + " " + tag2
      + " __ __"
      + " " + word0 + "^" + tag0
      + " " + word1 + "^" + tag1
      + " " + word2 + "^" + tag2
      + " __^" + tag0
      + " " + tag0 + "^" + tag1
      + " __"
      + " " + mod0
      + " " + mod1
      + " ROOT";
    d_instances.push_back( inst );
    inst = string("__")
      + " " + word0
      + " " + word1
      + " " + word2
      + " __ __"
      + " " + tag0
      + " " + tag1
      + " " + tag2
      + " __ __"
      + " " + word0 + "^" + tag0
      + " " + word1 + "^" + tag1
      + " " + word2 + "^" + tag2
      + " __"
      + " " + tag0 + "^" + tag1
      + " " + tag1 + "^" + tag2
      + " " + mod0
      + " " + mod1
      + " " + mod2
      + " ROOT";
    d_instances.push_back( inst );
    inst = word0
      + " " + word1
      + " " + word2
      + " __ __"
      + " " + tag0
      + " " + tag1
      + " " + tag2
      + " __ __"
      + " " + word0 + "^" + tag0
      + " " + word1 + "^" + tag1
      + " " + word2 + "^" + tag2
      + " __ __"
      + " " + tag1 + "^" + tag2
      + " " + tag2 + "^__"
      + " " + mod1
      + " " + mod2
      + " __"
      + " ROOT";
    d_instances.push_back( inst );
  }
  else {
    for ( size_t i=0 ; i < words.size(); ++i ){
      string word_0, word_1, word_2;
      string tag_0, tag_1, tag_2;
      string mod_0, mod_1, mod_2;
      if ( i == 0 ){
	word_2 = "__";
	tag_2 = "__";
	mod_2 = "__";
	word_1 = "__";
	tag_1 = "__";
	mod_1 = "__";
      }
      else if ( i == 1 ){
	word_2 = "__";
	tag_2 = "__";
	mod_2 = "__";
	word_1 = words[i-1];
	tag_1 = heads[i-1];
	mod_1 = mods[i-1];
      }
      else {
	word_2 = words[i-2];
	tag_2 = heads[i-2];
	mod_2 = mods[i-2];
	word_1 = words[i-1];
	tag_1 = heads[i-1];
	mod_1 = mods[i-1];
      }
      string word0 = words[i];
      string word1, word2;
      string tag0 = heads[i];
      string tag1, tag2;
      string mod0 = mods[i];
      string mod1, mod2;
      if ( i < words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	mod1 = mods[i+1];
	word2 = words[i+2];
	tag2 = heads[i+2];
	mod2 = mods[i+2];
      }
      else if ( i == words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	mod1 = mods[i+1];
	word2 = "__";
	tag2 = "__";
	mod2 = "__";
      }
      else {
	word1 = "__";
	tag1 = "__";
	mod1 = "__";
	word2 = "__";
	tag2 = "__";
	mod2 = "__";
      }
      string inst = word_2
	+ " " + word_1
	+ " " + word0
	+ " " + word1
	+ " " + word2
	+ " " + tag_2
	+ " " + tag_1
	+ " " + tag0
	+ " " + tag1
	+ " " + tag2
	+ " " + word_2 + "^" + tag_2
	+ " " + word_1 + "^" + tag_1
	+ " " + word0 + "^" + tag0
	+ " " + word1 + "^" + tag1
	+ " " + word2 + "^" + tag2
	+ " " + tag_1 + "^" + tag0
	+ " " + tag0 + "^" + tag1
	+ " " + mod_1
	+ " " + mod0
	+ " " + mod1
	+ " ROOT";
      d_instances.push_back( inst );
    }
  }
  return d_instances;
}

vector<string> Parser::createRelInstances( const parseData& pd ){
  vector<string> r_instances;
  const vector<string>& words = pd.words;
  const vector<string>& heads = pd.heads;
  const vector<string>& mods = pd.mods;

  if ( words.size() == 1 ){
    string word0 = words[0];
    string tag0 = heads[0];
    string mod0 = mods[0];
    string inst = "__ __ " + word0 + " __ __ " + mod0
      + " __ __ "  + tag0 + " __ __ __^" + tag0
      + " " + tag0 + "^__ __^__^" + tag0
      + " " + tag0 + "^__^__ __";
    r_instances.push_back( inst );
  }
  else if ( words.size() == 2 ){
    string word0 = words[0];
    string tag0 = heads[0];
    string mod0 = mods[0];
    string word1 = words[1];
    string tag1 = heads[1];
    string mod1 = mods[1];
    //
    string inst = string("__ __")
      + " " + word0
      + " " + word1
      + " __"
      + " " + mod0
      + " __ __"
      + " " + tag0
      + " " + tag1
      + " __"
      + " __^" + tag0
      + " " + tag0 + "^" + tag1
      + " __^__^" + tag0
      + " " + tag0 + "^" + tag1 + "^__"
      + " __";
    r_instances.push_back( inst );
    inst = string("__")
      + " " + word0
      + " " + word1
      + " __ __"
      + " " + mod1
      + " __"
      + " " + tag0
      + " " + tag1
      + " __ __"
      + " " + tag0 + "^" + tag1
      + " " + tag1 + "^__"
      + " __^" + tag0 + "^" + tag1
      + " " + tag1 + "^__^__"
      + " __";
    r_instances.push_back( inst );
  }
  else if ( words.size() == 3 ) {
    string word0 = words[0];
    string tag0 = heads[0];
    string mod0 = mods[0];
    string word1 = words[1];
    string tag1 = heads[1];
    string mod1 = mods[1];
    string word2 = words[2];
    string tag2 = heads[2];
    string mod2 = mods[2];
    //
    string inst = string("__ __")
      + " " + word0
      + " " + word1
      + " " + word2
      + " " + mod0
      + " __ __"
      + " " + tag0
      + " " + tag1
      + " " + tag2
      + " __^" + tag0
      + " " + tag0 + "^" + tag1
      + " __^__^" + tag0
      + " " + tag0 + "^" + tag1 + "^" + tag2
      + " __";
    r_instances.push_back( inst );
    inst = string("__")
      + " " + word0
      + " " + word1
      + " " + word2
      + " __"
      + " " + mod1
      + " __"
      + " " + tag0
      + " " + tag1
      + " " + tag2
      + " __"
      + " " + tag0 + "^" + tag1
      + " " + tag1 + "^" + tag2
      + " __^" + tag0 + "^" + tag1
      + " " + tag1 + "^" + tag2 + "^__"
      + " __";
    r_instances.push_back( inst );
    inst = word0
      + " " + word1
      + " " + word2
      + " __ __"
      + " " + mod2
      + " " + tag0
      + " " + tag1
      + " " + tag2
      + " __ __"
      + " " + tag1 + "^" + tag2
      + " " + tag2 + "^__"
      + " " + tag0 + "^" + tag1 + "^" + tag2
      + " " + tag2 + "^__^__"
      + " __";
    r_instances.push_back( inst );
  }
  else {
    for ( size_t i=0 ; i < words.size(); ++i ){
      string word_0, word_1, word_2;
      string tag_0, tag_1, tag_2;
      string mod_0, mod_1, mod_2;
      if ( i == 0 ){
	word_2 = "__";
	tag_2 = "__";
	mod_2 = "__";
	word_1 = "__";
	tag_1 = "__";
	mod_1 = "__";
      }
      else if ( i == 1 ){
	word_2 = "__";
	tag_2 = "__";
	mod_2 = "__";
	word_1 = words[i-1];
	tag_1 = heads[i-1];
	mod_1 = mods[i-1];
      }
      else {
	word_2 = words[i-2];
	tag_2 = heads[i-2];
	mod_2 = mods[i-2];
	word_1 = words[i-1];
	tag_1 = heads[i-1];
	mod_1 = mods[i-1];
      }
      string word0 = words[i];
      string word1, word2;
      string tag0 = heads[i];
      string tag1, tag2;
      string mod0 = mods[i];
      string mod1, mod2;
      if ( i < words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	mod1 = mods[i+1];
	word2 = words[i+2];
	tag2 = heads[i+2];
	mod2 = mods[i+2];
      }
      else if ( i == words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	mod1 = mods[i+1];
	word2 = "__";
	tag2 = "__";
	mod2 = "__";
      }
      else {
	word1 = "__";
	tag1 = "__";
	mod1 = "__";
	word2 = "__";
	tag2 = "__";
	mod2 = "__";
      }
      //
      string inst = word_2
	+ " " + word_1
	+ " " + word0
	+ " " + word1
	+ " " + word2
	+ " " + mod0
	+ " " + tag_2
	+ " " + tag_1
	+ " " + tag0
	+ " " + tag1
	+ " " + tag2
	+ " " + tag_1 + "^" + tag0
	+ " " + tag0 + "^" + tag1
	+ " " + tag_2 + "^" + tag_1 + "^" + tag0
	+ " " + tag0 + "^" + tag1 + "^" + tag2
	+ " __";
      r_instances.push_back( inst );
    }
  }
  return r_instances;
}


void Parser::addDeclaration( Document& doc ) const {
#pragma omp critical(foliaupdate)
  {
    doc.declare( AnnotationType::DEPENDENCY, dep_tagset,
		 "annotator='frog-depparse-" + version
		 + "', annotatortype='auto'");
  }
}

parseData Parser::prepareParse( const vector<Word *>& fwords ){
  parseData pd;
  Sentence *sent = 0;
  vector<Entity*> entities;
#pragma omp critical(foliaupdate)
  {
    sent = fwords[0]->sentence();
    entities = sent->select<Entity>(MWU_tagset);
  }
  for ( size_t i=0; i < fwords.size(); ++i ){
    Word *word = fwords[i];
    vector<Word*> mwuv = lookup( word, entities );
    if ( !mwuv.empty() ){
      string multi_word;
      string head;
      string mod;
      for ( const auto& mwu : mwuv ){
	UnicodeString tmp;
#pragma omp critical(foliaupdate)
	{
	  tmp = mwu->text( textclass );
	}
	if ( filter )
	  tmp = filter->filter( tmp );
	string ms = UnicodeToUTF8( tmp );
	multi_word += ms;
	PosAnnotation *postag = mwu->annotation<PosAnnotation>( POS_tagset );
	head += postag->feat("head");
	vector<Feature*> feats = postag->select<Feature>();
	for ( const auto& feat : feats ){
	  mod += feat->cls();
	  if ( &feat != &feats.back() ){
	    mod += "|";
	  }
	}
	if ( &mwu != &mwuv.back() ){
	  multi_word += "_";
	  head += "_";
	  mod += "_";
	}
      }
      pd.words.push_back( multi_word );
      pd.heads.push_back( head );
      pd.mods.push_back( mod );
      pd.mwus.push_back( mwuv );
      i += mwuv.size()-1;
    }
    else {
      UnicodeString tmp;
#pragma omp critical(foliaupdate)
      {
	tmp = word->text( textclass );
      }
      if ( filter )
	tmp = filter->filter( tmp );
      string ms = UnicodeToUTF8( tmp );
      pd.words.push_back( ms );
      PosAnnotation *postag = word->annotation<PosAnnotation>( POS_tagset );
      string head = postag->feat("head");
      pd.heads.push_back( head );
      string mod;
      vector<Feature*> feats = postag->select<Feature>();
      if ( feats.empty() ){
	mod = "__";
      }
      else {
	for ( const auto& feat : feats ){
	  mod += feat->cls();
	  if ( &feat != &feats.back() ){
	    mod += "|";
	  }
	}
      }
      pd.mods.push_back( mod );
      vector<Word*> vec;
      vec.push_back(word);
      pd.mwus.push_back( vec );
    }
  }
  return pd;
}

void appendResult( const vector<Word *>& words,
		   parseData& pd,
		   const string& tagset,
		   const vector<int>& nums,
		   const vector<string>& roles ){
  Sentence *sent = words[0]->sentence();
  KWargs args;
  args["generate_id"] = sent->id();
  args["set"] = tagset;
  DependenciesLayer *dl = new DependenciesLayer( args, sent->doc() );
#pragma omp critical(foliaupdate)
  {
    sent->append( dl );
  }
  for ( size_t i=0; i < nums.size(); ++i ){
    if ( nums[i] != 0 ){
      KWargs args;
      args["generate_id"] = dl->id();
      args["class"] = roles[i];
      args["set"] = tagset;
#pragma omp critical(foliaupdate)
      {
	Dependency *d = new Dependency( args, sent->doc() );
	dl->append( d );
	Headspan *dh = new Headspan();
	for ( const auto& wrd : pd.mwus[nums[i]-1] ){
	  dh->append( wrd );
	}
	d->append( dh );
	DependencyDependent *dd = new DependencyDependent();
	for ( const auto& it : pd.mwus[i] ){
	  dd->append( it );
	}
	d->append( dd );
      }
    }
  }
}

void appendParseResult( const vector<Word *>& words,
			parseData& pd,
			const string& tagset,
			const vector<parsrel>& res ){
  vector<int> nums;
  vector<string> roles;
  for ( const auto& it : res ){
    nums.push_back( it.head );
    roles.push_back( it.deprel );
  }
  appendResult( words, pd, tagset, nums, roles );
}

void timbl( Timbl::TimblAPI* tim,
	    const vector<string>& instances,
	    vector<timbl_result>& results ){
  results.clear();
  for ( const auto& inst : instances ){
    const Timbl::ValueDistribution *db;
    const Timbl::TargetValue *tv = tim->Classify( inst, db );
    results.push_back( timbl_result( tv->Name(), db->Confidence(tv), db ) );
  }
}

void Parser::Parse( const vector<Word*>& words,
		    TimerBlock& timers ){
  timers.parseTimer.start();
  if ( !isInit ){
    LOG << "Parser is not initialized! EXIT!" << endl;
    throw runtime_error( "Parser is not initialized!" );
  }
  if ( words.empty() ){
    LOG << "unable to parse an analisis without words" << endl;
    return;
  }
  timers.prepareTimer.start();
  parseData pd = prepareParse( words );
  timers.prepareTimer.stop();
  vector<timbl_result> p_results;
  vector<timbl_result> d_results;
  vector<timbl_result> r_results;
#pragma omp parallel sections
  {
#pragma omp section
      {
	timers.pairsTimer.start();
	vector<string> instances = createPairInstances( pd );
	timbl( pairs, instances, p_results );
	timers.pairsTimer.stop();
      }
#pragma omp section
      {
	timers.dirTimer.start();
	vector<string> instances = createDirInstances( pd );
	timbl( dir, instances, d_results );
	timers.dirTimer.stop();
      }
#pragma omp section
      {
	timers.relsTimer.start();
	vector<string> instances = createRelInstances( pd );
	timbl( rels, instances, r_results );
	timers.relsTimer.stop();
      }
  }

  timers.csiTimer.start();
  vector<parsrel> res = parse( p_results,
			       r_results,
			       d_results,
			       pd.words.size(),
			       maxDepSpan );
  timers.csiTimer.stop();
  appendParseResult( words, pd, dep_tagset, res );
  timers.parseTimer.stop();
}
