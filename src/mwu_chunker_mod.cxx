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

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <map>

#include "timbl/TimblAPI.h"

#include "frog/Frog.h" // defines etc.
#include "frog/mwu_chunker_mod.h"

using namespace std;
using namespace TiCC;
using namespace folia;

mwuAna::mwuAna( Word *fwrd, const string& glue_tag ){
  spec = false;
  word = fwrd->str();
  string tag;
#pragma omp critical(foliaupdate)
  {
    tag = fwrd->annotation<PosAnnotation>()->cls();
  }
  spec = ( tag == glue_tag );
  fwords.push_back( fwrd );
}

void mwuAna::merge( const mwuAna *add ){
  fwords.push_back( add->fwords[0] );
  delete add;
}

EntitiesLayer *mwuAna::addEntity( const std::string& tagset,
				  Sentence *sent, EntitiesLayer *el ){
  if ( fwords.size() > 1 ){
    if ( el == 0 ){
#pragma omp critical(foliaupdate)
      {
	KWargs args;
	args["generate_id"] = sent->id();
	el = new EntitiesLayer(sent->doc(),args);
	sent->append( el );
      }
    }
    KWargs args;
    args["set"] = tagset;
    args["generate_id"] = el->id();
    Entity *e=0;
#pragma omp critical(foliaupdate)
    {
      e = new Entity( el->doc(), args );
      el->append( e );
    }
    for ( size_t p=0; p < fwords.size(); ++p ){
#pragma omp critical(foliaupdate)
      {
	e->append( fwords[p] );
      }
    }
  }
  return el;
}

Mwu::Mwu(LogStream * logstream){
  mwuLog = new LogStream( logstream, "mwu-" );
}

Mwu::~Mwu(){
  reset();
  delete mwuLog;
}

void Mwu::reset(){
  for( size_t i=0; i< mWords.size(); ++i ){
    delete mWords[i];
  }
  mWords.clear();
}

void Mwu::add( Word *word ){
  mWords.push_back( new mwuAna( word, glue_tag ) );
}


bool Mwu::read_mwus( const string& fname) {
  *Log(mwuLog) << "read mwus " + fname << endl;
  ifstream mwufile(fname, ios::in);
  if(mwufile.bad()){
    return false;
  }
  string line;
  while( getline( mwufile, line ) ) {
    vector<string> res1, res2; //res1 has mwus and tags, res2 has ind. words
    if ( ( split_at(line, res1, " ") == 2 ) &&
	 ( split_at(res1[0], res2, "_") >= 2 ) ){
      string key = res2[0];
      res2.erase(res2.begin());
      MWUs.insert( make_pair( key, res2 ) );
    }
    else {
      *Log(mwuLog) << "invalid entry in MWU file " << line << endl;
      return false;
    }
  }
  return true;
}

bool Mwu::init( const Configuration& config ) {
  *Log(mwuLog) << "initiating mwuChunker..." << endl;
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
    *Log(mwuLog) << "cannot find attribute 't' in configfile" << endl;
    return false;
  }
  mwuFileName = prefix( config.configDir(), val );
  if ( !read_mwus(mwuFileName) ) {
    *Log(mwuLog) << "Cannot read mwu file " << mwuFileName << endl;
    return false;
  }
  val = config.lookUp( "version", "mwu" );
  if ( val.empty() ){
    version = "1.0";
  }
  else
    version = val;
  val = config.lookUp( "set", "mwu" );
  if ( val.empty() ){
    mwu_tagset = "http://ilk.uvt.nl/folia/sets/frog-mwu-nl";
  }
  else
    mwu_tagset = val;

  val = config.lookUp( "gluetag", "mwu" );
  if ( val.empty() ){
    glue_tag = "SPEC(deeleigen)";
  }
  else
    glue_tag = val;

  return true;
}

ostream &operator <<( ostream& os,
		      const Mwu& mwu ){
  for( size_t i = 0; i < mwu.mWords.size(); ++i )
    os << i+1 << "\t" << mwu.mWords[i]->getWord() << endl;
  return os;
}

