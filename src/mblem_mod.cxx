/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2012
  Tilburg University

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

#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include "timbl/TimblAPI.h"

#include "ucto/unicode.h"
#include "libfolia/folia.h"
#include "libfolia/document.h"
#include "frog/Frog.h"
#include "frog/Configuration.h"
#include "frog/mblem_mod.h"

using namespace std;
using namespace folia;

Mblem::Mblem(): myLex(0),punctuation( "?...,:;\\'`(){}[]%#+-_=/!" ), 
		history(20), debug(0) {
  mblemLog = new LogStream( theErrLog, "mblem" );
}

void Mblem::read_transtable( const string& tableName ) {
  ifstream bron( tableName.c_str() );
  if ( !bron ) {
    *Log(mblemLog) << "translation table file '" << tableName 
		    << "' appears to be missing." << endl;
    exit(1);
  }
  while( bron ){
    string className;
    string classCode;
    bron >> className;
    bron >> ws;
    bron >> classCode;
    if ( classMap.find( classCode ) == classMap.end() ){
      // stupid HACK to only accept first occurence
      // multiple occurences is a NO NO i think
      classMap[classCode] = className;
    }
    // else {
    //   *Log(mblemLog) << "multiple entry " << className << " " << classCode << " in translation table file: " << tableName  << " (Ignored) " << endl;      
    // }
    bron >> ws;
  }
  return;
}

bool Mblem::init( const Configuration& config ) {
  *Log(mblemLog) << "Initiating lemmatizer..." << endl;
  debug = tpDebug;
  string db = config.lookUp( "debug", "mblem" );
  if ( !db.empty() )
    debug = stringTo<int>( db );
  string val = config.lookUp( "version", "mblem" );
  if ( val.empty() ){
    version = "1.0";
  }
  else
    version = val;
  val = config.lookUp( "set", "mblem" );
  if ( val.empty() ){
    tagset = "http://ilk.uvt.nl/folia/sets/frog-mblem-nl";
  }
  else
    tagset = val;
  
  string transName = config.lookUp( "transFile", "mblem" );
  if ( !transName.empty() ){
    transName = prefix( config.configDir(), transName );
    read_transtable( transName );
  }
  string treeName = config.lookUp( "treeFile", "mblem"  );
  if ( treeName.empty() )
    treeName = "mblem.tree";
  treeName = prefix( config.configDir(), treeName );
  string opts = config.lookUp( "timblOpts", "mblem" );
  if ( opts.empty() )
    opts = "-a1";
  //make it silent
  opts += " +vs -vf";	    
  //Read in (igtree) data
  myLex = new Timbl::TimblAPI(opts);
  return myLex->GetInstanceBase(treeName);
}

Mblem::~Mblem(){
  //    *Log(mblemLog) << "cleaning up MBLEM stuff" << endl;
  delete myLex;
  myLex = 0;
  delete mblemLog;
}

string Mblem::make_instance( const UnicodeString& in ) {
  if (debug)
    *Log(mblemLog) << "making instance from: " << in << endl;
  UnicodeString instance = "";
  size_t length = in.length();
  size_t j;
  for ( size_t i=0; i < history; i++) {
    j = length - history + i;
    if (( i < history - length ) &&
	(length<history))
      instance += "= ";
    else {
      instance += in[j];
      instance += ' ';
    }
  }
  instance += "?";
  string result = UnicodeToUTF8(instance);
  if (debug)
    *Log(mblemLog) << "inst: " << instance << endl;
  
  return result;
}

bool similar( const string& tag, const string& lookuptag,
	      const string& CGNentry ){
  return tag.find( CGNentry ) != string::npos &&
    lookuptag.find( CGNentry ) != string::npos ;
}

bool isSimilar( const string& tag, const string& cgnTag ){
  // Dutch CGN constraints
  return 
    tag == cgnTag ||
    similar( tag, cgnTag, "hulpofkopp" ) ||
    similar( tag, cgnTag, "neut,zelfst" ) ||
    similar( tag, cgnTag, "rang,bep,zelfst,onverv" ) ||
    similar( tag, cgnTag, "stell,onverv" ) ||
    similar( tag, cgnTag, "hoofd,prenom" ) ||
    similar( tag, cgnTag, "soort,ev" ) ||
    similar( tag, cgnTag, "ev,neut" ) ||
    similar( tag, cgnTag, "inf" ) ||
    similar( tag, cgnTag, "zelfst" ) ||
    similar( tag, cgnTag, "voorinf" ) ||
    similar( tag, cgnTag, "verldw,onverv" ) ||
    similar( tag, cgnTag, "ott,3,ev" ) ||
    similar( tag, cgnTag, "ott,2,ev" ) ||
    similar( tag, cgnTag, "ott,1,ev" ) ||
    similar( tag, cgnTag, "ott,1of2of3,mv" ) ||
    similar( tag, cgnTag, "ott,1of2of3,ev" ) ||
    similar( tag, cgnTag, "ovt,1of2of3,mv" ) ||
    similar( tag, cgnTag, "ovt,1of2of3,ev" ) ||
    similar( tag, cgnTag, "ovt,3,ev" ) ||
    similar( tag, cgnTag, "ovt,2,ev" ) ||
    similar( tag, cgnTag, "ovt,1,ev" );
}

