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

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <map>

#include "timbl/TimblAPI.h"

#include "frog/Frog.h" // defines etc.
#include "libfolia/folia.h" // defines etc.
#include "frog/mwu_chunker_mod.h"

using namespace Timbl;
using namespace std;

mwuAna::mwuAna( folia::AbstractElement *fwrd,
		const std::string& wrd, const std::string& tg ){
  fwords.push_back( fwrd );
  word = wrd;
  std::vector<std::string> parts;
  int num = Timbl::split_at_first_of( tg, parts, "()" );
  if ( num < 1 ){
    throw runtime_error("tag should look like 'Main_Tag(Subtags)' but it is: '" + tg + "'" );
  }
  else {
    if ( num > 2 ){
      *Log(theErrLog) << "WARNING: found a suspicious tag: '" << tg << "'. Tags should look like 'Main_Tag(Subtags)' " << endl;
    }
    tagHead = parts[0];
    if ( num > 1 ){
      //
      // the MBT tagger returns things like N(soort,ev,basis,zijd,stan)
      // the Parser is trained with N(soort|ev|basis|zijd|stan)
      // so convert
      //
      string result = parts[1];
      string::size_type pos = result.find( "," );
      while ( pos != string::npos ){
	result.replace(pos,1,"|");
	pos = result.find( "," );
      }
      tagMods = result;
    }
    tag = tg;
  }
}  

ostream &operator <<( ostream& os,
		      const mwuAna& mwa ){
  using folia::operator<<;
  os << "MWU: [";
  for ( size_t i=0; i < mwa.fwords.size(); ++i )
    os << mwa.fwords[i]->id() << ",";
  os << "]";
  return os;
}

void mwuAna::append( const mwuAna *add ){
  //  cerr << " APPEND: " << *add << endl << " to " << *this << endl;
  fwords.push_back( add->getFword() );
  //  cerr << "result " << *this << endl;
}

string mwuAna::getTagMods() const {
  if ( tagMods.empty() )
    return "__";
  else
    return tagMods;
}

void mwuAna::addEntity( folia::AbstractElement *sent ){
  if ( fwords.size() <= 1 )
    return;
  folia::AbstractElement *el = 0;
  try {
    el = sent->annotation( folia::Entities_t );
  }
  catch(...){
    el = new folia::EntitiesLayer("");
#pragma omp critical(foliaupdate)
    {
      sent->append( el );
    }
  }
  folia::AbstractElement *e = new folia::Entity("");
#pragma omp critical(foliaupdate)
  {
    el->append( e );
  }
  for ( size_t p=0; p < fwords.size(); ++p ){
#pragma omp critical(foliaupdate)
    {
      e->append( fwords[p] );
    }
  }
}

Mwu::~Mwu(){}

void Mwu::reset(){
  mWords.clear();
}

void Mwu::add( folia::AbstractElement *fw, 
	       const std::string& word, const std::string& tag ){
  mWords.push_back( new mwuAna( fw, word, tag ) );
}


bool Mwu::read_mwus( const string& fname) {
  *Log(theErrLog) << "read mwus " + fname + "\n";
  ifstream mwufile(fname.c_str(), ios::in);
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
      *Log(theErrLog) << "invalid entry in MWU file " << line << endl;
      return false;
    }
  }
  return true;
}
  
bool Mwu::init( const Configuration& config ) {
  myCFS = "_";
  *Log(theErrLog) << "initiating mwuChunker ... \n";
  debug = tpDebug;
  string db = config.lookUp( "debug", "mwu" );
  if ( !db.empty() )
    debug = stringTo<int>( db );
  string att = config.lookUp( "t", "mwu" );  
  if ( att.empty() ){
    *Log(theErrLog) << "cannot find attribute 't' in configfile" << endl;
    return false;
  }
  mwuFileName = prefix( config.configDir(), att );
  att = config.lookUp( "c", "mwu" );  
  if ( !att.empty() )
    myCFS = att;
  if (!read_mwus(mwuFileName)) {
    *Log(theErrLog) << "Cannot read mwu file " << mwuFileName << endl;
    return false;
  }
  return true;
}

ostream &operator <<( ostream& os,
		      const Mwu& mwu ){
  for( size_t i = 0; i < mwu.mWords.size(); ++i )
    os << i+1 << "\t" << mwu.mWords[i]->getWord() << endl;
  return os;
}

