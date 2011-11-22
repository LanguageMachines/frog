/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2011
  Tilburg University

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for Dutch
  Version 0.04
 
  This file is part of frog.

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
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"
#include "timbl/TimblAPI.h"
#include "libfolia/folia.h"
#include "frog/Frog.h"
#include "frog/Configuration.h"
#include "frog/mwu_chunker_mod.h"
#include "frog/FrogData.h"
#include "frog/Parser.h"

using namespace std;
using namespace Timbl;
using namespace folia;

PythonInterface::PythonInterface( ) {
  Py_OptimizeFlag = 1; // enable optimisation (-O) mode
  Py_Initialize();
  string newpath = Py_GetPath();
  newpath += string(":") + PYTHONDIR;
  PySys_SetPath( (char*)newpath.c_str() );
  try {
    PyObject *im = PyImport_ImportModule( "csidp" );
    if ( im ){
      module.assign( im );
      PyObject *mf = PyObject_GetAttrString(module, "main");
      if ( mf )
	mainFunction.assign( mf );
      else {
	PyErr_Print();
	exit(1);
      }
    }
    else {
      PyErr_Print();
      exit(1);
    }
  }
  catch( exception const & ){
    PyErr_Print();
    exit(1);
  }
}

PythonInterface::~PythonInterface() {
  Py_Finalize();
}

void PythonInterface::parse( const string& depFile,
			     const string& modFile,
			     const string& dirFile,
			     const string& maxDist,
			     const string& inputFile,
			     const string& outputFile ) {
  
  PyObjectRef tmp = PyObject_CallFunction(mainFunction,
					  (char *)"[s, s, s, s, s, s, s, s, s, s, s]",
					  "--dep", depFile.c_str(),
					  "--mod", modFile.c_str(),
					  "--dir", dirFile.c_str(),
					  "-m", maxDist.c_str(),
					  "--out", outputFile.c_str(),
					  inputFile.c_str() );
}

Common::Timer prepareTimer;
Common::Timer relsTimer;
Common::Timer pairsTimer;
Common::Timer dirTimer;
Common::Timer csiTimer;

struct parseData {
  void clear() { words.clear(); heads.clear(); mods.clear(); mwus.clear(); }
  vector<string> words;
  vector<string> heads;
  vector<string> mods;
  vector<vector<AbstractElement *> > mwus;
};

ostream& operator<<( ostream& os, const parseData& pd ){
  using folia::operator<<;
  os << "pd words " << pd.words << endl;
  os << "pd heads " << pd.heads << endl;
  os << "pd mods " << pd.mods << endl;
  os << "pd mwus ";
  for ( size_t i=0; i < pd.mwus.size(); ++i ){
    for ( size_t j=0; j < pd.mwus[i].size(); ++j )
      os << pd.mwus[i][j]->id();
    os << endl;
  }
  return os;
}
  
void Parser::createParserFile( const parseData& pd ){
  const vector<string>& words = pd.words;
  const vector<string>& heads = pd.heads;
  const vector<string>& mods = pd.mods;

  ofstream anaFile( fileName.c_str() );
  if ( anaFile ){
    for( size_t i = 0; i < words.size(); ++i ){
      anaFile << i+1 << "\t" << words[i] << "\t" << "*" << "\t" << heads[i] 
	      << "\t" << heads[i] << "\t" << mods[i] << "\t"<< "0" 
	      << "\t" << "_" << "\t" << "_" << "\t" << "_" << endl;
    }
  }
  else {
    cerr << "unable to create a parser file " << fileName << endl;
    exit( EXIT_FAILURE );
  }
}