void Mblem::addLemma( FoliaElement *word, const string& cls ){
  KWargs args;
  args["set"]=tagset;
  args["cls"]=cls;
#pragma omp critical(foliaupdate)
  {
    word->addLemmaAnnotation( args );
  }
}
  
void Mblem::addAltLemma( Word *word, const string& cls ){
  Alternative *alt = new Alternative();
#pragma omp critical(foliaupdate)
  {
    word->append( alt );
  }
  addLemma( alt, cls );
}
  
string Mblem::postprocess( Word *word ){
  string tag;
#pragma omp critical(foliaupdate)
  {
    tag = word->pos();
  }
  size_t index = 0;
  size_t nrlookup = mblemResult.size();
  bool none=true;
  string result;
  while ( index < nrlookup  ){
    if (debug)
      *Log(mblemLog) << "compare cgn-tag " << tag << " with " << mblemResult[index].getTag() << endl;
    if ( isSimilar( tag, mblemResult[index].getTag() ) ){
      if ( debug )
	*Log(mblemLog) << "similar! " << endl;
      string res = mblemResult[index].getLemma();
      if ( none ){
	addLemma( word, res );
	result = res;
	none = false;
      }
      else {
	// there are more matching lemmas. add them as alternatives
	addAltLemma( word, res );
      }
    }
    ++index;
  }
  
  if ( none  ) {
    if (debug)
      *Log(mblemLog) << "NO CORRESPONDING TAG! " << tag << endl;
    result = mblemResult[0].getLemma();
    addLemma( word, result );
  }
  return result;
} 

void Mblem::addDeclaration( Document& doc ) const {
  doc.declare( AnnotationType::LEMMA, 
	       tagset,
	       "annotator='frog-mblem-" + version
	       + "', annotatortype='auto'");
}

