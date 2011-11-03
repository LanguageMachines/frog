/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2011
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
#include "frog/Frog.h"
#include "frog/Configuration.h"
#include "frog/mblem_mod.h"

using namespace std;
using namespace Timbl;

Mblem::Mblem(): myLex(0),punctuation( "?...,:;\\'`(){}[]%#+-_=/!" ), 
		history(20), debug(0) {}

void Mblem::read_transtable( const string& tableName ) {
  ifstream bron( tableName.c_str() );
  if ( !bron ) {
    *Log(theErrLog) << "translation table file '" << tableName 
		    << "' appears to be missing." << endl;
    exit(1);
  }
  while( bron ){
    string className;
    string classCode;
    bron >> className;
    bron >> ws;
    bron >> classCode;
    if ( classMap.find( classCode ) == classMap.end() )
      // stupid HACK to only accept first occurence
      // multiple occurences is a NO NO i think
      classMap[classCode] = className;
    bron >> ws;
  }
  return;
}

bool Mblem::init( const Configuration& config ) {
  *Log(theErrLog) << "Initiating lemmatizer...\n";
  string db = config.lookUp( "debug", "mblem" );
  if ( !db.empty() )
    debug = stringTo<int>( db );
  
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
  myLex = new TimblAPI(opts);
  return myLex->GetInstanceBase(treeName);
}

Mblem::~Mblem(){
  //    *Log(theErrLog) << "cleaning up MBLEM stuff" << endl;
  delete myLex;
  myLex = 0;
}

string Mblem::make_instance( const UnicodeString& in ) {
  if (debug)
    cout << "making instance from: " << in << endl;
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
  string result = folia::UnicodeToUTF8(instance);
  if (debug)
    cout << "inst: " << instance << endl;
  
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
    similar( tag, cgnTag, "ott,2,ev" ) ||
    similar( tag, cgnTag, "ott,1,ev" ) ||
    similar( tag, cgnTag, "ott,1of2of3,mv" ) ||
    similar( tag, cgnTag, "ovt,1of2of3,mv" ) ||
    similar( tag, cgnTag, "ovt,1of2of3,ev" ) ||
    similar( tag, cgnTag, "ovt,3,ev" ) ||
    similar( tag, cgnTag, "ovt,2,ev" ) ||
    similar( tag, cgnTag, "ovt,1,ev" ) ||
    similar( tag, cgnTag, "ovt,1of2of3,mv" );
}
  
string Mblem::postprocess( const string& tag ){
  if ( debug ){
    cout << "\n\tlemmas: ";
    for( vector<mblemData>::const_iterator it=mblemResult.begin(); 
	 it != mblemResult.end(); ++it)
      cout << it->getLemma() << "/ "<< it->getTag()<< " ";
  }
  string res;
  size_t index = 0;
  size_t nrlookup = mblemResult.size();
  while ( index < nrlookup &&
	  !isSimilar( tag, mblemResult[index].getTag() ) ){
    ++index;
  }
  // Here index is either < nrlookup which means there is some similarity
  // between tag and  mblem[index].getTag(), 
  // or == nrlookup, which means no match
  
  if ( index == nrlookup ) {
    if (debug)
      cout << "NO CORRESPONDING TAG! " << tag << endl;
    res = mblemResult[0].getLemma();
  }
  else {
    res = mblemResult[index].getLemma();
  }
  if (debug)
    cout << "final MBLEM lemma: " << res << endl;
  return res;
} 

