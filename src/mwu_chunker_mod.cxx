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

#define LOG *TiCC::Log(mwuLog)

mwuAna::mwuAna( folia::Word *fwrd,
		const string& txt,
		const string& glue_tag,
		size_t index ){
  spec = false;
  word = txt;
  string tag;
  if ( fwrd ){
#pragma omp critical (foliaupdate)
    {
      tag = fwrd->annotation<folia::PosAnnotation>()->cls();
    }
  }
  spec = ( tag == glue_tag );
  fwords.push_back( fwrd );
  mwu_start = mwu_end = index;
}

mwuAna::mwuAna( const string& txt,
		const string& tag,
		const string& glue_tag,
		size_t index ){
  spec = false;
  word = txt;
  spec = ( tag == glue_tag );
  mwu_start = mwu_end = index;
}

void mwuAna::merge( const mwuAna *add ){
  if ( !add->fwords.empty() ){
    fwords.push_back( add->fwords[0] );
  }
  mwu_end = add->mwu_end;
  delete add;
}

folia::EntitiesLayer *mwuAna::addEntity( const string& tagset,
					 const string& textclass,
					 folia::Sentence *sent,
					 folia::EntitiesLayer *el ){
  if ( fwords.size() > 1 ){
    if ( el == 0 ){
#pragma omp critical (foliaupdate)
      {
	folia::KWargs args;
	string s_id = sent->id();
	if ( ! s_id.empty() ){
	  args["generate_id"] = s_id;
	}
	args["set"] = tagset;
	el = new folia::EntitiesLayer( args, sent->doc() );
	sent->append( el );
      }
    }
    folia::KWargs args;
    args["set"] = tagset;
    string el_id = el->id();
    if ( ! el_id.empty() ){
      args["generate_id"] = el_id;
    }
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
    folia::Entity *e=0;
#pragma omp critical (foliaupdate)
    {
      e = new folia::Entity( args, el->doc() );
      el->append( e );
    }
    for ( const auto& fw : fwords ){
#pragma omp critical (foliaupdate)
      {
	e->append( fw );
      }
    }
  }
  return el;
}

Mwu::Mwu( TiCC::LogStream * logstream ){
  mwuLog = new TiCC::LogStream( logstream, "mwu-" );
  filter = 0;
}

Mwu::~Mwu(){
  reset();
  delete mwuLog;
  delete filter;
}

void Mwu::reset(){
  for ( const auto& it : mWords ){
    delete it;
  }
  mWords.clear();
}

void Mwu::add( folia::Word *word, size_t index ){
  icu::UnicodeString tmp;
#pragma omp critical (foliaupdate)
  {
    tmp = word->text( textclass );
  }
  if ( filter )
    tmp = filter->filter( tmp );
  string txt = TiCC::UnicodeToUTF8( tmp );
  mWords.push_back( new mwuAna( word, txt, glue_tag, index ) );
}

void Mwu::add( frog_record& fd, size_t index ){
  icu::UnicodeString tmp;
#pragma omp critical (dataupdate)
  {
    tmp = TiCC::UnicodeFromUTF8(fd.word);
  }
  if ( filter )
    tmp = filter->filter( tmp );
  string txt = TiCC::UnicodeToUTF8( tmp );
  mWords.push_back( new mwuAna( txt, fd.tag, glue_tag, index ) );
}

bool Mwu::read_mwus( const string& fname) {
  LOG << "read mwus " + fname << endl;
  ifstream mwufile(fname, ios::in);
  if(mwufile.bad()){
    return false;
  }
  string line;
  while( getline( mwufile, line ) ) {
    vector<string> res1, res2; //res1 has mwus and tags, res2 has ind. words
    if ( ( TiCC::split_at(line, res1, " ") == 2 ) &&
	 ( TiCC::split_at(res1[0], res2, "_") >= 2 ) ){
      string key = res2[0];
      res2.erase(res2.begin());
      MWUs.insert( make_pair( key, res2 ) );
    }
    else {
      LOG << "invalid entry in MWU file " << line << endl;
      return false;
    }
  }
  return true;
}

bool Mwu::init( const TiCC::Configuration& config ) {
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
  if ( charFile.empty() )
    charFile = config.lookUp( "char_filter_file" );
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
    glue_tag = val;
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
  for ( size_t i = 0; i < mwu.mWords.size(); ++i )
    os << i+1 << "\t" << mwu.mWords[i]->getWord()
       <<  "(" << mwu.mWords[i]->mwu_start << "," << mwu.mWords[i]->mwu_end
       << ")" << endl;
  return os;
}