bool Parser::init( const Configuration& configuration ){
  string pairsFileName;
  string pairsOptions = "-a1 +D +vdb+di";
  string dirFileName;
  string dirOptions = "-a1 +D +vdb+di";
  string relsFileName;
  string relsOptions = "-a1 +D +vdb+di";
  PI = new PythonInterface();
  parseLog = new LogStream( theErrLog );
  parseLog->addmessage("parser-");
  maxDepSpanS = "20";
  maxDepSpan = 20;
  bool problem = false;
  *Log(parseLog) << "initiating parser ... " << endl;
  string cDir = configuration.configDir();
  string att = configuration.lookUp( "maxDepSpan", "parser" );
  if ( !att.empty() ){
    size_t gs;
    if ( stringTo<size_t>( att, gs, 0, 50 ) ){
      maxDepSpanS = att;
      maxDepSpan = gs;
    }
    else {
      *Log(parseLog) << "invalid maxDepSPan value in config file" << endl;
      *Log(parseLog) << "keeping default " << maxDepSpan << endl;
      problem = true;
    }
  }
  att = configuration.lookUp( "pairsFile", "parser" );
  if ( !att.empty() )
    pairsFileName = prefix( cDir, att );
  else {
    *Log(parseLog) << "missing pairsFile option" << endl;
    problem = true;
  }
  att = configuration.lookUp( "pairsOptions", "parser" );
  if ( !att.empty() )
    pairsOptions = att;
  else {
    *Log(parseLog) << "missing pairsOptions option" << endl;
    problem = true;
  }
  att = configuration.lookUp( "dirFile", "parser" );
  if ( !att.empty() )
    dirFileName = prefix( cDir, att );
  else {
    *Log(parseLog) << "missing dirFile option" << endl;
    problem = true;
  }
  att = configuration.lookUp( "dirOptions", "parser" );
  if ( !att.empty() ){
    dirOptions = att;
  }
  else {
    *Log(parseLog) << "missing dirOptions option" << endl;
    problem = true;
  }
  att = configuration.lookUp( "relsFile", "parser" );
  if ( !att.empty() )
    relsFileName = prefix( cDir, att );
  else {
    *Log(parseLog) << "missing relsFile option" << endl;
    problem = true;
  }
  att = configuration.lookUp( "relsOptions", "parser" );
  if ( !att.empty() ){
    relsOptions = att;
  }
  else {
    *Log(parseLog) << "missing relsOptions option" << endl;
    problem = true;
  }
  if ( problem )
    return false;
  
  bool happy = true;
  pairs = new TimblAPI( pairsOptions );
  if ( pairs->Valid() ){
    *Log(parseLog) << "reading " <<  pairsFileName << endl;
    happy = pairs->GetInstanceBase( pairsFileName );
  }
  else
    *Log(parseLog) << "creating Timbl for pairs failed:" << pairsOptions << endl;
  if ( happy ){
    dir = new TimblAPI( dirOptions );
    if ( dir->Valid() ){
      *Log(parseLog) << "reading " <<  dirFileName << endl;
      happy = dir->GetInstanceBase( dirFileName );
    }
    else
      *Log(parseLog) << "creating Timbl for dir failed:" << dirOptions << endl;
    if ( happy ){
      rels = new TimblAPI( relsOptions );
      if ( rels->Valid() ){
	*Log(parseLog) << "reading " <<  relsFileName << endl;
	happy = rels->GetInstanceBase( relsFileName );
      }
      else
	*Log(parseLog) << "creating Timbl for rels failed:" << relsOptions << endl;
    }
  }
  isInit = happy;
  return happy;
}

Parser::~Parser(){
  delete rels;
  delete dir;
  delete pairs;
  delete PI;
}

