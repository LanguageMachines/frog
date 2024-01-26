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

#include "frog/mwu_chunker_mod.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <map>

#include "timbl/TimblAPI.h"
#include "ticcutils/PrettyPrint.h"

#include "frog/Frog-util.h" // defines etc.

using namespace std;
using TiCC::operator<<;
using icu::UnicodeString;

#define LOG *TiCC::Log(errLog)
#define DBG *TiCC::Log(dbgLog)

mwuAna::mwuAna( const UnicodeString& wrd,
		bool glue_tag,
		size_t index ):
  mwu_start( index ),
  mwu_end( index ),
  word(wrd),
  spec(glue_tag)
{
  /// create a mwu Analysis record
  /*!
    \param wrd The text of the word
    \param glue_tag was a \e 'glue' tag detected?
    \param index The (initial) position in the sentence
   */
}

void mwuAna::merge( const mwuAna *add ){
  /// merge two mwuAna records
  /*
    \param add the mwuAna to merge.

    in fact we only take over the endpostion of add the other fields are
    of no interest more, as the words are already resolved
   */
  mwu_end = add->mwu_end;
  delete add;
}

Mwu::Mwu( TiCC::LogStream *err_log, TiCC::LogStream *dbg_log ){
  /// create a Mwu record (UNINITIALIZED yet)
  /*!
    \param err_log The LogStream for errors
    \param dbg_log The LogStream for debugging

    a call to Mwu::init() is needed to be able to use the Mwu classifier
   */
  errLog = new TiCC::LogStream( err_log, "mwu-" );
  dbgLog = new TiCC::LogStream( dbg_log, "mwu-" );
  filter = 0;
}

Mwu::~Mwu(){
  /// erase an Mwu record
  reset();
  delete errLog;
  delete dbgLog;
  delete filter;
}

void Mwu::reset(){
  /// clear an Mwu record
  for ( const auto& it : mWords ){
    delete it;
  }
  mWords.clear();
}

void Mwu::add( const frog_record& fd ){
  /// add a mwuAna record to this Mwu
  /*!
    \param fd The frog_data structure with the information to use
   */
  icu::UnicodeString word = fd.word;
  if ( filter ){
    word = filter->filter( word );
  }
  bool glue = ( fd.tag == glue_tag );
  size_t index = mWords.size();
  mWords.push_back( new mwuAna( word, glue, index ) );
}

bool Mwu::read_mwus( const string& fname) {
  /// fill our table with MWU's
  /*!
    \param fname the file to reaf from
   */
  LOG << "read mwus " + fname << endl;
  ifstream mwufile(fname, ios::in);
  if ( !mwufile ){
    LOG << "reading of " << fname << " FAILED" << endl;
    return false;
  }
  UnicodeString line;
  while( TiCC::getline( mwufile, line ) ) {
    vector<UnicodeString> res1 = TiCC::split_at(line, " ");
    if ( res1.size() == 2 ){
      vector<UnicodeString> res2 = TiCC::split_at(res1[0], "_");;
      //res1 has mwus and tags, res2 has ind. words
      if ( res2.size() >= 2 ){
	UnicodeString key = res2[0];
	res2.erase(res2.begin());
	MWUs.insert( make_pair( key, res2 ) );
      }
      else {
	LOG << "invalid entry in MWU file " << line << endl;
	return false;
      }
    }
    else {
      LOG << "invalid entry in MWU file " << line << endl;
      return false;
    }
  }
  return true;
}

