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

#define LOG *TiCC::Log(mblemLog)

Mblem::Mblem( TiCC::LogStream *logstream ):
  myLex(0),
  punctuation( "?...,:;\\'`(){}[]%#+-_=/!" ),
  history(20),
  debug(0),
  keep_case( false ),
  filter(0)
{
  mblemLog = new TiCC::LogStream( logstream, "mblem" );
}

bool Mblem::fill_ts_map( const string& file ){
  ifstream is( file );
  if ( !is ){
    LOG << "Unable to open file: '" << file << "'" << endl;
    return false;
  }
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> parts;
    if ( TiCC::split( line, parts ) != 3 ){
      LOG << "invalid line in: '" << file << "' (expected 3 parts)" << endl;
      return false;
    }
    token_strip_map[parts[0]].insert( make_pair( parts[1], TiCC::stringTo<int>( parts[2] ) ) );
  }
  if ( debug ){
    LOG << "read token strip rules from: '" << file << "'" << endl;
  }
  return true;
}

bool Mblem::init( const TiCC::Configuration& config ) {
  LOG << "Initiating lemmatizer..." << endl;
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
    cerr << "usage of a mblem transFile is no longer needed!" << endl;
    cerr << "skipping : " << transName << endl;
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

  string one_one_tagS = config.lookUp( "one_one_tags", "mblem" );
  if ( !one_one_tagS.empty() ){
    vector<string> tags = TiCC::split_at( one_one_tagS, "," );
    for ( auto const& t : tags ){
      one_one_tags.insert( t );
    }
  }

  string par = config.lookUp( "keep_case", "mblem" );
  if ( !par.empty() ){
    keep_case = TiCC::stringTo<bool>( par );
  }

  string cls = config.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }

  string opts = config.lookUp( "timblOpts", "mblem" );
  if ( opts.empty() )
    opts = "-a1";
  //make it silent
  opts += " +vs -vf -F TABBED";
  //Read in (igtree) data
  myLex = new Timbl::TimblAPI(opts);
  return myLex->GetInstanceBase(treeName);
}

Mblem::~Mblem(){
  //    LOG << "cleaning up MBLEM stuff" << endl;
  delete filter;
  delete myLex;
  myLex = 0;
  delete mblemLog;
}

string Mblem::make_instance( const UnicodeString& in ) {
  if (debug) {
    LOG << "making instance from: " << in << endl;
  }
  UnicodeString instance = "";
  size_t length = in.length();
  for ( size_t i=0; i < history; i++) {
    size_t j = length - history + i;
    if (( i < history - length ) &&
	(length<history))
      instance += "=\t";
    else {
      instance += in[j];
      instance += '\t';
    }
  }
  instance += "?";
  string result = folia::UnicodeToUTF8(instance);
  if ( debug ){
    LOG << "inst: " << instance << endl;
  }
  return result;
}

void Mblem::addLemma( folia::Word *word, const string& cls ){
  folia::KWargs args;
  args["set"]=tagset;
  args["class"]=cls;
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }

#pragma omp critical (foliaupdate)
  {
    try {
      word->addLemmaAnnotation( args );
    }
    catch( const exception& e ){
      LOG << e.what() << " addLemma failed." << endl;
      throw;
    }
  }
}

void Mblem::filterTag( const string& postag ){
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    string tag = it->getTag();
    if ( postag == tag ){
      if ( debug ){
	LOG << "compare cgn-tag " << postag << " with mblem-tag " << tag
		       << "\n\t==> identical tags"  << endl;
      }
      ++it;
    }
    else {
      if ( debug ){
	LOG << "compare cgn-tag " << postag << " with mblem-tag " << tag
		       << "\n\t==> different tags" << endl;
      }
      it = mblemResult.erase(it);
    }
  }
  if ( debug && mblemResult.empty() ){
    LOG << "NO CORRESPONDING TAG! " << postag << endl;
  }
}

void Mblem::makeUnique( ){
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    string lemma = it->getLemma();
    auto it2 = it+1;
    while( it2 != mblemResult.end() ){
      if (debug){
	LOG << "compare lemma " << lemma << " with " << it2->getLemma() << " ";
      }
      if ( lemma == it2->getLemma() ){
	if ( debug ){
	  LOG << "equal " << endl;
	}
	it2 = mblemResult.erase(it2);
      }
      else {
	if ( debug ){
	  LOG << "NOT equal! " << endl;
	}
	++it2;
      }
    }
    ++it;
  }
  if (debug){
    LOG << "final result after filter and unique" << endl;
    for ( const auto& mbr : mblemResult ){
      LOG << "lemma alt: " << mbr.getLemma()
	  << "\ttag alt: " << mbr.getTag() << endl;
    }
  }
}

void Mblem::getFoLiAResult( folia::Word *word, const UnicodeString& uWord ){
  if ( mblemResult.empty() ){
    // just return the word as a lemma
    string result = folia::UnicodeToUTF8( uWord );
    addLemma( word, result );
  }
  else {
    for ( auto const& it : mblemResult ){
      string result = it.getLemma();
      addLemma( word, result );
    }
  }
}