string Mblem::Classify( Word *sword ){
  string word;
  string tag;
#pragma omp critical(foliaupdate)
  {  
    word = sword->str();
    tag = sword->annotation<PosAnnotation>()->feat("head");
  }
  if ( tag == "SPEC" || tag == "LET" ) {
    addLemma( sword, word );
    return word;
  }
  UnicodeString uWord = UTF8ToUnicode(word);
  uWord.toLower();
  mblemResult.clear();
  string inst = make_instance(uWord);  
  string classString;
  myLex->Classify( inst, classString );
  if (debug)
    *Log(mblemLog) << "class: " << classString  << endl;
  // 1st find all alternatives
  vector<string> parts;
  int numParts = split_at( classString, parts, "|" );
  if ( numParts < 1 ){
    *Log(mblemLog) << "no alternatives found" << endl;
  }
  int index = 0;
  while ( index < numParts ) {
    UnicodeString part = UTF8ToUnicode( parts[index++] );
    if (debug)
      *Log(mblemLog) <<"part = " << part << endl;
    UnicodeString lemma = "";
    string restag;
    int lpos = part.indexOf("+");
    if ( lpos < 0 ){ // nothing to edit
      restag = UnicodeToUTF8( part );
      lemma = uWord;
    }
    else {
      restag = UnicodeToUTF8( UnicodeString( part, 0, lpos ) );
      UnicodeString editstr = UnicodeString( part, lpos );
      UnicodeString insstr;
      UnicodeString delstr;
      UnicodeString prefix;
      bool done = false;
      for ( int lpos=0; !done && lpos < editstr.length(); ++lpos ) {
	if ( debug )
	  *Log(mblemLog) << "editstr[" << lpos << "] = " 
			 << UnicodeToUTF8(editstr[lpos]) << endl;
	switch( editstr[lpos] ) {
	case 'P': {
	  if (editstr[lpos-1] =='+') {
	    lpos++;
	    int tmppos = editstr.indexOf("+", lpos);
	    if ( debug )
	      *Log(mblemLog) << "tmppos = " << tmppos << endl;
	    if ( tmppos >=0 ){
	      prefix = UnicodeString( editstr, lpos, tmppos - lpos );
	      lpos = tmppos - 1;
	    }
	    else {
	      prefix = UnicodeString( editstr, lpos );
	      done = true;
	    }
	    if (debug)
	      *Log(mblemLog) << "prefix=" << prefix << endl;
	  }
	  break;
	}
	case 'D': {
	  if (editstr[lpos-1] =='+') {
	    lpos++;
	    int tmppos = editstr.indexOf("+", lpos);
	    if ( tmppos >= 0 ){
	      delstr = UnicodeString( editstr, lpos, tmppos - lpos );
	      lpos = tmppos - 1;
	    }
	    else {
	      delstr = UnicodeString( editstr, lpos );
	      done = true;
	    }
	    if (debug)
	      *Log(mblemLog) << "delstr=" << delstr << endl;
	  }
	  break;
	}
	case 'I': {
	  if (editstr[lpos-1] =='+') {
	    lpos++;
	    int tmppos = editstr.indexOf("+", lpos);
	    if ( tmppos >=0 ){
	      insstr = UnicodeString( editstr, lpos, tmppos - lpos);
	      lpos = tmppos - 1;
	    }
	    else {
	      insstr = UnicodeString( editstr, lpos);
	      done = true;
	    }
	    if (debug)
	      *Log(mblemLog) << "insstr=" << insstr << endl;
	  }
	  break;
	}
	default:
	  break;
	}
      }
      if (debug){
	*Log(mblemLog) << "pre-prefix word: '" << uWord << "' prefix: '"
		       << prefix << "'" << endl;
      }	
      int prefixpos = 0;
      if ( !prefix.isEmpty() ) {
	prefixpos = uWord.indexOf(prefix);
	if (debug)
	  *Log(mblemLog) << "prefixpos = " << prefixpos << endl;
	// repair cases where there's actually not a prefix present
	if (prefixpos > uWord.length()-2) {
	  prefixpos = 0;
	  prefix.remove();
	}
      }
      
      if (debug)
	*Log(mblemLog) << "prefixpos = " << prefixpos << endl;
      if (prefixpos >= 0) {
	lemma = UnicodeString( uWord, 0L, prefixpos );
	prefixpos = prefixpos + prefix.length();
      }
      if (debug)
	*Log(mblemLog) << "post prefix != 0 word: "<< uWord 
		       << " lemma: " << lemma
		       << " prefix: " << prefix
		       << " insstr: " << insstr
		       << " delstr: " << delstr
		       << " l_delstr=" << delstr.length()
		       << " l_word=" << uWord.length()
		       << endl;
      
      if ( uWord.endsWith( delstr ) ){
	if ( uWord.length() > delstr.length() ){
	  // chop delstr from the back, but do not delete the whole word
	  UnicodeString part = UnicodeString( uWord, prefixpos, uWord.length() - delstr.length() - prefixpos );
	  lemma += part + insstr;
	}
	else if ( insstr.isEmpty() ){
	  // no replacement, just take part after the prefix
	  lemma += UnicodeString( uWord, prefixpos, uWord.length() ); // uWord;
	}
	else {
	  // but replace if possible
	  lemma += insstr;
	}
      }
      else if ( lemma.isEmpty() ){
	lemma = uWord;
      }
    }
    if ( !classMap.empty() ){
      // translate TAG(number) stuf back to CGN things
      map<string,string>::const_iterator it = classMap.find(restag);
      if ( debug )
	*Log(mblemLog) << "looking up " << restag << endl;
      if ( it != classMap.end() ){
	restag = it->second;
	if ( debug )
	  *Log(mblemLog) << "found " << restag << endl;
      }
      else
	*Log(mblemLog) << "problem: found no translation for " 
		       << restag << " using it 'as-is'" << endl;
    }
    if ( debug )
      *Log(mblemLog) << "appending lemma " << lemma << " and tag " << restag << endl;
    mblemResult.push_back( mblemData( UnicodeToUTF8(lemma), restag ) );
  } // while
  if ( debug ) {
    *Log(mblemLog) << "stored lemma and tag options: " << mblemResult.size() << " lemma's and " << mblemResult.size() << " tags:" << endl;
    for( size_t index=0; index < mblemResult.size(); ++index ){
      *Log(mblemLog) << "lemma alt: " << mblemResult[index].getLemma()
		     << "\ttag alt: " << mblemResult[index].getTag() << endl;
    }
  }
  string res = postprocess( sword ); 
  return res;
}