void Mwu::addDeclaration( Document& doc ) const {
#pragma omp critical(foliaupdate)
  {
    doc.declare( AnnotationType::ENTITY,
		 mwu_tagset,
		 "annotator='frog-mwu-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void Mwu::Classify( const vector<Word*>& words ){
  if ( words.empty() )
    return;
  reset();
  for ( size_t i=0; i < words.size(); ++i ){
    add( words[i] );
  }
  Classify();
  EntitiesLayer *el = 0;
  Sentence *sent;
#pragma omp critical(foliaupdate)
  {
    sent = words[0]->sentence();
  }
  for( size_t i = 0; i < mWords.size(); ++i ){
    el = mWords[i]->addEntity( mwu_tagset, sent, el );
  }
}

void Mwu::Classify(){
  if ( debug )
    *Log(mwuLog) << "Starting mwu Classify" << endl;
  mymap2::iterator best_match;
  size_t matchLength = 0;
  size_t max = mWords.size();

  // add all current sequences of SPEC(deeleigen) words to MWUs
  for( size_t i=0; i < max-1; ++i ) {
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
  for( i = 0; i < max; i++) {
    string word = mWords[i]->getWord();
    if ( debug )
      *Log(mwuLog) << "checking word[" << i <<"]: " << word << endl;
    pair<mymap2::iterator, mymap2::iterator> matches = MWUs.equal_range(word);
    if ( matches.first != MWUs.end() ) {
      //match
      mymap2::iterator current_match = matches.first;
      if (  debug  ) {
	*Log(mwuLog) << "MWU: match found!\t" << current_match->first << endl;
      }
      while(current_match != matches.second && current_match != MWUs.end()) {
	vector<string> match = current_match->second;
	size_t max_match = match.size();
	size_t j = 0;
	if ( debug )
	  *Log(mwuLog) << "checking " << max_match << " matches:" << endl;
	for(; i + j + 1 < max && j < max_match; j++) {
	  if ( match[j] != mWords[i+j+1]->getWord() ) {
	    if ( debug )
	      *Log(mwuLog) << "match " << j <<" (" << match[j]
		   << ") doesn't match with word " << i+ j + 1
			   << " (" << mWords[i+j + 1]->getWord() <<")" << endl;
	    // mismatch in jth word of current mwu
	    break;
	  }
	  else if ( debug )
	    *Log(mwuLog) << " matched " <<  mWords[i+j+1]->getWord() << " j=" << j << endl;

	}
	if (j == max_match && j > matchLength ){
	  // a match. remember this!
	  best_match = current_match;
	  matchLength = j;
	}
	++current_match;
      } // while
      if( debug ){
	if (matchLength >0 ) {
	  *Log(mwuLog) << "MWU: found match starting with " << (*best_match).first << endl;
	} else {
	  *Log(mwuLog) <<"MWU: no match" << endl;
	}
      }
      // we found a matching mwu, break out of loop thru sentence,
      // do useful stuff, and recurse to find more mwus
      if (matchLength > 0 )
	break;
    } //match found
    else {
      if( debug )
	*Log(mwuLog) <<"MWU:check: no match" << endl;
    }
  } //for (i < max)
  if (matchLength > 0 ) {
    //concat
    if ( debug )
      *Log(mwuLog) << "mwu found, processing" << endl;
    for ( size_t j = 1; j <= matchLength; ++j) {
      if ( debug )
	*Log(mwuLog) << "concat " << mWords[i+j]->getWord() << endl;
      mWords[i]->merge( mWords[i+j] );
    }
    vector<mwuAna*>::iterator anatmp1 = mWords.begin() + i;
    vector<mwuAna*>::iterator anatmp2 = ++anatmp1 + matchLength;
    mWords.erase(anatmp1, anatmp2);
    if ( debug ){
      *Log(mwuLog) << "tussenstand:" << endl;
      *Log(mwuLog) << *this << endl;
    }
    Classify( );
  } //if (matchLength)
  return;
} // //Classify
