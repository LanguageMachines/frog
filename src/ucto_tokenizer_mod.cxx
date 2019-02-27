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
#include "frog/FrogData.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/PrettyPrint.h"
#include "config.h"

using namespace std;
using TiCC::operator<<;

#define LOG *TiCC::Log(errLog)
#define DBG *TiCC::Log(dbgLog)

UctoTokenizer::UctoTokenizer( TiCC::LogStream *errlog,
			      TiCC::LogStream *dbglog ) {
  tokenizer = 0;
  cur_is = 0;
  errLog = new TiCC::LogStream( errlog, "tok-" );
  if ( dbglog ){
    dbgLog = new TiCC::LogStream( dbglog, "tok-" );
  }
  else {
    dbgLog = errLog;
  }
  debug = 0;
}

UctoTokenizer::~UctoTokenizer(){
  // dbg_log is deleted by deleting the tokenizer!
  if ( dbgLog != errLog ){
    delete errLog;
  }
  delete tokenizer;
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
  tokenizer->setErrorLog( dbgLog );
  tokenizer->setEosMarker( "" );
  tokenizer->setVerbose( false );
  tokenizer->setParagraphDetection( false ); //detection of paragraphs
  tokenizer->setXMLOutput( true );
  if ( !config.hasSection("tokenizer") ){
    tokenizer->setPassThru();
    // when passthru, we don't further initialize the tokenizer
    // it wil run in minimal mode then.
  }
  else {
    string val = config.lookUp( "debug", "tokenizer" );
    if ( val.empty() ){
      val = config.lookUp( "debug" );
    }
    if ( !val.empty() )
      debug = TiCC::stringTo<int>( val );
    if ( debug > 1 ){
      tokenizer->setDebug( debug );
    }
    val = config.lookUp( "textcatdebug", "tokenizer" );
    if ( !val.empty() ){
      tokenizer->set_tc_debug( true );
    }
    string languages = config.lookUp( "languages", "tokenizer" );
    vector<string> language_list;
    if ( !languages.empty() ){
      language_list = TiCC::split_at( languages, "," );
      LOG << "Language List ="  << language_list << endl;
    }
    // when a language (list) is specified on the command line,
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
    textredundancy = config.lookUp( "textredundancy", "tokenizer" );
    if ( !textredundancy.empty() ){
      tokenizer->setTextRedundancy( textredundancy );
    }
  }
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

void UctoTokenizer::setFiltering( bool b ){
  if ( tokenizer ){
    tokenizer->setFiltering( b );
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

void UctoTokenizer::set_TC_debug( const bool b ) {
  if ( tokenizer ){
    tokenizer->set_tc_debug( b );
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

frog_data create_fd( vector<Tokenizer::Token>& tokens ){
  frog_data result;
  int quotelevel = 0;
  while ( !tokens.empty() ){
    const auto tok = tokens.front();
    tokens.erase(tokens.begin());
    frog_record tmp;
    tmp.word = TiCC::UnicodeToUTF8(tok.us);
    tmp.token_class = TiCC::UnicodeToUTF8(tok.type);
    tmp.no_space = (tok.role & Tokenizer::TokenRole::NOSPACE);
    tmp.language = tok.lc;
    tmp.new_paragraph = (tok.role & Tokenizer::TokenRole::NEWPARAGRAPH);
    result.units.push_back( tmp );
    if ( (tok.role & Tokenizer::TokenRole::BEGINQUOTE) ){
      ++quotelevel;
    }
    if ( (tok.role & Tokenizer::TokenRole::ENDQUOTE) ){
      --quotelevel;
    }
    if ( (tok.role & Tokenizer::TokenRole::ENDOFSENTENCE) ){
      // we are at ENDOFSENTENCE.
      // when quotelevel == 0, we step out, until the next call
      if ( quotelevel == 0 ){
     	break;
      }
    }
  }
  return result;
}

frog_data UctoTokenizer::tokenize_stream_next( ){
  // this is non greedy. Might be called multiple times to consume
  // the whole stream
  // will return tokens upto an ENDOFSENTENCE token or out of data
  if ( tokenizer) {
    vector<Tokenizer::Token> new_toks = tokenizer->tokenizeStream( *cur_is );
    // add new tokens to the queue
    queue.insert( queue.end(), new_toks.begin(), new_toks.end() );
    frog_data result = create_fd( queue ); // may leave entries in the queue
    return result;
  }
  else
    throw runtime_error( "ucto tokenizer not initialized" );
}

frog_data UctoTokenizer::tokenize_stream( istream& is ){
  ///  restart the tokenizer on stream @is
  ///  and calls tokenizer_stream_next() for the first results
  cur_is = &is;
  queue.clear();
  return tokenize_stream_next();
}

#if UCTO_INT_VERSION < 14
frog_data UctoTokenizer::tokenize_line( const string& line ){
  if ( tokenizer ){
    cur_is = new istringstream ( line ); // HACK!, will leak
    return tokenize_stream( *cur_is );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

frog_data UctoTokenizer::tokenize_line_next() {
  if ( tokenizer ){
    return tokenize_stream_next();
    }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

#else

frog_data UctoTokenizer::tokenize_line( const string& line ){
  if ( tokenizer ){
    tokenizer->tokenizeLine( line ); // will consume whole line!
    return tokenize_line_next(); // returns next sentence in the line
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

frog_data UctoTokenizer::tokenize_line_next() {
  if ( tokenizer ){
    vector<Tokenizer::Token> tokens = tokenizer->popSentence();
    return create_fd( tokens );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

#endif // UCTO_INT_VERSION

string get_parent_id( folia::FoliaElement *el ){
  if ( !el ){
    return "";
  }
  else if ( !el->id().empty() ){
    return el->id();
  }
  else {
    return get_parent_id( el->parent() );
  }
}

vector<folia::Word*> UctoTokenizer::add_words( folia::Sentence* s,
					       const string& textclass,
					       const string& tok_set,
					       const frog_data& fd ) const {
  vector<folia::Word*> wv;
  if (  debug > 5 ){
    DBG << "add_words\n" << fd << endl;
  }
  for ( const auto& word : fd.units ){
    if (  debug > 5 ){
      DBG << "add_result\n" << word << endl;
    }
    folia::KWargs args;
    string ids = get_parent_id( s );
    if ( !ids.empty() ){
      args["generate_id"] = ids;
    }
    args["class"] = word.token_class;
    if ( word.no_space ){
      args["space"] = "no";
    }
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
    if ( !tok_set.empty() ){
      args["set"] = tok_set;
    }
    folia::Word *w;
#pragma omp critical (foliaupdate)
    {
      if (  debug > 5 ){
	DBG << "create Word(" << args << ") = " << word.word << endl;
      }
      w = new folia::Word( args, s->doc() );
      w->settext( word.word, textclass );
      if (  debug > 5 ){
	DBG << "add_result, create a word, done:" << w << endl;
      }
      s->append( w );
    }
    wv.push_back( w );
  }
  if ( textredundancy == "full" ){
    s->settext( s->str(textclass), textclass );
  }
  return wv;
}