bool Mwu::init( const TiCC::Configuration& config ) {
  /// initialize the Mwu using a Configuration structure
  /*!
    \param config the configuration to use
   */
  LOG << "initiating mwuChunker..." << endl;
  debug = 0;
  string val = config.lookUp( "debug", "mwu" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "t", "mwu" );
  if ( val.empty() ){
    LOG << "cannot find attribute 't' in configfile" << endl;
    return false;
  }
  mwuFileName = prefix( config.configDir(), val );
  if ( !read_mwus(mwuFileName) ) {
    LOG << "Cannot read mwu file " << mwuFileName << endl;
    return false;
  }
  val = config.lookUp( "version", "mwu" );
  if ( val.empty() ){
    _version = "1.0";
  }
  else {
    _version = val;
  }
  val = config.lookUp( "set", "mwu" );
  if ( val.empty() ){
    mwu_tagset = "http://ilk.uvt.nl/folia/sets/frog-mwu-nl";
  }
  else {
    mwu_tagset = val;
  }
  string charFile = config.lookUp( "char_filter_file", "tagger" );
  if ( charFile.empty() ){
    charFile = config.lookUp( "char_filter_file" );
  }
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new TiCC::UniFilter();
    filter->fill( charFile );
  }
  val = config.lookUp( "gluetag", "mwu" );
  if ( val.empty() ){
    glue_tag = "SPEC(deeleigen)";
  }
  else {
    glue_tag = TiCC::UnicodeFromUTF8(val);
  }

  string cls = config.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }

  return true;
}

using TiCC::operator<<;

ostream &operator<<( ostream& os, const Mwu& mwu ){
  /// output an Mwu (debugging only)
  for ( size_t i = 0; i < mwu.mWords.size(); ++i ){
    os << i+1 << "\t" << mwu.mWords[i]->getWord()
       <<  "(" << mwu.mWords[i]->mwu_start << "," << mwu.mWords[i]->mwu_end
       << ")" << endl;
  }
  return os;
}

void Mwu::add_provenance( folia::Document& doc,
			    folia::processor *main ) const {
  /// add provenance information for the tokenizer. (FoLiA output only)
  /*!
    \param doc the FoLiA document to add to
    \param main the processor to use (presumably the Frog processor)
  */
  string _label = "mwu";
  if ( !main ){
    throw logic_error( "mwu::add_provenance() without arguments." );
  }
  folia::KWargs args;
  args["name"] = _label;
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  args.clear();
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::ENTITY, mwu_tagset, args );
}

void Mwu::Classify( frog_data& sent ){
  /// run the Mwu classifier on a sentence in frog_data format
  /*!
    \param sent a frog_data structure with unresolved MWU's
   */
  reset();
  // setup the Mwu
  for ( auto& word : sent.units ){
    add( word );
  }
  // collect mwu's
  Classify();
  //  take over the mwu postions in the frog_data
  for ( const auto& mword : mWords ){
    if ( mword->mwu_start != mword->mwu_end ){
      sent.mwus[mword->mwu_start] = mword->mwu_end;
    }
  }
  sent.resolve_mwus();
}

UnicodeString decap( const UnicodeString& word ){
  UnicodeString result;
  for ( int i=0; i < word.length(); ++i ){
    if ( i == 0 ){
      result += u_tolower(word[0]);
    }
    else {
      result += word[i];
    }
  }
  return result;
}

