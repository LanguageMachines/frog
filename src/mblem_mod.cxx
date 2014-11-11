/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2014
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
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "frog/Frog.h"
#include "frog/mblem_mod.h"

using namespace std;
using namespace TiCC;
using namespace folia;

Mblem::Mblem( LogStream *logstream ):
  myLex(0),
  punctuation( "?...,:;\\'`(){}[]%#+-_=/!" ),
  history(20),
  debug(0)
{
  mblemLog = new LogStream( logstream, "mblem" );
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
  debug = 0;
  string val = config.lookUp( "debug", "mblem" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "version", "mblem" );
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

  val = config.lookUp( "set", "tagger" );
  if ( val.empty() ){
    cgn_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
  }
  else
    cgn_tagset = val;

  string transName = config.lookUp( "transFile", "mblem" );
  if ( !transName.empty() ){
    transName = prefix( config.configDir(), transName );
    read_transtable( transName );
  }
  string treeName = config.lookUp( "treeFile", "mblem"  );
  if ( treeName.empty() )
    treeName = "mblem.tree";
  treeName = prefix( config.configDir(), treeName );

  string charFile = config.lookUp( "char_filter_file", "mblem" );
  if ( charFile.empty() )
    charFile = config.lookUp( "char_filter_file" );
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new Tokenizer::UnicodeFilter();
    filter->fill( charFile );
  }

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
  delete filter;
  delete myLex;
  myLex = 0;
  delete mblemLog;
}