void Mblem::addDeclaration( folia::Document& doc ) const {
#pragma omp critical (foliaupdate)
  {
    doc.declare( folia::AnnotationType::LEMMA,
		 tagset,
		 "annotator='frog-mblem-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void Mblem::Classify( folia::Word *sword ){
  if ( sword->isinstance( folia::PlaceHolder_t ) )
    return;
  UnicodeString uword;
  string pos;
  string token_class;
#pragma omp critical (foliaupdate)
  {
    uword = sword->text( textclass );
    pos = sword->pos();
    string txtcls = sword->textclass();
    if ( txtcls == textclass ){
      // so only use the word class is the textclass of the word
      // matches the wanted text
      token_class = sword->cls();
    }
  }
  if (debug){
    LOG << "Classify " << uword << "(" << pos << ") ["
	<< token_class << "]" << endl;
  }
  if ( filter )
    uword = filter->filter( uword );

  if ( token_class == "ABBREVIATION" ){
    // We dont handle ABBREVIATION's so just take the word as such
    string word = folia::UnicodeToUTF8(uword);
    addLemma( sword, word );
    return;
  }
  auto const& it1 = token_strip_map.find( pos );
  if ( it1 != token_strip_map.end() ){
    // some tag/tokenizer_class combinations are special
    // we have to strip a few letters to get a lemma
    auto const& it2 = it1->second.find( token_class );
    if ( it2 != it1->second.end() ){
      UnicodeString uword2 = UnicodeString( uword, 0, uword.length() - it2->second );
      if ( uword2.isEmpty() ){
	uword2 = uword;
      }
      string word = folia::UnicodeToUTF8(uword2);
      addLemma( sword, word );
      return;
    }
  }
  if ( one_one_tags.find(pos) != one_one_tags.end() ){
    // some tags are just taken as such
    string word = folia::UnicodeToUTF8(uword);
    addLemma( sword, word );
    return;
  }
  if ( !keep_case ){
    uword.toLower();
  }
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
  if ( debug){
    LOG << "class: " << classString  << endl;
  }
  // 1st find all alternatives
  vector<string> parts;
  int numParts = TiCC::split_at( classString, parts, "|" );
  if ( numParts < 1 ){
    LOG << "no alternatives found" << endl;
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
      size_t n = TiCC::split_at( partS, edits, "+" );
      if ( n < 1 )
	throw runtime_error( "invalid editstring: " + partS );
      restag = edits[0]; // the first one is the POS tag

      UnicodeString insstr;
      UnicodeString delstr;
      UnicodeString prefix;
      for ( const auto& edit : edits ){
	if ( edit == edits.front() ){
	  continue;
	}
	switch ( edit[0] ){
	case 'P':
	  prefix = folia::UTF8ToUnicode( edit.substr( 1 ) );
	  break;
	case 'I':
	  insstr = folia::UTF8ToUnicode( edit.substr( 1 ) );
	  break;
	case 'D':
	  delstr = folia::UTF8ToUnicode( edit.substr( 1 ) );
	  break;
	default:
	  LOG << "Error: strange value in editstring: " << edit
			 << endl;
	}
      }
      if (debug){
	LOG << "pre-prefix word: '" << uWord << "' prefix: '"
		       << prefix << "'" << endl;
      }
      int prefixpos = 0;
      if ( !prefix.isEmpty() ) {
	// Whenever Toads makemblem is improved, (the infamous
	// 'tegemoetgekomen' example), this should probably
	// become prefixpos = uWord.lastIndexOf(prefix);
	prefixpos = uWord.indexOf(prefix);
	if (debug){
	  LOG << "prefixpos = " << prefixpos << endl;
	}
	// repair cases where there's actually not a prefix present
	if (prefixpos > uWord.length()-2) {
	  prefixpos = 0;
	  prefix.remove();
	}
      }

      if (debug){
	LOG << "prefixpos = " << prefixpos << endl;
      }
      if (prefixpos >= 0) {
	lemma = UnicodeString( uWord, 0L, prefixpos );
	prefixpos = prefixpos + prefix.length();
      }
      if (debug){
	LOG << "post word: "<< uWord
	    << " lemma: " << lemma
	    << " prefix: " << prefix
	    << " delstr: " << delstr
	    << " insstr: " << insstr
	    << endl;
      }
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
      if ( debug ){
	LOG << "looking up " << restag << endl;
      }
      if ( it != classMap.end() ){
	restag = it->second;
	if ( debug ){
	  LOG << "found " << restag << endl;
	}
      }
      else {
	LOG << "problem: found no translation for "
	    << restag << " using it 'as-is'" << endl;
      }
    }
    if ( debug ){
      LOG << "appending lemma " << lemma << " and tag " << restag << endl;
    }
    mblemResult.push_back( mblemData( folia::UnicodeToUTF8(lemma), restag ) );
  } // while
  if ( debug ) {
    LOG << "stored lemma and tag options: " << mblemResult.size()
	<< " lemma's and " << mblemResult.size() << " tags:" << endl;
    for ( const auto& mbr : mblemResult ){
      LOG << "lemma alt: " << mbr.getLemma()
	  << "\ttag alt: " << mbr.getTag() << endl;
    }
  }
}

vector<pair<string,string> > Mblem::getResult() const {
  vector<pair<string,string> > result;
  for ( const auto& mbr : mblemResult ){
    result.push_back( make_pair( mbr.getLemma(),
				 mbr.getTag() ) );
  }
  return result;
}
