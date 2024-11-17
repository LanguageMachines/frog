/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2024
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
#include <map>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "config.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/SocketBasics.h"
#include "ticcutils/json.hpp"
#include "timbl/TimblAPI.h"
#include "frog/Frog-util.h"
#include "frog/csidp.h"
#include "frog/Parser.h"

using namespace std;
using namespace icu;

using TiCC::operator<<;
using namespace nlohmann;

#define LOG *TiCC::Log(errLog)
#define DBG *TiCC::Dbg(dbgLog)

/// structure to store parsing results
struct parseData {
  void clear() { words.clear(); heads.clear(); mods.clear(); mwus.clear(); }
  vector<UnicodeString> words;
  vector<UnicodeString> heads;
  vector<UnicodeString> mods;
  vector<vector<folia::Word*> > mwus;
};

ostream& operator<<( ostream& os, const parseData& pd ){
  /// output a parseData structure (debugging only)
  /*!
    \param os the output stream
    \param pd the parseData to dump
    \return the stream
  */
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

UnicodeString filter_spaces( const UnicodeString& in ){
  // the word may contain spaces, remove them all!
  UnicodeString result;
  for ( int i=0; i < in.length(); ++i ){
    if ( u_isspace( in[i] ) ){
      continue;
    }
    result += in[i];
  }
  return result;
}

bool Parser::init( const TiCC::Configuration& configuration ){
  /// initialize a Parser from the configuration
  /*!
    \param configuration the config to use
    \return true on succes
    extract all needed information from the configuration and setup the parser
    by creating 3 Timbl instances
  */
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
	dbgLog->set_level( LogLevel::LogDebug );
      }
    }
  }
  val = configuration.lookUp( "version", "parser" );
  if ( val.empty() ){
    _version = "1.0";
  }
  else {
    _version = val;
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
  if ( charFile.empty() ){
    charFile = configuration.lookUp( "char_filter_file" );
  }
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

  val = configuration.lookUp( "host", "parser" );
  if ( !val.empty() ){
    // so using an external parser server
    _host = val;
    val = configuration.lookUp( "port", "parser" );
    if ( !val.empty() ){
      _port = val;
    }
    else {
      LOG << "missing port option" << endl;
      problem = true;
    }
    val = configuration.lookUp( "pairs_base", "parser" );
    if ( !val.empty() ){
      _pairs_base = val;
    }
    else {
      LOG << "missing pairs_base option" << endl;
      problem = true;
    }
    val = configuration.lookUp( "dirs_base", "parser" );
    if ( !val.empty() ){
      _dirs_base = val;
    }
    else {
      LOG << "missing dirs_base option" << endl;
      problem = true;
    }
    val = configuration.lookUp( "rels_base", "parser" );
    if ( !val.empty() ){
      _rels_base = val;
    }
    else {
      LOG << "missing rels_base option" << endl;
      problem = true;
    }
  }
  else {
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
  if ( _host.empty() ){
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
  }
  else {
    string mess = check_server( _host, _port, "DependencyParser" );
    if ( !mess.empty() ){
      LOG << "FAILED to find a server for the Dependency parser:" << endl;
      LOG << mess << endl;
      LOG << "timblserver not running??" << endl;
      happy = false;
    }
    else {
      LOG << "using Parser Timbl's on " << _host << ":" << _port << endl;
    }
  }
  isInit = happy;
  return happy;
}

Parser::~Parser(){
  /// destructor
  delete rels;
  delete dir;
  delete pairs;
}

vector<UnicodeString> Parser::createPairInstances( const parseData& pd ){
  /// create a list of Instances for the 'pairs' Timbl
  /*!
    \param pd the parsedata structure with words, heads and modifiers
    \return a list of string where every string is a Timbl test instance
  */
  vector<UnicodeString> instances;
  const vector<UnicodeString>& words = pd.words;
  const vector<UnicodeString>& heads = pd.heads;
  const vector<UnicodeString>& mods = pd.mods;
  if ( words.size() == 1 ){
    UnicodeString inst =
      "__ " + words[0] + " __ ROOT ROOT ROOT __ " + heads[0]
      + " __ ROOT ROOT ROOT "+ words[0] +"^ROOT ROOT ROOT ROOT^"
      + heads[0] + " _";
    instances.push_back( inst );
  }
  else {
    for ( size_t i=0 ; i < words.size(); ++i ){
      UnicodeString word_1, word0, word1;
      UnicodeString tag_1, tag0, tag1;
      UnicodeString mods0;
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
      UnicodeString inst = word_1 + " " + word0 + " " + word1
	+ " ROOT ROOT ROOT " + tag_1 + " "
	+ tag0 + " " + tag1 + " ROOT ROOT ROOT " + tag0
	+ "^ROOT ROOT ROOT ROOT^" + mods0 + " _";
      instances.push_back( inst );
    }
    //
    for ( size_t wPos=0; wPos < words.size(); ++wPos ){
      UnicodeString w_word_1, w_word0, w_word1;
      UnicodeString w_tag_1, w_tag0, w_tag1;
      UnicodeString w_mods0;
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
	UnicodeString inst = w_word_1 + " " + w_word0 + " " + w_word1;

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
	  inst += " LEFT " + TiCC::toUnicodeString( wPos - pos );
	}
	else {
	  inst += " RIGHT "+ TiCC::toUnicodeString( pos - wPos );
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

vector<UnicodeString> Parser::createDirInstances( const parseData& pd ){
  /// create a list of Instances for the 'dir' Timbl
  /*!
    \param pd the parsedata structure with words, heads and modifiers
    \return a list of string where every string is a Timbl test instance
  */
  vector<UnicodeString> d_instances;
  const vector<UnicodeString>& words = pd.words;
  const vector<UnicodeString>& heads = pd.heads;
  const vector<UnicodeString>& mods = pd.mods;

  if ( words.size() == 1 ){
    UnicodeString word0 = words[0];
    UnicodeString tag0 = heads[0];
    UnicodeString mod0 = mods[0];
    UnicodeString inst = "__ __ " + word0 + " __ __ __ __ " + tag0
      + " __ __ __ __ " + word0 + "^" + tag0
      + " __ __ __^" + tag0 + " " + tag0 +"^__ __ " + mod0
      + " __ ROOT";
    d_instances.push_back( inst );
  }
  else if ( words.size() == 2 ){
    UnicodeString word0 = words[0];
    UnicodeString tag0 = heads[0];
    UnicodeString mod0 = mods[0];
    UnicodeString word1 = words[1];
    UnicodeString tag1 = heads[1];
    UnicodeString mod1 = mods[1];
    UnicodeString inst = UnicodeString("__ __")
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
    inst = UnicodeString("__")
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
    UnicodeString word0 = words[0];
    UnicodeString tag0 = heads[0];
    UnicodeString mod0 = mods[0];
    UnicodeString word1 = words[1];
    UnicodeString tag1 = heads[1];
    UnicodeString mod1 = mods[1];
    UnicodeString word2 = words[2];
    UnicodeString tag2 = heads[2];
    UnicodeString mod2 = mods[2];
    UnicodeString inst = UnicodeString("__ __")
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
    inst = UnicodeString("__")
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
      UnicodeString word_1, word_2;
      UnicodeString tag_1, tag_2;
      UnicodeString mod_1;
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
      UnicodeString word0 = words[i];
      UnicodeString word1, word2;
      UnicodeString tag0 = heads[i];
      UnicodeString tag1, tag2;
      UnicodeString mod0 = mods[i];
      UnicodeString mod1;
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
      UnicodeString inst = word_2
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

vector<UnicodeString> Parser::createRelInstances( const parseData& pd ){
  /// create a list of Instances for the 'rel' Timbl
  /*!
    \param pd the parsedata structure with words, heads and modifiers
    \return a list of string where every string is a Timbl test instance
  */
  vector<UnicodeString> r_instances;
  const vector<UnicodeString>& words = pd.words;
  const vector<UnicodeString>& heads = pd.heads;
  const vector<UnicodeString>& mods = pd.mods;

  if ( words.size() == 1 ){
    UnicodeString word0 = words[0];
    UnicodeString tag0 = heads[0];
    UnicodeString mod0 = mods[0];
    UnicodeString inst = "__ __ " + word0 + " __ __ " + mod0
      + " __ __ "  + tag0 + " __ __ __^" + tag0
      + " " + tag0 + "^__ __^__^" + tag0
      + " " + tag0 + "^__^__ __";
    r_instances.push_back( inst );
  }
  else if ( words.size() == 2 ){
    UnicodeString word0 = words[0];
    UnicodeString tag0 = heads[0];
    UnicodeString mod0 = mods[0];
    UnicodeString word1 = words[1];
    UnicodeString tag1 = heads[1];
    UnicodeString mod1 = mods[1];
    //
    UnicodeString inst = UnicodeString("__ __")
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
    inst = UnicodeString("__")
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
    UnicodeString word0 = words[0];
    UnicodeString tag0 = heads[0];
    UnicodeString mod0 = mods[0];
    UnicodeString word1 = words[1];
    UnicodeString tag1 = heads[1];
    UnicodeString mod1 = mods[1];
    UnicodeString word2 = words[2];
    UnicodeString tag2 = heads[2];
    UnicodeString mod2 = mods[2];
    //
    UnicodeString inst = UnicodeString("__ __")
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
    inst = UnicodeString("__")
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
      UnicodeString word_1, word_2;
      UnicodeString tag_1, tag_2;
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
      UnicodeString word0 = words[i];
      UnicodeString word1, word2;
      UnicodeString tag0 = heads[i];
      UnicodeString tag1, tag2;
      UnicodeString mod0 = mods[i];
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
      UnicodeString inst = word_2
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

void Parser::add_provenance( folia::Document& doc, folia::processor *main ) const {
  /// add provenance information to the FoLiA document
  /*!
    \param doc the foLiA document we are working on
    \param main the main processor (presumably Frog) we want to add a new one to
  */
  string _label = "dep-parser";
  if ( !main ){
    throw logic_error( "Parser::add_provenance() without arguments." );
  }
  folia::KWargs args;
  args["name"] = _label;
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  args.clear();
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::DEPENDENCY, dep_tagset, args );
}

void extract( const UnicodeString& tv,
	      UnicodeString& head,
	      UnicodeString& mods ){
  /// spit a (CGN-like) tag into a head and a modifier part
  /*!
    \param tv a CGN-like tag
    \param head the extacted head
    \param mods the modfiers, concatenated using the '|' symbol

    Example: the tag WW(pv,tgw,met-t) will be split into a head 'WW' and a
    mods string 'pv|tgw|met-t'
   */
  vector<UnicodeString> v = TiCC::split_at_first_of( tv, "()" );
  head = v[0];
  mods.remove();
  if ( v.size() > 1 ){
    vector<UnicodeString> mv = TiCC::split_at( v[1], "," );
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
  /// setup the parser for  action
  /*!
    \param fd the frog_data structure with the needed information
    \return a parseData structure for further processing
  */
  parseData pd;                                          //     |
  for ( size_t i = 0; i < fd.units.size(); ++i ){        //     |
    if ( fd.mwus.find( i ) == fd.mwus.end() ){           //     |
      UnicodeString head;                                       //     |
      UnicodeString mods;                                       //     |
      extract( fd.units[i].tag, head, mods );            //     |
      UnicodeString word_s = filter_spaces(fd.units[i].word);   //     |
      pd.words.push_back( word_s );                      //     |
      pd.heads.push_back( head );                        //     |
      if ( mods.isEmpty() ){                               //    \/
	// HACK: make this bug-to-bug compatible with older versions.
	// But in fact this should also be done for the mwu's loop below!
	// now sometimes empty mods get appended there.
	// extract should return "__", I suppose  (see above)
	mods = "__";
      }
      pd.mods.push_back( mods );
    }
    else {
      UnicodeString multi_word;
      UnicodeString multi_head;
      UnicodeString multi_mods;
      for ( size_t k = i; k <= fd.mwus[i]; ++k ){
	icu::UnicodeString tmp = fd.units[k].word;
	if ( filter ){
	  tmp = filter->filter( tmp );
	}
	tmp = filter_spaces( tmp );
	UnicodeString head;
	UnicodeString mods;
	extract( fd.units[k].tag, head, mods );
	if ( k == i ){
	  multi_word = tmp;
	  multi_head = head;
	  multi_mods = mods;
	}
	else {
	  multi_word += "_" + tmp;
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


vector<timbl_result> timbl( Timbl::TimblAPI* tim,
			    const vector<UnicodeString>& instances ){
  /// call a Timbl experiment with a list of instances
  /*!
    \param tim The Timbl to use
    \param instances the instances to feed to the Timbl
    \return a list of timbl_result structures with the result of processing
    all instances
   */
  vector<timbl_result> result;
  for ( const auto& inst : instances ){
    const Timbl::ClassDistribution *db;
    const Timbl::TargetValue *tv = tim->Classify( inst, db );
    result.push_back( timbl_result( TiCC::UnicodeToUTF8(tv->name()),
				    db->Confidence(tv), *db ) );
  }
  return result;
}

vector<pair<string,double>> parse_vd( const string& ds ){
  /// parse a ClassDistribution string into a vector of class/value pairs
  /*!
    \param ds a string representation of a Timbl ClassDistribution
    \return a vector of string/double pairs. Each pair is one class + it's
    (relative) count
  */
  vector<pair<string,double>> result;
  vector<string> parts = TiCC::split_at_first_of( ds, "{,}" );
  for ( const auto& p : parts ){
    vector<string> sd = TiCC::split( p );
    assert( sd.size() == 2 );
    result.push_back( make_pair( sd[0], TiCC::stringTo<double>( sd[1] ) ) );
  }
  return result;
}

vector<timbl_result> Parser::timbl_server( const string& base,
					   const vector<UnicodeString>& instances ){
  /// call a Timbl Server with a list of instances
  /*!
    \param base used to select the Timbl to use (should be configured correctly)
    \param instances the instances to feed to the Timbl Server
    \return a list of timbl_result structures with the result of processing
    all instances
   */
  vector<timbl_result> result;
  Sockets::ClientSocket client;
  if ( !client.connect( _host, _port ) ){
    LOG << "failed to open connection, " << _host
	<< ":" << _port << endl
	<< "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  DBG << "calling " << base << " server" << endl;
  string line;
  client.read( line );
  json response;
  try {
    response = json::parse( line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << line << "':"
	<< e.what() << endl;
    abort();
  }
  DBG << "received json data:" << response.dump(2) << endl;

  json out_json;
  out_json["command"] = "base";
  out_json["param"] = base;
  string out_line = out_json.dump() + "\n";
  DBG << "sending BASE json data:" << out_line << endl;
  client.write( out_line );
  client.read( line );
  try {
    response = json::parse( line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << line << "':"
	<< e.what() << endl;
    abort();
  }
  DBG << "received json data:" << response.dump(2) << endl;

  // create json query struct
  json query;
  query["command"] = "classify";
  json arr = json::array();
  for ( const auto& inst : instances ){
    arr.push_back( TiCC::UnicodeToUTF8(inst) );
  }
  query["params"] = arr;
  DBG << "send json" << query.dump(2) << endl;
  // send it to the server
  line = query.dump() + "\n";
  client.write( line );
  // receive json
  client.read( line );
  DBG << "received line:" << line << "" << endl;
  try {
    response = json::parse( line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << line << "':"
	<< e.what() << endl;
    LOG << "the request to the server was: '" <<  query.dump(2) << "'" << endl;
    abort();
  }
  DBG << "received json data:" << response.dump(2) << endl;
  if ( !response.is_array() ){
    string cat = response["category"];
    double conf = response.value("confidence",0.0);
    vector<pair<string,double>> vd = parse_vd(response["distribution"]);
    result.push_back( timbl_result( cat, conf, vd ) );
  }
  else {
    for ( const auto& it : response.items() ){
      string cat = it.value()["category"];
      double conf = it.value().value("confidence",0.0);
      vector<pair<string,double>> vd = parse_vd( it.value()["distribution"] );
      result.push_back( timbl_result( cat, conf, vd ) );
    }
  }
  return result;
}

void appendParseResult( frog_data& fd,
			const vector<parsrel>& res ){
  /// transfer the outcome of the parser back into the fog_data structure
  /*!
    \param fd the frog_data we want to add to
    \param res the parser's results
  */
  vector<int> nums;
  vector<string> roles;
  for ( const auto& it : res ){
    if ( it.head == 0 && it.deprel.empty() ){
      continue;
    }
    nums.push_back( it.head );
    roles.push_back( it.deprel );
  }
  // cerr << "NUMS=" << nums << endl;
  // cerr << "roles=" << roles << endl;
  for ( size_t i = 0; i < nums.size(); ++i ){
    fd.mw_units[i].parse_index = nums[i];
    fd.mw_units[i].parse_role = roles[i];
  }
}

void Parser::Parse( frog_data& fd, TimerBlock& timers ){
  /// Run the Parser on 1 frog_data structure
  /*!
    \param fd the frog_data structure with our input
    \param timers the TimerBlock for measuring what we wasting

    This function will run 3 Timbl's in parallel to get its information
    which is the handled to the 'real' parsing CSIDP process
  */
  timers.parseTimer.start();
  if ( !isInit ){
    LOG << "Parser is not initialized! EXIT!" << endl;
    throw runtime_error( "Parser is not initialized!" );
  }
  if ( fd.empty() ){
    LOG << "unable to parse an analysis without words" << endl;
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
	vector<UnicodeString> instances = createPairInstances( pd );
	if ( _host.empty() ){
	  p_results = timbl( pairs, instances );
	}
	else {
	  p_results = timbl_server( _pairs_base, instances );
	}
	timers.pairsTimer.stop();
      }
#pragma omp section
      {
	timers.dirTimer.start();
	vector<UnicodeString> instances = createDirInstances( pd );
	if ( _host.empty() ){
	  d_results = timbl( dir, instances );
	}
	else {
	  d_results = timbl_server( _dirs_base, instances );
	}
	timers.dirTimer.stop();
      }
#pragma omp section
      {
	timers.relsTimer.start();
	vector<UnicodeString> instances = createRelInstances( pd );
	if ( _host.empty() ){
	  r_results = timbl( rels, instances );
	}
	else {
	  r_results = timbl_server( _rels_base, instances );
	}
	timers.relsTimer.stop();
      }
  }

  timers.csiTimer.start();
  vector<parsrel> res = parse( p_results,
			       r_results,
			       d_results,
			       pd.words.size(),
			       maxDepSpan,
			       dbgLog );
  timers.csiTimer.stop();
  appendParseResult( fd, res );
  timers.parseTimer.stop();
}

ParserBase::~ParserBase(){
  delete errLog;
  delete dbgLog;
  delete filter;
}

void ParserBase::add_result( const frog_data& fd,
			     const vector<folia::Word*>& wv ) const {
  /// add the parser's conclusiong to the FoLiA we are working on
  /*!
    \param fd the frog_data with the parse outcome allready inserted
    \param wv the original folia::Word elements.

    This will create a DependencyLayer at the Sentence level of the \e wv with
    Dependency records for every dependency found
  */
  //  DBG << "Parser::add_result:" << endl << fd << endl;
  folia::Sentence *s = wv[0]->sentence();
  folia::KWargs args;
  if ( !s->id().empty() ){
    args["generate_id"] = s->id();
  }
  args["set"] = getTagset();
  // if ( textclass != "current" ){
  //   args["textclass"] = textclass;
  // }
  folia::DependenciesLayer *el = s->add_child<folia::DependenciesLayer>( args );
  for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
    //    DBG << "POS=" << pos << endl;
    string cls = fd.mw_units[pos].parse_role;
    int dep_id = fd.mw_units[pos].parse_index;
    if ( cls != "ROOT" && dep_id != 0 ){
      if ( !el->id().empty() ){
	args["generate_id"] = el->id();
      }
      args["class"] = cls;
      if ( textclass != "current" ){
	args["textclass"] = textclass;
      }
      args["set"] = getTagset();
      folia::Dependency *dep = el->add_child<folia::Dependency>( args );
      args.clear();
      // if ( textclass != "current" ){
      // 	args["textclass"] = textclass;
      // }
      //      LOG << "wv.size=" << wv.size() << endl;
      folia::Headspan *dh = dep->add_child<folia::Headspan>( args );
      // DBG << "mw_units:" << fd.mw_units[pos] << endl;
      size_t head_index = fd.mw_units[pos].parse_index-1;
      // DBG << "head_index=" << head_index << endl;
      for ( auto const& i : fd.mw_units[head_index].parts ){
	// DBG << "i=" << i << endl;
	dh->append( wv[i] );
      }
      folia::DependencyDependent *dd = dep->add_child<folia::DependencyDependent>( args );
      for ( auto const& i : fd.mw_units[pos].parts ){
	dd->append( wv[i] );
      }
    }
  }
}