string Mblem::make_instance( const UnicodeString& in ) {
  if (debug)
    *Log(mblemLog) << "making instance from: " << in << endl;
  UnicodeString instance = "";
  size_t length = in.length();
  for ( size_t i=0; i < history; i++) {
    size_t j = length - history + i;
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

void Mblem::addLemma( Word *word, const string& cls ){
  KWargs args;
  args["set"]=tagset;
  args["cls"]=cls;
#pragma omp critical(foliaupdate)
  {
    try {
      word->addLemmaAnnotation( args );
    }
    catch( const exception& e ){
      *Log(mblemLog) << e.what() << " addLemma failed." << endl;
      exit(EXIT_FAILURE);
    }
  }
}

void Mblem::filterTag( const string& postag ){
  vector<mblemData>::iterator it = mblemResult.begin();
  while( it != mblemResult.end() ){
    if ( isSimilar( postag, it->getTag() ) ){
      if ( debug )
	*Log(mblemLog) << "compare cgn-tag " << postag << " with "
		       << it->getTag() << " similar! " << endl;
      ++it;
    }
    else {
      if ( debug )
	*Log(mblemLog) << "compare cgn-tag " << postag << " with "
		       << it->getTag() << " NOT similar! " << endl;
      it = mblemResult.erase(it);
    }
  }
  if ( debug && mblemResult.empty() )
    *Log(mblemLog) << "NO CORRESPONDING TAG! " << postag << endl;
}

void Mblem::makeUnique( ){
  vector<mblemData>::iterator it = mblemResult.begin();
  while( it != mblemResult.end() ){
    string lemma = it->getLemma();
    vector<mblemData>::iterator it2 = it+1;
    while( it2 != mblemResult.end() ){
      if (debug)
	*Log(mblemLog) << "compare lemma " << lemma << " with " << it2->getLemma() << " ";
      if ( lemma == it2->getLemma() ){
	if ( debug )
	  *Log(mblemLog) << "equal " << endl;
	it2 = mblemResult.erase(it2);
      }
      else {
	if ( debug )
	  *Log(mblemLog) << "NOT equal! " << endl;
	++it2;
      }
    }
    ++it;
  }
  if (debug){
    *Log(mblemLog) << "final result after filter and unique" << endl;
    for( size_t index=0; index < mblemResult.size(); ++index ){
      *Log(mblemLog) << "lemma alt: " << mblemResult[index].getLemma()
		     << "\ttag alt: " << mblemResult[index].getTag() << endl;
    }
  }
}

void Mblem::getFoLiAResult( Word *word, const UnicodeString& uWord ){
  if ( mblemResult.empty() ){
    // just return the word as a lemma
    string result = UnicodeToUTF8( uWord );
    addLemma( word, result );
  }
  else {
    vector<mblemData>::iterator it = mblemResult.begin();
    while( it != mblemResult.end() ){
      string result = it->getLemma();
      addLemma( word, result );
      ++it;
    }
  }
}


void Mblem::addDeclaration( Document& doc ) const {
  doc.declare( AnnotationType::LEMMA,
	       tagset,
	       "annotator='frog-mblem-" + version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void Mblem::Classify( Word *sword ){
  if ( sword->isinstance(PlaceHolder_t ) )
    return;
  UnicodeString word;
  string tag;
  string pos;
  string token_class;
#pragma omp critical(foliaupdate)
  {
    word = sword->text();
    pos = sword->pos();
    token_class = sword->cls();
    tag = sword->annotation<PosAnnotation>( cgn_tagset )->feat("head");
  }
  if (debug)
    *Log(mblemLog) << "Classify " << word << "(" << pos << ") ["
		   << token_class << "]" << endl;
  if ( filter )
    word = filter->filter( word );
  if ( tag == "LET" ){
    addLemma( sword, UnicodeToUTF8(word) );
    return;
  }
  else if ( tag == "SPEC" ){
    if ( pos.find("eigen") != string::npos ){
      // SPEC(deeleigen) might contain suffixes
      if ( token_class == "QUOTE-SUFFIX" ){
	addLemma( sword, UnicodeToUTF8(UnicodeString( word, 0, word.length()-1)));
      }
      else if ( token_class == "WORD-WITHSUFFIX" ){
	addLemma( sword, UnicodeToUTF8(UnicodeString( word, 0, word.length()-2)));
      }
      else {
	addLemma( sword, UnicodeToUTF8(word) );      return;
      }
    }
    else
      addLemma( sword, UnicodeToUTF8(word) );
    return;
  }
  if ( tag != "SPEC")
    word.toLower();
  Classify( word );
  filterTag( pos );
  makeUnique();
  getFoLiAResult( sword, word );
}

void Mblem::Classify( const UnicodeString& uWord ){
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
    string partS = parts[index++];
    UnicodeString lemma;
    string restag;
    string::size_type pos = partS.find("+");
    if ( pos == string::npos ){
      // nothing to edit
      restag = partS;
      lemma = uWord;
    }
    else {
      // some edit info available, like: WW(27)+Dgekomen+Ikomen
      vector<string> edits;
      size_t n = split_at( partS, edits, "+" );
      if ( n < 1 )
	throw runtime_error( "invalid editstring: " + partS );
      restag = edits[0]; // the first one is the POS tag

      UnicodeString insstr;
      UnicodeString delstr;
      UnicodeString prefix;
      for ( size_t l=1; l < edits.size(); ++l ) {
	switch ( edits[l][0] ){
	case 'P':
	  prefix = UTF8ToUnicode( edits[l].substr( 1 ) );
	  break;
	case 'I':
	  insstr = UTF8ToUnicode( edits[l].substr( 1 ) );
	  break;
	case 'D':
	  delstr =  UTF8ToUnicode( edits[l].substr( 1 ) );
	  break;
	default:
	  *Log(mblemLog) << "Error: strange value in editstring: " << edits[l] << endl;
	}
      }
      if (debug){
	*Log(mblemLog) << "pre-prefix word: '" << uWord << "' prefix: '"
		       << prefix << "'" << endl;
      }
      int prefixpos = 0;
      if ( !prefix.isEmpty() ) {
	// Whenever Toads makemblem is improved, (the infamous
	// 'tegemoetgekomen' example), this should probably
	// become prefixpos = uWord.lastIndexOf(prefix);
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
	*Log(mblemLog) << "post word: "<< uWord
		       << " lemma: " << lemma
		       << " prefix: " << prefix
		       << " delstr: " << delstr
		       << " insstr: " << insstr
		       << endl;

      if ( uWord.endsWith( delstr ) ){
	if ( uWord.length() > delstr.length() ){
	  // chop delstr from the back, but do not delete the whole word
	  if ( uWord.length() == delstr.length() + 1  && insstr.isEmpty() ){
	    // do not mangle short words too
	    lemma += uWord;
	  }
	  else {
	    UnicodeString part = UnicodeString( uWord, prefixpos, uWord.length() - delstr.length() - prefixpos );
	    lemma += part + insstr;
	  }
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
}

vector<pair<string,string> > Mblem::getResult() const {
  vector<pair<string,string> > result;
  for ( size_t i=0; i < mblemResult.size(); ++i ){
    result.push_back( make_pair( mblemResult[i].getLemma(),
				 mblemResult[i].getTag() ) );
  }
  return result;
}