static vector<AbstractElement *> lookup( AbstractElement *word, 
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

void Parser::createPairs( const parseData& pd ){
  const vector<string>& words = pd.words;
  const vector<string>& heads = pd.heads;
  const vector<string>& mods = pd.mods;
  string pFile = fileName + ".pairs.inst";
  remove( pFile.c_str() );
  ofstream ps( pFile.c_str() );
  if ( ps ){
    if ( words.size() == 1 ){
      ps << "__ " << words[0] << " __"
	 << " ROOT ROOT ROOT __ " << heads[0]
	 << " __ ROOT ROOT ROOT "
	 << words[0] << "^ROOT ROOT ROOT ROOT^" 
	 << heads[0]
	 << " _" << endl;
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
	ps << word_1 << " " << word0 << " " << word1
	   << " ROOT ROOT ROOT "
	   << tag_1 << " " << tag0 << " " << tag1 
	   << " ROOT ROOT ROOT "
	   << tag0 << "^ROOT ROOT ROOT ROOT^" << mods0
	   << " _" << endl;
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
	  //	  os << wPos << "-" << pos << " ";
	  if ( pos > wPos + maxDepSpan )
	    break;
	  if ( wPos == pos )
	    continue;
	  if ( wPos > maxDepSpan + pos )
	    continue;
	  
	  ps << w_word_1;
	  ps << " " << w_word0;
	  ps << " " << w_word1;
	  
	  if ( pos == 0 )
	    ps << " __";
	  else
	    ps << " " << words[pos-1];
	  if ( pos < words.size() )
	    ps << " " << words[pos];
	  else
	    ps << " __";
	  if ( pos < words.size()-1 )
	    ps << " " << words[pos+1];
	  else
	    ps << " __";
	  ps << " " << w_tag_1;
	  ps << " " << w_tag0;
	  ps << " " << w_tag1;
	  if ( pos == 0 )
	    ps << " __";
	  else
	    ps << " " << heads[pos-1];
	  if ( pos < words.size() )
	    ps << " " << heads[pos];
	  else
	    ps << " __";
	  if ( pos < words.size()-1 )
	    ps << " " << heads[pos+1];
	  else
	    ps << " __";
	  
	  ps << " " << w_tag0 << "^";
	  if ( pos < words.size() )
	    ps << heads[pos];
	  else
	    ps << "__";
	  
	  if ( wPos > pos )
	    ps << " LEFT " << wPos - pos;
	  else
	    ps << " RIGHT " << pos - wPos;
	  if ( pos >= words.size() )
	    ps << " __";
	  else
	    ps << " " << mods[pos];
	  ps << "^" << w_mods0;
	  ps << " __" << endl;
	}
      }
    }
  }
}

