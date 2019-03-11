/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
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

#include "frog/ucto_tokenizer_mod.h"

#include <string>
#include "timbl/TimblAPI.h"
#include "frog/Frog-util.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/PrettyPrint.h"
#include "config.h"

using namespace std;
using TiCC::operator<<;

#define LOG *TiCC::Log(uctoLog)

UctoTokenizer::UctoTokenizer( TiCC::LogStream * logstream ) {
  tokenizer = 0;
  uctoLog = new TiCC::LogStream( logstream, "tok-" );
}

string resolve_configdir( const string& rules_name, const string& dir ){
  if ( TiCC::isFile( rules_name ) ){
    return rules_name;
  }
  string temp = prefix( dir, rules_name );
  if ( TiCC::isFile( temp ) ){
    return temp;
  }
  // not found. so lets hope ucto can find it!
  return rules_name;
}

bool UctoTokenizer::init( const TiCC::Configuration& config ){
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
  if ( !config.hasSection("tokenizer") ){
    tokenizer->setPassThru();
  }
  if ( debug > 1 ){
    tokenizer->setDebug( debug );
  }
  string languages = config.lookUp( "languages", "tokenizer" );
  vector<string> language_list;
  if ( !languages.empty() ){
    language_list = TiCC::split_at( languages, "," );
    LOG << "Language List ="  << language_list << endl;
  }
  if ( tokenizer->getPassThru() ){
    // when passthru, we don't further initialize the tokenizer
    // it wil run in minimal mode then.
  }
  else {
    // when a languas (list) is specified on the command line,
    // it overrules the language from the config file
    string rulesName;
    if ( language_list.empty() ){
      rulesName = config.lookUp( "rulesFile", "tokenizer" );
    }
    if ( rulesName.empty() ){
      if ( language_list.empty() ){
	LOG << "no 'rulesFile' or 'languages' found in configuration" << endl;
	return false;
      }
      if ( !tokenizer->init( language_list ) ){
	return false;
      }
      tokenizer->setLanguage( language_list[0] );
    }
    else {
      rulesName = resolve_configdir( rulesName, config.configDir() );
      if ( !tokenizer->init( rulesName ) ){
	return false;
      }
      if ( !language_list.empty() ){
	tokenizer->setLanguage( language_list[0] );
      }
      else {
	tokenizer->setLanguage( "none" );
      }
    }
  }
  string textredundancy = config.lookUp( "textredundancy", "tokenizer" );
  if ( !textredundancy.empty() ){
    tokenizer->setTextRedundancy( textredundancy );
  }
  tokenizer->setEosMarker( "" );
  tokenizer->setVerbose( false );
  tokenizer->setParagraphDetection( false ); //detection of paragraphs
  tokenizer->setXMLOutput( true );
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

void UctoTokenizer::setInputClass( const std::string& cls ){
  if ( tokenizer ){
    if ( !cls.empty() )
      tokenizer->setInputClass( cls );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setOutputClass( const std::string& cls ){
  if ( tokenizer ){
    if ( !cls.empty() )
      tokenizer->setOutputClass( cls );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setDocID( const std::string& id ){
  if ( tokenizer ){
    if ( !id.empty() )
      tokenizer->setDocID( id );
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

void UctoTokenizer::setTextRedundancy( const string& tr ) {
  if ( tokenizer ){
    tokenizer->setTextRedundancy( tr );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

void UctoTokenizer::setPassThru( const bool b ) {
  if ( tokenizer ){
    tokenizer->setPassThru( b );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

bool UctoTokenizer::getPassThru() const {
  if ( tokenizer ){
    return tokenizer->getPassThru();
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

#if UCTO_INT_VERSION < 15
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

string UctoTokenizer::tokenizeStream( istream& is ){
  if ( tokenizer ){
    return tokenizer->tokenizeSentenceStream( is );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

folia::Document *UctoTokenizer::tokenize_folia( const string& buffer ){
  if ( !tokenizer ){
    throw runtime_error( "ucto tokenizer not initialized" );
  }
  folia::Document *doc = new folia::Document();
  if ( buffer.find("<?xml " ) == 0 ){
    doc->readFromString( buffer );
  }
  else {
    doc->readFromFile( buffer );
  }
  tokenize( *doc );
  return doc;
}

#else

vector<string> UctoTokenizer::tokenize( const string& line ){
  if ( tokenizer ){
    tokenizer->reset();
    tokenizer->tokenizeLine( line );
    return tokenizer->getSentences();
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

string UctoTokenizer::tokenizeStream( istream& is ){
  if ( tokenizer ){
    vector<Tokenizer::Token> toks = tokenizer->tokenizeOneSentence( is );
    return tokenizer->getString( toks );
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

folia::Document *UctoTokenizer::tokenize_folia( const string& buffer ){
  if ( !tokenizer ){
    throw runtime_error( "ucto tokenizer not initialized" );
  }
  else {
    return tokenizer->tokenize_folia( buffer );
  }
}

#endif

folia::Document *UctoTokenizer::tokenize( istream& is ){
  if ( tokenizer )
    return tokenizer->tokenize( is );
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

folia::Document *UctoTokenizer::tokenizestring( const string& s){
  if ( tokenizer) {
    istringstream is(s);
    return tokenizer->tokenize( is);
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}