void Mwu::Classify(){
  /// examine the Mwu's internal mwuAna nodes to determine the spans of
  /// all mwu's found
  /*!
    First we collect all 'glue tags' into MWU's

    Second we do a lookup of the words in our table and keep the longest match
   */
  if ( debug > 1 ) {
    DBG << "Starting mwu Classify" << endl;
  }
  mymap2::iterator best_match;
  size_t matchLength = 0;
  size_t max = mWords.size();

  // add all current sequences of the glue_tag words to MWUs
  for ( size_t i=0; i < max-1; ++i ) {
    if ( mWords[i]->isSpec() && mWords[i+1]->isSpec() ) {
      vector<UnicodeString> newmwu;
      while ( i < max && mWords[i]->isSpec() ){
	newmwu.push_back(mWords[i]->getWord());
	i++;
      }
      UnicodeString key = newmwu[0];
      newmwu.erase( newmwu.begin() );
      MWUs.insert( make_pair(key, newmwu) );
    }
  }
  size_t i;
  for ( i = 0; i < max; i++) {
    UnicodeString word = mWords[i]->getWord();
    if ( debug > 1 ){
      DBG << "checking word[" << i <<"]: " << word << endl;
    }
    auto matches = MWUs.equal_range(word);
    if ( i == 0
	 && matches.first == matches.second ) {
      // no match on first word. try decaped version.
      // we do this ONLY for the very first word in the sentence!
      word = decap( word );
      if ( debug > 1 ){
     	DBG << "checking decapped word [" << i <<"]: " << word << endl;
      }
      matches = MWUs.equal_range(word);
    }
    if ( matches.first != matches.second ) {
      //match
      auto current_match = matches.first;
      if (  debug > 1 ) {
	DBG << "MWU: match found for " << word << endl;
      }
      while( current_match != matches.second
	     && current_match != MWUs.end() ){
	vector<UnicodeString> match = current_match->second;
	size_t max_match = match.size();
	size_t j = 0;
	if ( debug > 1 ){
	  DBG << "checking " << max_match << " matches:" << endl;
	}
	for (; i + j + 1 < max && j < max_match; j++) {
	  if ( match[j] != mWords[i+j+1]->getWord() ) {
	    if ( debug > 1){
	      DBG << "match " << j <<" (" << match[j]
			   << ") doesn't match with word " << i+ j + 1
			   << " (" << mWords[i+j + 1]->getWord() <<")" << endl;
	    }
	    // mismatch in jth word of current mwu
	    break;
	  }
	  else if ( debug > 1 ){
	    DBG << " matched " <<  mWords[i+j+1]->getWord()
			 << " j=" << j << endl;
	  }

	}
	if (j == max_match && j > matchLength ){
	  // a match. remember this!
	  best_match = current_match;
	  matchLength = j;
	}
	++current_match;
      } // while
      if( debug > 1){
	if (matchLength >0 ) {
	  DBG << "MWU: found match starting with " << (*best_match).first << endl;
	}
	else {
	  DBG <<"MWU: no match" << endl;
	}
      }
      // we found a matching mwu, break out of the loop
      // do useful stuff, and recurse to find more mwus
      if ( matchLength > 0 ){
	break;
      }
    } //match found
    else {
      if( debug > 1 ) {
	DBG <<"MWU:check: no match" << endl;
      }
    }
  } //for (i < max)
  if (matchLength > 0 ) {
    //concat
    if ( debug >1 ){
      DBG << "mwu found, processing" << endl;
    }
    for ( size_t j = 1; j <= matchLength; ++j) {
      if ( debug > 1 ){
	DBG << "concat " << mWords[i+j]->getWord() << endl;
      }
      mWords[i]->merge( mWords[i+j] );
    }
    auto anatmp1 = mWords.begin() + i;
    auto anatmp2 = ++anatmp1 + matchLength;
    mWords.erase(anatmp1, anatmp2);
    if ( debug > 1 ){
      DBG << "tussenstand:" << endl;
      DBG << *this << endl;
    }
    Classify( );
  } //if (matchLength)
  return;
} // //Classify

void Mwu::add_result( const frog_data& fd,
		      const vector<folia::Word*>& wv ) const {
  /// add the mwu's in \e fd as Entities to the parent of the \e wv list of Word
  /*!
    \param fd The tagged results
    \param wv The folia:Word vector
  */
  folia::Sentence *s = wv[0]->sentence();
  folia::KWargs args;
  if ( !s->id().empty() ){
    args["generate_id"] = s->id();
  }
  args["set"] = getTagset();
  folia::EntitiesLayer *el;
#pragma omp critical (foliaupdate)
  {
    el = s->add_child<folia::EntitiesLayer>( args );
  }
  for ( const auto& mwu : fd.mwus ){
    if ( !el->id().empty() ){
      args["generate_id"] = el->id();
    }
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
#pragma omp critical (foliaupdate)
    {
      folia::Entity *e = el->add_child<folia::Entity>( args );
      for ( size_t pos = mwu.first; pos <= mwu.second; ++pos ){
	e->append( wv[pos] );
      }
    }
  }
}