void Parser::createRelDir( const parseData& pd ){
  const vector<string>& words = pd.words;
  const vector<string>& heads = pd.heads;
  const vector<string>& mods = pd.mods;

  string word_2, word_1, word0, word1, word2;
  string tag_2, tag_1, tag0, tag1, tag2;
  string mod_2, mod_1, mod0, mod1, mod2;
  string dFile = fileName + ".dir.inst";
  remove( dFile.c_str() );
  ofstream ds( dFile.c_str() );
  string rFile = fileName + ".rels.inst";
  remove( rFile.c_str() );
  ofstream rs( rFile.c_str() );
  if ( ds && rs ){
    if ( words.size() == 1 ){
      word0 = words[0];
      tag0 = heads[0];
      mod0 = mods[0];
      ds << "__ __";
      ds << " " << word0;
      ds << " __ __ __ __";
      ds << " " << tag0;
      ds << " __ __ __ __";
      ds << " " << word0 << "^" << tag0;
      ds << " __ __ __^" << tag0;
      ds << " " << tag0 << "^__";
      ds << " __";
      ds << " " << mod0;
      ds << " __ ROOT" << endl;
      //
      rs << "__ __";
      rs << " " << word0;
      rs << " __ __";
      rs << " " << mod0;
      rs << " __ __";
      rs << " " << tag0;
      rs << " __ __";
      rs << " __^" << tag0;
      rs << " " << tag0 << "^__";
      rs << " __^__^" << tag0;
      rs << " " << tag0 << "^__^__";
      rs << " __" << endl;
    }
    else if ( words.size() == 2 ){
      word0 = words[0];
      tag0 = heads[0];
      mod0 = mods[0];
      word1 = words[1];
      tag1 = heads[1];
      mod1 = mods[1];
      ds << "__ __";
      ds << " " << word0;
      ds << " " << word1;
      ds << " __ __ __";
      ds << " " << tag0;
      ds << " " << tag1;
      ds << " __ __ __";
      ds << " " << word0 << "^" << tag0;
      ds << " " << word1 << "^" << tag1;
      ds << " __ __^" << tag0;
      ds << " " << tag0 << "^" << tag1;
      ds << " __";
      ds << " " << mod0;
      ds << " " << mod1;
      ds << " ROOT" << endl;
      ds << "__";
      ds << " " << word0;
      ds << " " << word1;
      ds << " __ __ __";
      ds << " " << tag0;
      ds << " " << tag1;
      ds << " __ __ __";
      ds << " " << word0 << "^" << tag0;
      ds << " " << word1 << "^" << tag1;
      ds << " __ __";
      ds << " " << tag0 << "^" << tag1;
      ds << " " << tag1 << "^__";
      ds << " " << mod0;
      ds << " " << mod1;
      ds << " __";
      ds << " ROOT" << endl;
      //
      rs << "__ __";
      rs << " " << word0;
      rs << " " << word1;
      rs << " __";
      rs << " " << mod0;
      rs << " __ __";
      rs << " " << tag0;
      rs << " " << tag1;
      rs << " __";
      rs << " __^" << tag0;
      rs << " " << tag0 << "^" << tag1;
      rs << " __^__^" << tag0;
      rs << " " << tag0 << "^" << tag1 << "^__";
      rs << " __" << endl;
      rs << "__";
      rs << " " << word0;
      rs << " " << word1;
      rs << " __ __";
      rs << " " << mod1;
      rs << " __";
      rs << " " << tag0;
      rs << " " << tag1;
      rs << " __ __";
      rs << " " << tag0 << "^" << tag1;
      rs << " " << tag1 << "^__";
      rs << " __^" << tag0 << "^" << tag1;
      rs << " " << tag1 << "^__^__";
      rs << " __" << endl;
    }
    else if ( words.size() == 3 ) {
      word0 = words[0];
      tag0 = heads[0];
      mod0 = mods[0];
      word1 = words[1];
      tag1 = heads[1];
      mod1 = mods[1];
      word2 = words[2];
      tag2 = heads[2];
      mod2 = mods[2];
      ds << "__ __";
      ds << " " << word0;
      ds << " " << word1;
      ds << " " << word2;
      ds << " __ __";
      ds << " " << tag0;
      ds << " " << tag1;
      ds << " " << tag2;
      ds << " __ __";
      ds << " " << word0 << "^" << tag0;
      ds << " " << word1 << "^" << tag1;
      ds << " " << word2 << "^" << tag2;
      ds << " __^" << tag0;
      ds << " " << tag0 << "^" << tag1;
      ds << " __";
      ds << " " << mod0;
      ds << " " << mod1;
      ds << " ROOT" << endl;
      ds << "__";
      ds << " " << word0;
      ds << " " << word1;
      ds << " " << word2;
      ds << " __ __";
      ds << " " << tag0;
      ds << " " << tag1;
      ds << " " << tag2;
      ds << " __ __";
      ds << " " << word0 << "^" << tag0;
      ds << " " << word1 << "^" << tag1;
      ds << " " << word2 << "^" << tag2;
      ds << " __";
      ds << " " << tag0 << "^" << tag1;
      ds << " " << tag1 << "^" << tag2;
      ds << " " << mod0;
      ds << " " << mod1;
      ds << " " << mod2;
      ds << " ROOT" << endl;
      ds << word0;
      ds << " " << word1;
      ds << " " << word2;
      ds << " __ __";
      ds << " " << tag0;
      ds << " " << tag1;
      ds << " " << tag2;
      ds << " __ __";
      ds << " " << word0 << "^" << tag0;
      ds << " " << word1 << "^" << tag1;
      ds << " " << word2 << "^" << tag2;
      ds << " __ __";
      ds << " " << tag1 << "^" << tag2;
      ds << " " << tag2 << "^__";
      ds << " " << mod1;
      ds << " " << mod2;
      ds << " __";
      ds << " ROOT" << endl;
      //
      rs << "__ __";
      rs << " " << word0;
      rs << " " << word1;
      rs << " " << word2;
      rs << " " << mod0;
      rs << " __ __";
      rs << " " << tag0;
      rs << " " << tag1;
      rs << " " << tag2;
      rs << " __^" << tag0;
      rs << " " << tag0 << "^" << tag1;
      rs << " __^__^" << tag0;
      rs << " " << tag0 << "^" << tag1 << "^" << tag2;
      rs << " __" << endl;
      rs << "__";
      rs << " " << word0;
      rs << " " << word1;
      rs << " " << word2;
      rs << " __";
      rs << " " << mod1;
      rs << " __";
      rs << " " << tag0;
      rs << " " << tag1;
      rs << " " << tag2;
      rs << " __";
      rs << " " << tag0 << "^" << tag1;
      rs << " " << tag1 << "^" << tag2;
      rs << " __^" << tag0 << "^" << tag1;
      rs << " " << tag1 << "^" << tag2 << "^__";
      rs << " __" << endl;
      rs << word0;
      rs << " " << word1;
      rs << " " << word2;
      rs << " __ __";
      rs << " " << mod2;
      rs << " " << tag0;
      rs << " " << tag1;
      rs << " " << tag2;
      rs << " __ __";
      rs << " " << tag1 << "^" << tag2;
      rs << " " << tag2 << "^__";
      rs << " " << tag0 << "^" << tag1 << "^" << tag2;
      rs << " " << tag2 << "^__^__";
      rs << " __" << endl;
    }
    else {
      for ( size_t i=0 ; i < words.size(); ++i ){
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
	word0 = words[i];
	tag0 = heads[i];
	mod0 = mods[i];
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
	ds << word_2;
	ds << " " << word_1;
	ds << " " << word0;
	ds << " " << word1;
	ds << " " << word2;
	ds << " " << tag_2;
	ds << " " << tag_1;
	ds << " " << tag0;
	ds << " " << tag1;
	ds << " " << tag2;
	ds << " " << word_2 << "^" << tag_2;
	ds << " " << word_1 << "^" << tag_1;
	ds << " " << word0 << "^" << tag0;
	ds << " " << word1 << "^" << tag1;
	ds << " " << word2 << "^" << tag2;
	ds << " " << tag_1 << "^" << tag0;
	ds << " " << tag0 << "^" << tag1;
	ds << " " << mod_1;
	ds << " " << mod0;
	ds << " " << mod1;
	ds << " ROOT" << endl;
	//
	rs << word_2;
	rs << " " << word_1;
	rs << " " << word0;
	rs << " " << word1;
	rs << " " << word2;
	rs << " " << mod0;
	rs << " " << tag_2;
	rs << " " << tag_1;
	rs << " " << tag0;
	rs << " " << tag1;
	rs << " " << tag2;
	rs << " " << tag_1 << "^" << tag0;
	rs << " " << tag0 << "^" << tag1;
	rs << " " << tag_2 << "^" << tag_1 << "^" << tag0;
	rs << " " << tag0 << "^" << tag1 << "^" << tag2;
	rs << " __" << endl;
      }
    }
  }
}

