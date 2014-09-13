/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2014
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
#include "ticcutils/Configuration.h"
#include "frog/ucto_tokenizer_mod.h"

using namespace std;
using namespace TiCC;

UctoTokenizer::UctoTokenizer(LogStream * logstream) {
  tokenizer = 0;
  uctoLog = new LogStream( logstream, "tok-" );
}

bool UctoTokenizer::init( const Configuration& config, const string & docid, bool pass ){
  if ( tokenizer )
    throw runtime_error( "ucto tokenizer is already initialized" );
  tokenizer = new Tokenizer::TokenizerClass();
  tokenizer->setErrorLog( uctoLog );
  int debug = 0;
  string val = config.lookUp( "debug", "tokenizer" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() )
    debug = TiCC::stringTo<int>( val );
  tokenizer->setDebug( debug );
  if ( pass ){
    // when passthru, we don't further initialize the tokenizer
    // it wil run in minimal mode then.
    tokenizer->setPassThru( true );
  }
  else {
    string rulesName = config.lookUp( "rulesFile", "tokenizer" );
    if ( rulesName.empty() ){
      *Log(uctoLog) << "no rulesFile found in configuration" << endl;
      return false;
    }
    else {
      if ( !tokenizer->init( rulesName ) )
	return false;
    }
  }
  tokenizer->setEosMarker( "" );
  tokenizer->setVerbose( false );
  tokenizer->setSentenceDetection( true ); //detection of sentences
  tokenizer->setParagraphDetection( false ); //detection of paragraphs
  tokenizer->setXMLOutput( true, docid );
  return true;
}

void UctoTokenizer::setUttMarker( const string& u ) {
  if ( tokenizer ){
    if ( !u.empty() )
      tokenizer->setEosMarker( u );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setSentencePerLineInput( bool b ) {
  if ( tokenizer )
    tokenizer->setSentencePerLineInput( b );
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setQuoteDetection( bool b ) {
  if ( tokenizer )
    tokenizer->setQuoteDetection( b );
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setInputEncoding( const std::string & enc ){
  if ( tokenizer ){
    if ( !enc.empty() )
      tokenizer->setInputEncoding( enc );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setTextClass( const std::string& cls ){
  if ( tokenizer ){
    if ( !cls.empty() )
      tokenizer->setTextClass( cls );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setInputXml( bool b ){
  if ( tokenizer ){
    tokenizer->setXMLInput( b );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

vector<string> UctoTokenizer::tokenize( const string& line ){
  if ( tokenizer ){
    tokenizer->reset();
    if ( tokenizer->getPassThru() ){
      bool bos = true;
      tokenizer->passthruLine( line, bos );
    }
    else {
      tokenizer->tokenizeLine( line );
    }
    return tokenizer->getSentences();
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

folia::Document UctoTokenizer::tokenize( istream& is ){
  if ( tokenizer )
    return tokenizer->tokenize( is );
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

folia::Document * UctoTokenizer::tokenizehelper( const string& s){
  /* this helper function is only here for the frog-python binding, we have to cheat a
   * pointer in for it to work properly */
  if ( tokenizer) {
    istringstream is(s);
    return tokenizer->tokenizenew( is);
  } else
    throw runtime_error( "ucto tokenizer not initialized" );
}

bool UctoTokenizer::tokenize( folia::Document& doc ){
  if ( tokenizer )
    return tokenizer->tokenize( doc );
  else
    throw runtime_error( "ucto tokenizer not initialized" );

}
