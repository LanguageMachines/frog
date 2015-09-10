/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
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
  ifstream bron( tableName );
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

bool Mblem::fill_eq_set( const string& file ){
  ifstream is( file );
  if ( !is ){
    *Log(mblemLog) << "Unable to open file: '" << file << "'" << endl;    
    return false;
  }
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' )
      continue;
    line = TiCC::trim(line);
    equiv_set.insert( line );
  }
  if ( debug ){
    *Log(mblemLog) << "read tag-equivalents from: '" << file << "'" << endl;    
  }
  return true;
}

bool Mblem::fill_ts_map( const string& file ){
  ifstream is( file );
  if ( !is ){
    *Log(mblemLog) << "Unable to open file: '" << file << "'" << endl;    
    return false;
  }
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> parts;
    if ( TiCC::split( line, parts ) != 3 ){
      *Log(mblemLog) << "invalid line in: '" << file << "' (expected 3 parts)" << endl;
      return false;
    }
    token_strip_map[parts[0]].insert( make_pair( parts[1], TiCC::stringTo<int>( parts[2] ) ) );
  }
  if ( debug ){
    *Log(mblemLog) << "read uct token strip rules from: '" << file << "'" << endl;    
  }
  return true;
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
    POS_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
  }
  else
    POS_tagset = val;

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

  string tokenStripFile = config.lookUp( "token_strip_file", "mblem" );
  if ( !tokenStripFile.empty() ){
    tokenStripFile = prefix( config.configDir(), tokenStripFile );
    if ( !fill_ts_map( tokenStripFile ) )
      return false;
  }
   
  string equiv_file = config.lookUp( "equivalents_file", "mblem" );
  if ( !equiv_file.empty() ){
    equiv_file = prefix( config.configDir(), equiv_file );
    if ( !fill_eq_set( equiv_file ) )
      return false;
  }

  string one_one_tagS = config.lookUp( "one_one_tags", "mblem" );
  if ( !one_one_tagS.empty() ){
    vector<string> tags;
    TiCC::split_at( one_one_tagS, tags, "," );
    for ( auto const& t : tags ){
      one_one_tags.insert( t );
    }
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

bool equivalent( const string& tag, 
		 const string& lookuptag,
		 const string& part ){
  return tag.find( part ) != string::npos &&
    lookuptag.find( part ) != string::npos ;
}

bool isSimilar( const string& tag, const string& cgnTag, const set<string>& eq_s ){
  for ( auto const& part : eq_s ){
    if ( equivalent( tag, cgnTag, part ) )
      return true;
  }
  return false;
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
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    string tag = it->getTag();
    if ( postag == tag ){
      if ( debug ){
	*Log(mblemLog) << "compare cgn-tag " << postag << " with mblem-tag " << tag
		       << "\n\t==> identical tags"  << endl;
      }
      ++it;
    }
    else if ( isSimilar( postag, tag, equiv_set ) ){
      if ( debug ){
	*Log(mblemLog) << "compare cgn-tag " << postag << " with mblem-tag " << tag
		       << "\n\t==> similar tags" << endl;
      }
      ++it;
    }
    else {
      if ( debug ){
	*Log(mblemLog) << "compare cgn-tag " << postag << " with mblem-tag " << tag
		       << "\n\t==> different tags" << endl;
      }
      it = mblemResult.erase(it);
    }
  }
  if ( debug && mblemResult.empty() )
    *Log(mblemLog) << "NO CORRESPONDING TAG! " << postag << endl;
}

void Mblem::makeUnique( ){
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    string lemma = it->getLemma();
    auto it2 = it+1;
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
    for ( auto const& it : mblemResult ){
      string result = it.getLemma();
      addLemma( word, result );
    }
  }
}


void Mblem::addDeclaration( Document& doc ) const {
#pragma omp critical (foliaupdate)
  {
    doc.declare( AnnotationType::LEMMA,
		 tagset,
		 "annotator='frog-mblem-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void Mblem::Classify( Word *sword ){
  if ( sword->isinstance(PlaceHolder_t ) )
    return;
  UnicodeString uword;
  string pos;
  string token_class;
#pragma omp critical(foliaupdate)
  {
    uword = sword->text();
    pos = sword->pos();
    token_class = sword->cls();
  }
  if (debug)
    *Log(mblemLog) << "Classify " << uword << "(" << pos << ") ["
		   << token_class << "]" << endl;

  if ( filter )
    uword = filter->filter( uword );

  auto const& it1 = token_strip_map.find( pos );
  if ( it1 != token_strip_map.end() ){
    auto const& it2 = it1->second.find( token_class );
    if ( it2 != it1->second.end() ){
      uword = UnicodeString( uword, 0, uword.length() - it2->second );
      string word = UnicodeToUTF8(uword);
      addLemma( sword, word );
      return;
    }
  }
  if ( one_one_tags.find(pos) != one_one_tags.end() ){
    string word = UnicodeToUTF8(uword);
    addLemma( sword, word );
    return;
  }

  uword.toLower();
  Classify( uword );
  filterTag( pos );
  makeUnique();
  getFoLiAResult( sword, uword );
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
      auto const& it = classMap.find(restag);
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