void Parser::prepareParse( AbstractElement *sent, parseData& pd ) {

  pd.clear();
  vector<AbstractElement *> fwords = sent->words();
  vector<AbstractElement *> entities = sent->select( Entity_t );
  
  for( size_t i=0; i < fwords.size(); ++i ){
    AbstractElement *word = fwords[i];
    vector<AbstractElement *> mwu = lookup( word, entities );
    if ( !mwu.empty() ){
      string word;
      string head;
      string mod;
      for ( size_t p=0; p < mwu.size(); ++p ){
	word += mwu[p]->str();
	AbstractElement *postag = mwu[p]->annotation(Pos_t);
	string pt = postag->cls();
	string cls = pt.substr(0,pt.find_first_of("("));
	head += cls;
	string tmp = pt.substr(pt.find_first_of("(")+1, 
			       pt.size() - pt.find_first_of("(") - 2 );
	for ( size_t i=0; i < tmp.size(); ++i ){
	  if ( tmp[i] == ',' )
	    tmp[i] = '|';
	}
	mod += tmp;
	if ( p < mwu.size() -1 ){
	  word += "_";
	  head += "_";
	  mod += "_";
	}
      }
      pd.words.push_back( word );
      pd.heads.push_back( head );
      pd.mods.push_back( mod );
      pd.mwus.push_back( mwu );
      i += mwu.size()-1;
    }
    else {
      pd.words.push_back( word->str() );
      AbstractElement *postag = word->annotation(Pos_t);
      string pt = postag->cls();
      string cls = pt.substr(0,pt.find_first_of("("));
      pd.heads.push_back( cls );
      string tmp = pt.substr(pt.find_first_of("(")+1, 
			     pt.size() - pt.find_first_of("(") - 2 );
      if ( tmp.empty() )
	tmp = "__";
      for ( size_t i=0; i < tmp.size(); ++i ){
	if ( tmp[i] == ',' )
	  tmp[i] = '|';
      }
      pd.mods.push_back( tmp );
      vector<AbstractElement*> vec;
      vec.push_back(word);
      pd.mwus.push_back( vec );
    }
  }
  
  createParserFile( pd );

#pragma omp parallel sections
  {
#pragma omp section
    {
      createPairs( pd );
    }
#pragma omp section
    {
      createRelDir( pd );
    }
  }
}

 void appendParseResult( AbstractElement *sent, 
			 parseData& pd,
			 istream& is ){
   string line;
   int cnt=0;
   vector<int> nums;
   vector<string> roles;
   while ( getline( is, line ) ){
     vector<string> parts;
     int num = Timbl::split_at( line, parts, " " );
     if ( num > 7 ){
       if ( Timbl::stringTo<int>( parts[0] ) != cnt+1 ){
	 *Log(theErrLog) << "confused! " << endl;
	 *Log(theErrLog) << "got line '" << line << "'" << endl;
       }
       nums.push_back( Timbl::stringTo<int>(parts[6]) );
       roles.push_back( parts[7] );
     }
     ++cnt;
   }
   AbstractElement *dl = new DependenciesLayer("");
   sent->append( dl );
   
   for ( size_t i=0; i < nums.size(); ++i ){
     if ( nums[i] != 0 ){
       AbstractElement *d = new Dependency( "generate-id='" + sent->id() + 
					    "', class='"+ roles[i] +"'" );
       dl->append( d );
       AbstractElement *h = new DependencyHead("");
       for ( size_t j=0; j < pd.mwus[nums[i]-1].size(); ++ j ){
	 h->append( pd.mwus[nums[i]-1][j] );
       }
       d->append( h );
       h = new DependencyDependent("");
       for ( size_t j=0; j < pd.mwus[i].size(); ++ j ){
	 h->append( pd.mwus[i][j] );
       }
       d->append( h );
     }
   }
 }
 
 
