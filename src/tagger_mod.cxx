/*
  $Id: Frog.cxx 13560 2011-11-22 13:51:45Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/Frog/trunk/src/Frog.cxx $

  Copyright (c) 2006 - 2011
  Tilburg University

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for Dutch
 
  This file is part of frog

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

#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "frog/Frog.h"
#include "frog/tagger_mod.h"

using namespace std;
using namespace folia;

MBTagger::MBTagger(){
  tagger = 0;
}

MBTagger::~MBTagger(){
  delete tagger;
}

bool MBTagger::init( const Configuration& conf ){
  if ( tagger != 0 ){
    *Log(theErrLog) << "MBTagger is already initialized!" << endl;
    return false;
  }  
  string tagset = conf.lookUp( "settings", "tagger" );
  if ( tagset.empty() ){
    *Log(theErrLog) << "Unable to find settings for Tagger" << endl;
    return false;
  }
  string init = "-s " + configuration.configDir() + tagset;
  if ( Tagger::Version() == "3.2.6" )
    init += " -vc";
  else
    init += " -vcf";
  tagger = new MbtAPI( init, *theErrLog );
  return tagger != 0;
}

string MBTagger::Classify( AbstractElement *sent ){
  string tagged;
  vector<AbstractElement*> swords = sent->words();
  if ( !swords.empty() ) {
    vector<string> words;
    string sentence; // the tagger needs the whole sentence
    for ( size_t w = 0; w < swords.size(); ++w ) {
      sentence += swords[w]->str();
      words.push_back( swords[w]->str() );
      if ( w < swords.size()-1 )
	sentence += " ";
    }
    if (tpDebug > 0 ) *Log(theErrLog) << "[DEBUG] SENTENCE: " << sentence << endl;
    
    if (tpDebug) 
      cout << "in: " << sentence << endl;
    tagged = tagger->Tag(sentence);
    if (tpDebug) {
      cout << "sentence: " << sentence
	   <<"\ntagged: "<< tagged
	   << endl;
    }
  }
  return tagged;
}