void Mblem::Classify( const UnicodeString& uWord ){
  mblemResult.clear();
  string inst = make_instance(uWord);  
  string classString;
  myLex->Classify( inst, classString );
  if (debug)
    cout << "class: " << classString  << endl;
  // 1st find all alternatives
  vector<string> parts;
  int numParts = split_at( classString, parts, "|" );
  if ( numParts < 1 ){
    cout << "no alternatives found" << endl;
  }
  int index = 0;
  while ( index < numParts ) {
    UnicodeString part = folia::UTF8ToUnicode( parts[index++] );
    if (debug)
      cout <<"part = " << part << endl;
    UnicodeString insstr;
    UnicodeString delstr;
    UnicodeString prefix;
    string restag;
    size_t lpos = part.indexOf("+");
    if ( lpos != string::npos )
      restag = folia::UnicodeToUTF8( UnicodeString( part, 0, lpos ) );
    else 
      restag =  folia::UnicodeToUTF8( part );
    if ( classMap.size() > 0 ){
      map<string,string>::const_iterator it = classMap.find(restag);
      if ( it != classMap.end() )
	restag = it->second;
    }
    size_t  pl = part.length();
    lpos++;
    while(lpos < pl) {
      switch( part[lpos] ) {
      case 'P': {
	if (part[lpos-1] =='+') {
	  lpos++;
	  size_t tmppos = part.indexOf("+", lpos);
	  if ( tmppos != string::npos )
	    prefix = UnicodeString( part, lpos, tmppos - lpos );
	  else 
	    prefix = UnicodeString( part, lpos );
	  if (debug)
	    cout << "prefix=" << prefix << endl;
	}
	break;
      }
      case 'D': {
	if (part[lpos-1] =='+') {
	  lpos++;
	  size_t tmppos = part.indexOf("+", lpos);
	  if ( tmppos != string::npos )
	    delstr = UnicodeString( part, lpos, tmppos - lpos );
	  else 
	    delstr = UnicodeString( part, lpos );
	  if (debug)
	    cout << "delstr=" << delstr << endl;
	}
	break;
      }
      case 'I': {
	if (part[lpos-1] =='+') {
	  lpos++;
	  size_t tmppos = part.indexOf("+", lpos);
	  if ( tmppos != string::npos )
	    insstr = UnicodeString( part, lpos, tmppos - lpos);
	  else 
	    insstr = UnicodeString( part, lpos);
	  if (debug)
	    cout << "insstr=" << insstr << endl;
	}
	break;
      }
      default:
	break;
      }
      lpos++;
    } // while lpos < pl
    
    if (debug){
      cout << "part: " << part << " split up in: " << endl;
      cout << "pre-prefix word: '" << uWord << "' prefix: '"
	   << prefix << "'" << endl;
    }	
    long prefixpos = 0;
    if ( !prefix.isEmpty() ) {
      prefixpos = uWord.indexOf(prefix);
      if (debug)
	cout << "prefixpos = " << prefixpos << endl;
      // repair cases where there's actually not a prefix present
      if (prefixpos > uWord.length()-2) {
	prefixpos = 0;
	prefix.remove();
      }
    }
    
    if (debug)
      cout << "prefixpos = " << prefixpos << endl;
    UnicodeString lemma = "";
    if (prefixpos >= 0) {
      lemma = UnicodeString( uWord, 0L, prefixpos );
      prefixpos = prefixpos + prefix.length();
    }
    if (debug)
      cout << "post prefix != 0 word: "<< uWord 
	   << " lemma: " << lemma
	   << " prefix: " << prefix
	   << " insstr: " << insstr
	   << " delstr: " << delstr
	   << " l_delstr=" << delstr.length()
	   << " l_word=" << uWord.length()
	   << endl;
    
    if ( uWord.endsWith( delstr ) ){
      if ( uWord.length() - delstr.length() > 0 ){
	UnicodeString part;
	part = UnicodeString( uWord, prefixpos, uWord.length() - delstr.length() - prefixpos );
	lemma += part + insstr;
      }
      else if ( insstr.isEmpty() ){
	// do not delete whole word
	lemma += uWord;
      }
      else {
	// but replace if possible
	lemma += insstr;
      }
    }
    else if ( lemma.isEmpty() ){
      lemma = uWord;
    }
    if ( debug )
      cout << "appending lemma " << lemma << " and tag " << restag << endl;
    mblemResult.push_back( mblemData( folia::UnicodeToUTF8(lemma), restag ) );
  } // while
  if ( debug ){
    cout << "stored lemma and tag options: " << mblemResult.size() << " lemma's and " << mblemResult.size() << " tags:\n";
    for( size_t index=0; index < mblemResult.size(); ++index ){
      cout << "lemma alt: " << mblemResult[index].getLemma() << endl;
      cout << "tag alt: " << mblemResult[index].getTag() << endl;
    }
    cout << "\n\n";
  }
}