void Parser::Parse( FrogData *fd, 
		    AbstractElement *sent,
		    const string& tmpDirName, TimerBlock& timers ){
  fileName = tmpDirName+"csiparser";
  timers.parseTimer.start();
  if ( !isInit ){
    *Log(parseLog) << "Parser is not initialized!" << endl;
    exit(1);
  }
  if ( !sent ){
    *Log(parseLog) << "unable to parse an analisis without words" << endl;
    return;
  }
  string resFileName = fileName + ".result"; 
  string pairsInName = fileName +".pairs.inst";
  string pairsOutName = fileName +".pairs.out";
  string dirInName = fileName + ".dir.inst";
  string dirOutName = fileName + ".dir.out";
  string relsInName = fileName + ".rels.inst";
  string relsOutName = fileName + ".rels.out";
  remove( resFileName.c_str() );
  timers.prepareTimer.start();
  parseData pd;  
  prepareParse( sent, pd );
  timers.prepareTimer.stop();
#pragma omp parallel sections
  {
#pragma omp section
    {
      remove( pairsOutName.c_str() );
      timers.pairsTimer.start();
      pairs->Test( pairsInName, pairsOutName );
      timers.pairsTimer.stop();
    }
#pragma omp section
    {
      remove( dirOutName.c_str() );
      timers.dirTimer.start();
      dir->Test( dirInName, dirOutName );
      timers.dirTimer.stop();
    }
#pragma omp section
    {
      remove( relsOutName.c_str() );
      timers.relsTimer.start();
      rels->Test( relsInName, relsOutName );
      timers.relsTimer.stop();
    }
  }
  timers.csiTimer.start();
  try {
    PI->parse( pairsOutName,
	       relsOutName,
	       dirOutName,
	       maxDepSpanS,
	       fileName,
	       resFileName );
    if ( PyErr_Occurred() )
      PyErr_Print();
  }
  catch( exception const & ){
    PyErr_Print();
  }
  timers.csiTimer.stop();
  ifstream resFile( resFileName.c_str() );
  if ( resFile ){
    fd->appendParseResult( resFile );
  }
  else
    *Log(parseLog) << "couldn't open results file: " << resFileName << endl;
  resFile.close();
  resFile.open( resFileName.c_str() );
  if ( resFile ){
    appendParseResult( sent, pd, resFile );
  }
  else
    *Log(parseLog) << "couldn't reopen open results file: " << resFileName << endl;

  if ( !keepIntermediateFiles ){
    remove( fileName.c_str() );
    remove( resFileName.c_str() );
    remove( pairsOutName.c_str() );
    remove( dirOutName.c_str() );
    remove( relsOutName.c_str() );
    remove( pairsInName.c_str() );
    remove( dirInName.c_str() );
    remove( relsInName.c_str() );
  }
  timers.parseTimer.stop();
}
