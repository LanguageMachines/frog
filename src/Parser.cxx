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

#include "frog/Parser.h"

#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "config.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/PrettyPrint.h"
#include "timbl/TimblAPI.h"
#include "frog/Frog-util.h"
#include "frog/csidp.h"

using namespace std;

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
  vector<vector<folia::Word*> > mwus;
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
  string val = configuration.lookUp( "debug", "parser" );
  if ( !val.empty() ){
    int level;
    if ( TiCC::stringTo<int>( val, level ) ){
      if ( level > 5 ){
	parseLog->setlevel( LogLevel::LogDebug );
      }
    }
  }
  val = configuration.lookUp( "version", "parser" );
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
    filter = new TiCC::UniFilter();
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

  string cls = configuration.lookUp( "outputclass" );
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

static vector<folia::Word *> lookup( folia::Word *word,
				     const vector<folia::Entity*>& entities ){
  vector<folia::Word*> vec;
  for ( const auto& ent : entities ){
    vec = ent->select<folia::Word>();
    if ( !vec.empty() ){
      if ( vec[0]->id() == word->id() ) {
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
      string word_1, word_2;
      string tag_1, tag_2;
      string mod_1;
      if ( i == 0 ){
	word_2 = "__";
	tag_2 = "__";
	//	mod_2 = "__";
	word_1 = "__";
	tag_1 = "__";
	mod_1 = "__";
      }
      else if ( i == 1 ){
	word_2 = "__";
	tag_2 = "__";
	//	mod_2 = "__";
	word_1 = words[i-1];
	tag_1 = heads[i-1];
	mod_1 = mods[i-1];
      }
      else {
	word_2 = words[i-2];
	tag_2 = heads[i-2];
	//	mod_2 = mods[i-2];
	word_1 = words[i-1];
	tag_1 = heads[i-1];
	mod_1 = mods[i-1];
      }
      string word0 = words[i];
      string word1, word2;
      string tag0 = heads[i];
      string tag1, tag2;
      string mod0 = mods[i];
      string mod1;
      if ( i < words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	mod1 = mods[i+1];
	word2 = words[i+2];
	tag2 = heads[i+2];
	//	mod2 = mods[i+2];
      }
      else if ( i == words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	mod1 = mods[i+1];
	word2 = "__";
	tag2 = "__";
	//	mod2 = "__";
      }
      else {
	word1 = "__";
	tag1 = "__";
	mod1 = "__";
	word2 = "__";
	tag2 = "__";
	//	mod2 = "__";
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
      string word_1, word_2;
      string tag_1, tag_2;
      if ( i == 0 ){
	word_2 = "__";
	tag_2 = "__";
	word_1 = "__";
	tag_1 = "__";
      }
      else if ( i == 1 ){
	word_2 = "__";
	tag_2 = "__";
	word_1 = words[i-1];
	tag_1 = heads[i-1];
      }
      else {
	word_2 = words[i-2];
	tag_2 = heads[i-2];
	word_1 = words[i-1];
	tag_1 = heads[i-1];
      }
      string word0 = words[i];
      string word1, word2;
      string tag0 = heads[i];
      string tag1, tag2;
      string mod0 = mods[i];
      if ( i < words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	word2 = words[i+2];
	tag2 = heads[i+2];
      }
      else if ( i == words.size() - 2 ){
	word1 = words[i+1];
	tag1 = heads[i+1];
	word2 = "__";
	tag2 = "__";
      }
      else {
	word1 = "__";
	tag1 = "__";
	word2 = "__";
	tag2 = "__";
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


void Parser::addDeclaration( folia::Document& doc ) const {
  doc.declare( folia::AnnotationType::DEPENDENCY, dep_tagset,
	       "annotator='frog-depparse-" + version
	       + "', annotatortype='auto'");
}

void Parser::addDeclaration( folia::Processor& proc ) const {
  proc.declare( folia::AnnotationType::DEPENDENCY, dep_tagset,
	       "annotator='frog-depparse-" + version
	       + "', annotatortype='auto'");
}

parseData Parser::prepareParse( const vector<folia::Word *>& fwords ){
  parseData pd;
  folia::Sentence *sent = 0;
  vector<folia::Entity*> entities;
#pragma omp critical (foliaupdate)
  {
    sent = fwords[0]->sentence();
    entities = sent->select<folia::Entity>(MWU_tagset);
  }
  for ( size_t i=0; i < fwords.size(); ++i ){
    folia::Word *word = fwords[i];
    vector<folia::Word*> mwuv = lookup( word, entities );
    if ( !mwuv.empty() ){
      string multi_word;
      string head;
      string mod;
      for ( const auto& mwu : mwuv ){
	icu::UnicodeString tmp;
#pragma omp critical (foliaupdate)
	{
	  tmp = mwu->text( textclass );
	}
	if ( filter )
	  tmp = filter->filter( tmp );
	string ms = TiCC::UnicodeToUTF8( tmp );
	// the word may contain spaces, remove them all!
	ms.erase(remove_if(ms.begin(), ms.end(), ::isspace), ms.end());
	multi_word += ms;
	folia::PosAnnotation *postag
	  = mwu->annotation<folia::PosAnnotation>( POS_tagset );
	head += postag->feat("head");
	vector<folia::Feature*> feats = postag->select<folia::Feature>();
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
      icu::UnicodeString tmp;
#pragma omp critical (foliaupdate)
      {
	tmp = word->text( textclass );
      }
      if ( filter )
	tmp = filter->filter( tmp );
      string ms = TiCC::UnicodeToUTF8( tmp );
      // the word may contain spaces, remove them all!
      ms.erase(remove_if(ms.begin(), ms.end(), ::isspace), ms.end());
      pd.words.push_back( ms );
      folia::PosAnnotation *postag
	= word->annotation<folia::PosAnnotation>( POS_tagset );
      string head = postag->feat("head");
      pd.heads.push_back( head );
      string mod;
      vector<folia::Feature*> feats = postag->select<folia::Feature>();
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
      vector<folia::Word*> vec;
      vec.push_back(word);
      pd.mwus.push_back( vec );
    }
  }
  return pd;
}


void extract( const string& tv, string& head, string& mods ){
  vector<string> v = TiCC::split_at_first_of( tv, "()" );
  head = v[0];
  mods.clear();
  if ( v.size() > 1 ){
    vector<string> mv = TiCC::split_at( v[1], "," );
    mods = mv[0];
    for ( size_t i=1; i < mv.size(); ++i ){
      mods += "|" + mv[i];
    }
  }
  else {
    mods = ""; // HACK should be "__", but then there are differences!
  }                                                      //     ^
}                                                        //     |
                                                         //     |
parseData Parser::prepareParse( frog_data& fd ){         //     |
  parseData pd;                                          //     |
  for ( size_t i = 0; i < fd.units.size(); ++i ){        //     |
    if ( fd.mwus.find( i ) == fd.mwus.end() ){           //     |
      string head;                                       //     |
      string mods;                                       //     |
      extract( fd.units[i].tag, head, mods );            //     |
      pd.words.push_back( fd.units[i].word );            //     |
      pd.heads.push_back( head );                        //     |
      if ( mods.empty() ){                               //    \/
	// HACK: make this bug-to-bug compatible with older versions.
	// But in fact this should also be done for the mwu's loop below!
	// now sometimes empty mods get appended there.
	// extract should return "__", I suppose  (see above)
	mods = "__";
      }
      pd.mods.push_back( mods );
    }
    else {
      string multi_word;
      string multi_head;
      string multi_mods;
      for ( size_t k = i; k <= fd.mwus[i]; ++k ){
	icu::UnicodeString tmp = TiCC::UnicodeFromUTF8( fd.units[k].word );
	if ( filter )
	  tmp = filter->filter( tmp );
	string ms = TiCC::UnicodeToUTF8( tmp );
	// the word may contain spaces, remove them all!
	ms.erase(remove_if(ms.begin(), ms.end(), ::isspace), ms.end());
	string head;
	string mods;
	extract( fd.units[k].tag, head, mods );
	if ( k == i ){
	  multi_word = ms;
	  multi_head = head;
	  multi_mods = mods;
	}
	else {
	  multi_word += "_" + ms;
	  multi_head += "_" + head;
	  multi_mods += "_" + mods;
	}
      }
      pd.words.push_back( multi_word );
      pd.heads.push_back( multi_head );
      pd.mods.push_back( multi_mods );
      i = fd.mwus[i];
    }
  }
  return pd;
}

void appendResult( const vector<folia::Word *>& words,
		   parseData& pd,
		   const string& tagset,
		   const string& textclass,
		   const vector<int>& nums,
		   const vector<string>& roles ){
  folia::Sentence *sent = 0;
#pragma omp critical (foliaupdate)
  {
    sent = words[0]->sentence();
  }
  folia::DependenciesLayer *dl = 0;
  folia::KWargs args;
  string s_id = sent->id();
  if ( !s_id.empty() ){
    args["generate_id"] = s_id;
  }
  args["set"] = tagset;
#pragma omp critical (foliaupdate)
  {
    dl = new folia::DependenciesLayer( args, sent->doc() );
    sent->append( dl );
  }
  string dl_id = dl->id();
  for ( size_t i=0; i < nums.size(); ++i ){
    if ( nums[i] != 0 ){
      folia::KWargs args;
      if ( !dl_id.empty() ){
	args["generate_id"] = dl_id;
      }
      args["class"] = roles[i];
      args["set"] = tagset;
      if ( textclass != "current" ){
	args["textclass"] = textclass;
      }
#pragma omp critical (foliaupdate)
      {
	folia::Dependency *d = new folia::Dependency( args, sent->doc() );
	dl->append( d );
	folia::Headspan *dh = new folia::Headspan();
	for ( const auto& wrd : pd.mwus[nums[i]-1] ){
	  dh->append( wrd );
	}
	d->append( dh );
	folia::DependencyDependent *dd = new folia::DependencyDependent();
	for ( const auto& it : pd.mwus[i] ){
	  dd->append( it );
	}
	d->append( dd );
      }
    }
  }
}

void appendParseResult( const vector<folia::Word *>& words,
			parseData& pd,
			const string& tagset,
			const string& textclass,
			const vector<parsrel>& res ){
  vector<int> nums;
  vector<string> roles;
  for ( const auto& it : res ){
    nums.push_back( it.head );
    roles.push_back( it.deprel );
  }
  appendResult( words, pd, tagset, textclass, nums, roles );
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

void Parser::Parse( const vector<folia::Word*>& words,
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
			       maxDepSpan,
			       parseLog );
  timers.csiTimer.stop();
  appendParseResult( words, pd, dep_tagset, textclass, res );
  timers.parseTimer.stop();
}

void appendResult( frog_data& fd,
		   const vector<int>& nums,
		   const vector<string>& roles ){
  for ( size_t i = 0; i < nums.size(); ++i ){
    fd.mw_units[i].parse_index = nums[i];
    fd.mw_units[i].parse_role = roles[i];
  }
}

void appendParseResult( frog_data& fd,
			const vector<parsrel>& res ){
  vector<int> nums;
  vector<string> roles;
  for ( const auto& it : res ){
    nums.push_back( it.head );
    roles.push_back( it.deprel );
  }
  appendResult( fd, nums, roles );
}

void Parser::Parse( frog_data& fd, TimerBlock& timers ){
  timers.parseTimer.start();
  if ( !isInit ){
    LOG << "Parser is not initialized! EXIT!" << endl;
    throw runtime_error( "Parser is not initialized!" );
  }
  if ( fd.empty() ){
    LOG << "unable to parse an analisis without words" << endl;
    return;
  }
  timers.prepareTimer.start();
  parseData pd = prepareParse( fd );
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
			       maxDepSpan,
			       parseLog );
  timers.csiTimer.stop();
  appendParseResult( fd, res );
  timers.parseTimer.stop();
}

void Parser::add_result( folia::Sentence *s,
			 const frog_data& fd,
			 const vector<folia::Word*>& wv ) const {
  folia::KWargs args;
  args["generate_id"] = s->id();
  args["set"] = getTagset();
  folia::DependenciesLayer *el = new folia::DependenciesLayer( args, s->doc() );
  s->append( el );
  for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
    string cls = fd.mw_units[pos].parse_role;
    if ( cls != "ROOT" ){
      args["generate_id"] = el->id();
      args["class"] = cls;
      folia::Dependency *e = new folia::Dependency( args, s->doc() );
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

void Parser::add_result( const frog_data& fd,
			 const vector<folia::Word*>& wv ) const {
  folia::Sentence *s = wv[0]->sentence();
  folia::KWargs args;
  args["generate_id"] = s->id();
  args["set"] = getTagset();
  folia::DependenciesLayer *el = new folia::DependenciesLayer( args, s->doc() );
  s->append( el );
  for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
    string cls = fd.mw_units[pos].parse_role;
    if ( cls != "ROOT" ){
      args["generate_id"] = el->id();
      args["class"] = cls;
      folia::Dependency *e = new folia::Dependency( args, s->doc() );
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