ostream &operator <<( ostream& os,
		      const mwuAna* mwu ){
  return os << *mwu;
}

void Mwu::Classify( folia::AbstractElement *sent ){
  Classify();
  for( size_t i = 0; i < mWords.size(); ++i ){
    mWords[i]->addEntity( sent );
  }
}

void Mwu::Classify(){
  if ( debug )
    cout << "\nStarting mwu Classify\n";
  mymap2::iterator best_match;
  size_t matchLength = 0;
  size_t max = mWords.size();
  
  // add all current sequences of SPEC(deeleigen) words to MWUs
  for( size_t i=0; i < max-1; ++i ) {
    if ( mWords[i]->getTagHead() == "SPEC" &&
	 mWords[i]->getTagMods() == "deeleigen" &&
	 mWords[i+1]->getTagHead() == "SPEC" &&
	 mWords[i+1]->getTagMods() == "deeleigen" ) {
      vector<string> newmwu;
      while ( i < max &&
	      mWords[i]->getTagHead() == "SPEC" &&
	      mWords[i]->getTagMods() == "deeleigen" ) {
	newmwu.push_back(mWords[i]->getWord());
	i++;
      }
      string key = newmwu[0];
      newmwu.erase( newmwu.begin() );
      MWUs.insert( make_pair(key, newmwu) );
    }
  }

  //  cerr << "hier Mwords " << mWords << endl; 
  size_t i; 
  for( i = 0; i < max; i++) {
    string word = mWords[i]->getWord();
    if ( debug )
      cout << "checking word[" << i <<"]: " << word << endl;
    pair<mymap2::iterator, mymap2::iterator> matches = MWUs.equal_range(word);
    if ( matches.first != MWUs.end() ) {
      //match
      mymap2::iterator current_match = matches.first;
      if (  debug  ) {
	cout << "MWU: match found!\t" << current_match->first << endl;
      }
      while(current_match != matches.second && current_match != MWUs.end()) {
	vector<string> match = current_match->second;
	size_t max_match = match.size();
	size_t j = 0;
	if ( debug )
	  cout << "checking " << max_match << " matches\n";
	for(; i + j + 1 < max && j < max_match; j++) {
	  if ( match[j] != mWords[i+j+1]->getWord() ) {
	    if ( debug )
	      cout << "match " << j <<" (" << match[j] 
		   << ") doesn't match with word " << i+ j + 1
		   << " (" << mWords[i+j + 1]->getWord() <<")\n";
	    // mismatch in jth word of current mwu
	    break;
	  }
	  else if ( debug )
	    cout << " matched " <<  mWords[i+j+1]->getWord() << " j=" << j << endl;
	  
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
	  cout << "MWU: found match starting with " << (*best_match).first << endl;
	} else {
	  cout <<"MWU: no match\n";
	}
      }
      // we found a matching mwu, break out of loop thru sentence, 
      // do useful stuff, and recurse to find more mwus
      if (matchLength > 0 )
	break;
    } //match found
    else { 
      if( debug )
	cout <<"MWU:check: no match\n";
    }
  } //for (i < max)
  if (matchLength > 0 ) {
    //concat
    if ( debug )
      cout << "mwu found, processing\n";
    for ( size_t j = 1; j <= matchLength; ++j) {
      if ( debug )
	cout << "concat " << mWords[i+j]->getWord() << endl;
      // and do the same for mWords elems (Word, Tag, Lemma, Morph)
      mWords[i]->append( mWords[i+j] );
      if ( debug ){
	cout << "concat tag " << mWords[i+j]->getTagHead()
	     << "(" << mWords[i+j]->getTagMods() << ")" << endl;
	cout << "gives : " << mWords[i]->getTagHead() 
	     << "(" << mWords[i]->getTagMods() << ")" << endl;
      }
      
    }
    vector<mwuAna*>::iterator anatmp1 = mWords.begin() + i;
    vector<mwuAna*>::iterator anatmp2 = ++anatmp1 + matchLength;
    for ( vector<mwuAna*>::iterator anaTmp = anatmp1;
	  anaTmp != anatmp2; ++ anaTmp )
      delete *anaTmp;
    mWords.erase(anatmp1, anatmp2);
    if ( debug ){
      cout << "tussenstand:" << endl;
      cout << *this << endl;
    }      
    Classify( );
  } //if (matchLength)
  //  cerr << endl << "done Mwords " << mWords << endl; 
  return;
} // //Classify
