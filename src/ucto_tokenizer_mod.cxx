/*
  $Id: $
  $URL: $

  Copyright (c) 2006 - 2012
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

#include <map>
#include <string>
#include "timbl/TimblAPI.h"
#include "frog/Frog.h"
#include "frog/Configuration.h"
#include "frog/ucto_tokenizer_mod.h"

using namespace std;

bool UctoTokenizer::init( const Configuration& conf ){
  string rulesName = conf.lookUp( "rulesFile", "tokenizer" );
  if ( rulesName.empty() ){
    *Log(theErrLog) << "no rulesFile found in configuration" << endl;
    return false;
  }
  else {
    tokenizer.setErrorLog( theErrLog );
    if ( !tokenizer.init( rulesName ) )
      return false;
  }
  string debug = conf.lookUp( "debug", "tokenizer" );
  if ( debug.empty() ){
    debug = conf.lookUp( "debug" );
  }
  if ( debug.empty() ){
    tokenizer.setDebug( tpDebug );
  }
  else
    tokenizer.setDebug( Timbl::stringTo<int>(debug) );
  tokenizer.setEosMarker( "" );
  tokenizer.setVerbose( false );
  tokenizer.setSentenceDetection( true ); //detection of sentences
  tokenizer.setParagraphDetection( false ); //detection of paragraphs  
  tokenizer.setXMLOutput( true, "frog" );
  return true;
}


folia::Document UctoTokenizer::tokenize( istream& is ){
  return tokenizer.tokenize( is );
}