void Mwu::addDeclaration( folia::Document& doc ) const {
  doc.declare( folia::AnnotationType::ENTITY,
	       mwu_tagset,
	       "annotator='frog-mwu-" + _version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void Mwu::addDeclaration( folia::Processor& proc ) const {
  proc.declare( folia::AnnotationType::ENTITY,
	       mwu_tagset,
	       "annotator='frog-mwu-" + _version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void Mwu::Classify( frog_data& sent ){
  reset();
  size_t id=0;
  for ( auto& word : sent.units ){
    add( word, id++ );
  }
  Classify();
  for ( const auto& mword : mWords ){
    if ( mword->mwu_start != mword->mwu_end ){
      sent.mwus[mword->mwu_start] = mword->mwu_end;
    }
  }
  sent.resolve_mwus();
}

void Mwu::Classify( const vector<folia::Word*>& words ){
  if ( words.empty() ){
    return;
  }
  reset();
  size_t id=0;
  for ( const auto& word : words ){
    add( word, id++  );
  }
  Classify();
  folia::EntitiesLayer *el = 0;
  folia::Sentence *sent;
#pragma omp critical (foliaupdate)
  {
    sent = words[0]->sentence();
  }
  for ( const auto& mword : mWords ){
    el = mword->addEntity( mwu_tagset, textclass, sent, el );
  }
}

void Mwu::Classify(){
  if ( debug > 1 ) {
    LOG << "Starting mwu Classify" << endl;
  }
  mymap2::iterator best_match;
  size_t matchLength = 0;
  size_t max = mWords.size();

  // add all current sequences of the glue_tag words to MWUs
  for ( size_t i=0; i < max-1; ++i ) {
    if ( mWords[i]->isSpec() && mWords[i+1]->isSpec() ) {
      vector<string> newmwu;
      while ( i < max && mWords[i]->isSpec() ){
	newmwu.push_back(mWords[i]->getWord());
	i++;
      }
      string key = newmwu[0];
      newmwu.erase( newmwu.begin() );
      MWUs.insert( make_pair(key, newmwu) );
    }
  }
  size_t i;
  for ( i = 0; i < max; i++) {
    string word = mWords[i]->getWord();
    if ( debug > 1 ){
      LOG << "checking word[" << i <<"]: " << word << endl;
    }
    const auto matches = MWUs.equal_range(word);
    if ( matches.first != MWUs.end() ) {
      //match
      auto current_match = matches.first;
      if (  debug > 1 ) {
	LOG << "MWU: match found for " << word << endl;
      }
      while( current_match != matches.second
	     && current_match != MWUs.end() ){
	vector<string> match = current_match->second;
	size_t max_match = match.size();
	size_t j = 0;
	if ( debug > 1 ){
	  LOG << "checking " << max_match << " matches:" << endl;
	}
	for (; i + j + 1 < max && j < max_match; j++) {
	  if ( match[j] != mWords[i+j+1]->getWord() ) {
	    if ( debug > 1){
	      LOG << "match " << j <<" (" << match[j]
			   << ") doesn't match with word " << i+ j + 1
			   << " (" << mWords[i+j + 1]->getWord() <<")" << endl;
	    }
	    // mismatch in jth word of current mwu
	    break;
	  }
	  else if ( debug > 1 ){
	    LOG << " matched " <<  mWords[i+j+1]->getWord()
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
	  LOG << "MWU: found match starting with " << (*best_match).first << endl;
	}
	else {
	  LOG <<"MWU: no match" << endl;
	}
      }
      // we found a matching mwu, break out of loop thru sentence,
      // do useful stuff, and recurse to find more mwus
      if ( matchLength > 0 ){
	break;
      }
    } //match found
    else {
      if( debug > 1 )
	LOG <<"MWU:check: no match" << endl;
    }
  } //for (i < max)
  if (matchLength > 0 ) {
    //concat
    if ( debug >1 ){
      LOG << "mwu found, processing" << endl;
    }
    for ( size_t j = 1; j <= matchLength; ++j) {
      if ( debug > 1 ){
	LOG << "concat " << mWords[i+j]->getWord() << endl;
      }
      mWords[i]->merge( mWords[i+j] );
    }
    vector<mwuAna*>::iterator anatmp1 = mWords.begin() + i;
    vector<mwuAna*>::iterator anatmp2 = ++anatmp1 + matchLength;
    mWords.erase(anatmp1, anatmp2);
    if ( debug > 1){
      LOG << "tussenstand:" << endl;
      LOG << *this << endl;
    }
    Classify( );
  } //if (matchLength)
  return;
} // //Classify

void Mwu::add_result( folia::Sentence *s,
		      const frog_data& fd,
		      const vector<folia::Word*>& wv ) const {
  folia::KWargs args;
  args["generate_id"] = s->id();
  args["set"] = getTagset();
  folia::EntitiesLayer *el = new folia::EntitiesLayer( args, s->doc() );
  s->append( el );
  for ( const auto& mwu : fd.mwus ){
    args["generate_id"] = el->id();
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
    folia::Entity *e = new folia::Entity( args, s->doc() );
    el->append( e );
    for ( size_t pos = mwu.first; pos <= mwu.second; ++pos ){
      e->append( wv[pos] );
    }
  }
}

void Mwu::add_result( const frog_data& fd,
		      const vector<folia::Word*>& wv ) const {
  folia::Sentence *s = wv[0]->sentence();
  folia::KWargs args;
  args["generate_id"] = s->id();
  args["set"] = getTagset();
  folia::EntitiesLayer *el = new folia::EntitiesLayer( args, s->doc() );
  s->append( el );
  for ( const auto& mwu : fd.mwus ){
    args["generate_id"] = el->id();
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
    folia::Entity *e = new folia::Entity( args, s->doc() );
    el->append( e );
    for ( size_t pos = mwu.first; pos <= mwu.second; ++pos ){
      e->append( wv[pos] );
    }
  }
}
