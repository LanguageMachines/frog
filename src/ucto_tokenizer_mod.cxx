/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2020
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

UctoTokenizer::UctoTokenizer( TiCC::LogStream *err_log,
			      TiCC::LogStream *dbg_log ) {
  /// Created a (yet UNINITIALIZED) Tokenizer
  /*!
    \param err_log A LogStream for error messages
    \param dbg_log A LogStream for debugging

     UctoTokenizer::init() needs to be called to get really going
  */
  tokenizer = 0;
  cur_is = 0;
  errLog = new TiCC::LogStream( err_log, "tok-" );
  if ( dbg_log ){
    dbgLog = new TiCC::LogStream( dbg_log, "tok-" );
  }
  else {
    dbgLog = errLog;
  }
  debug = 0;
}

UctoTokenizer::~UctoTokenizer(){
  /// Destroy the Tokenizer
  // dbg_log is deleted by deleting the tokenizer element.
  if ( dbgLog != errLog ){
    delete errLog;
  }
  delete tokenizer;
}

string resolve_configdir( const string& rules_name, const string& dir ){
  /// lame attempt to resolve the location of a rules file give a possible dir
  /*!
    \param rules_name The filename to search
    \param dir possible location of the file
  */
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
  /// initalize a Tokenizer using a Configuration structure
  /*!
    \param config the Configuration to use
    \return true on success, false otherwise

    this function sets up an Ucto tokenizer with some defaults and the
    values from config.

  */
  if ( tokenizer ){
    throw runtime_error( "ucto tokenizer is already initialized" );
  }
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
    if ( !val.empty() ){
      debug = TiCC::stringTo<int>( val );
    }
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
      tokenizer->setLangDetection(true);
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
        //fall back to rulesFile
        rulesName = config.lookUp( "rulesFile", "tokenizer" );
        if ( rulesName.empty() ) {
            LOG << "no 'rulesFile' found in configuration and language list initialisation failed" << endl;
            return false;
        }
      } else {
        rulesName = ""; //so we don't trigger the next init block
        tokenizer->setLanguage( language_list[0] );
      }
    }

    if (!rulesName.empty() ){
      rulesName = resolve_configdir( rulesName, config.configDir() );
      if ( !tokenizer->init( rulesName ) ){
	return false;
      }
      if ( !language_list.empty() ){
	tokenizer->setLanguage( language_list[0] );
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
  /// set the utterance marker for the tokenizer
  /*!
    \param u string holding the marker. e.g. "<utt>"
  */
  if ( tokenizer ){
    if ( !u.empty() ){
      tokenizer->setEosMarker( u );
    }
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setSentencePerLineInput( bool b ) {
  /// set the tokenizer SentencePerLine property
  /*!
    \param b a boolean, true to set to ON or OFF respectively
  */
  if ( tokenizer ){
    tokenizer->setSentencePerLineInput( b );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setQuoteDetection( bool b ) {
  /// set the tokenizer QuoteDetection property
  /*!
    \param b a boolean, true to set to ON or OFF respectively
  */
  if ( tokenizer ) {
    tokenizer->setQuoteDetection( b );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setWordCorrection( bool b ) {
  /// set the tokenizer WordCorrection property
  /*!
    \param b a boolean, true to set to ON or OFF respectively
  */
  if ( tokenizer ) {
    tokenizer->setWordCorrection( b );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setInputEncoding( const std::string & enc ){
  /// set the tokenizer InputEncoding value
  /*!
    \param enc a string holding a possible encoding. e.g. "WINDOWS-1252"
  */
  if ( tokenizer ){
    if ( !enc.empty() ){
      tokenizer->setInputEncoding( enc );
    }
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setInputClass( const std::string& cls ){
  /// set the tokenizer InputClass value
  /*!
    \param cls a string holding the inputclass. e.g. "OCR"
  */
  if ( tokenizer ){
    if ( !cls.empty() ){
      tokenizer->setInputClass( cls );
    }
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setOutputClass( const std::string& cls ){
  /// set the tokenizer OutputClass value
  /*!
    \param cls a string holding the outputclass. e.g. "current"
  */
  if ( tokenizer ){
    if ( !cls.empty() ){
      tokenizer->setOutputClass( cls );
    }
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setDocID( const std::string& id ){
  /// set the tokenizer DocID value (for FoLiA)
  /*!
    \param id a string holding the document id. e.g. "document_1"
  */
  if ( tokenizer ){
    if ( !id.empty() ){
      tokenizer->setDocID( id );
    }
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setInputXml( bool b ){
  /// set the tokenizer InputXml property
  /*!
    \param b a boolean, true to set to ON or OFF respectively
  */
  if ( tokenizer ){
    tokenizer->setXMLInput( b );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setFiltering( bool b ){
  /// set the tokenizer Filtering property
  /*!
    \param b a boolean, true to set to ON or OFF respectively
  */
  if ( tokenizer ){
    tokenizer->setFiltering( b );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setTextRedundancy( const string& tr ) {
  /// set the tokenizer TextRedundancy value (for FoLiA)
  /*!
    \param tr a string holding the value. Possible values are "none",
    "minimal" and "full"
  */
  if ( tokenizer ){
    tokenizer->setTextRedundancy( tr );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::set_TC_debug( const bool b ) {
  /// set the tokenizer TC_debug property
  /*!
    \param b a boolean, true to set to ON or OFF respectively
  */
  if ( tokenizer ){
    tokenizer->set_tc_debug( b );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::setPassThru( const bool b ) {
  /// set the tokenizer PassThru property
  /*!
    \param b a boolean, true to set to ON or OFF respectively
  */
  if ( tokenizer ){
    tokenizer->setPassThru( b );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

bool UctoTokenizer::getPassThru() const {
  /// get the value of the PassThru setting
  if ( tokenizer ){
    return tokenizer->getPassThru();
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

void UctoTokenizer::add_provenance( folia::Document& doc,
				    folia::processor *main ) const {
  /// add provenance information for the tokenizer. (FoLiA output only)
  /*!
    \param doc the FoLiA document to add to
    \param main the processor to use (presumably the Frog processor)
  */
  if ( !tokenizer ){
    throw runtime_error( "ucto tokenizer not initialized" );
  }
  if ( tokenizer->getPassThru() ){
    tokenizer->add_provenance_passthru( &doc, main );
  }
  else {
    tokenizer->add_provenance_setting( &doc, main );
    if ( !tokenizer->ucto_re_run() ){
      //      cerr << "FOUND processor: " << p << endl;
      tokenizer->add_provenance_structure( &doc, main );
    }
    else {
      cerr << "\nWARNING: cannot tokenize: " << doc.filename()
	   << ". It has been processed with ucto before! \n"
	   << "  Falling back to passthru mode. (you might consider using "
	   << "--skip=t)\n" << endl;
      tokenizer->setPassThru( true );
      tokenizer->add_provenance_passthru( &doc, main );
    }
  }
}

vector<string> UctoTokenizer::tokenize( const string& line ){
  /// Tokenize a buffer of characters into a list of tokenized sentences
  /*!
    \param line of sequence of characters to be tokenized
    \return a vector of strings each representing a sentence

    The input line may be long and include newlines etc. Is is assumed to be
    in the current InputEncoding.

    The output is sequence of tokenized strings in UTF8, each representing one
    sentence.
   */
  if ( tokenizer ){
    tokenizer->reset();
    tokenizer->tokenizeLine( line );
    return tokenizer->getSentences();
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

string UctoTokenizer::tokenizeStream( istream& is ){
  /// Tokenize characters from a stream into one tokenized sentences
  /*!
    \param is the input stream
    \return a string representing a sentence, or "" when done.

    This function will extract characters from stream and tokenize them into
    a sentence. Can be called repeatedly to get more sentences.

    The Ucto tokenizer is keeping state of the input, so when calling this
    function again it is possible that NO actual data is read from the stream
    while a sentence is still in the tokenizer's buffer

   */
  if ( tokenizer ){
    vector<Tokenizer::Token> toks = tokenizer->tokenizeOneSentence( is );
    return tokenizer->getString( toks );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

vector<Tokenizer::Token> UctoTokenizer::tokenize_stream_next( ){
  /// Tokenize characters from the current input stream into a list of Ucto::Token
  /*!
    \return a list of Ucto::Token elements which can be examined further

    This function will extract characters from stream and tokenize them.

    This is non greedy. Might be called multiple times to consume
    the whole stream.
    It will return tokens upto an ENDOFSENTENCE token or out of data
   */
  if ( tokenizer) {
    return tokenizer->tokenizeOneSentence( *cur_is );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

vector<Tokenizer::Token> UctoTokenizer::tokenize_stream( istream& is ){
  ///  restart the tokenizer on stream 'is'
  ///  and calls tokenizer_stream_next() for the first results
  /*!
    \param is the stream to connect to
    \return a list of Ucto::Token elements which can be examined further

    After calling tokenize_stream() you should continue by calling
    tokenize_stream_next() until no more tokens ar found.
   */
  if ( tokenizer ){
    tokenizer->reset();
    cur_is = &is;
    return tokenize_stream_next();
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

vector<Tokenizer::Token> UctoTokenizer::tokenize_line( const string& buffer,
						       const string& lang ){
  /// tokenize a buffer using a specific language
  /*!
    \param buffer a (possible long) sequence of characters
    \param lang the language to use for tokenizing
    \return a list of Ucto::Token elements representing the first sentence

    The buffer is consumed completely and stored as tokens in the Ucto Tokenizer

    After calling tokenize_line() you should continue by calling
    tokenize_line_next() repeatedly to extract the next sentences
   */
  if ( tokenizer ){
    tokenizer->reset();
    tokenizer->tokenizeLine( buffer, lang ); // will consume whole line!
    return tokenize_line_next(); // returns next sentence in the line
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

vector<Tokenizer::Token> UctoTokenizer::tokenize_line_next() {
  /// extract the next sequence of Token elements
  /*!
    \return a list of Ucto::Token elements representing the next sentence

    assumes the tokenizer is first set up using tokenize_line()
  */
  if ( tokenizer ){
    return tokenizer->popSentence();
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

string UctoTokenizer::get_data_version() const{
  /// returns the version of the uctodata files we use
  if ( tokenizer ){
    return tokenizer->get_data_version();
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

bool UctoTokenizer::get_setting_info( const std::string& lang,
				      std::string& name,
				      std::string& version ) const {
  /// get information about the current settings for a language
  /*!
    \param lang The language to examine
    \param name The name of the settings file used for \e lang
    \param version The version of the settingsfile
  */
  if ( tokenizer ){
    return tokenizer->get_setting_info( lang, name, version );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}


string get_parent_id( folia::FoliaElement *el ){
  /// return the id of the FoliaElement or its parent
  /*!
    \param el The FoliaElement to examine
    \return the id found or "" if nothing is found

    recurses upwards through the parents when the element doesn't have an id
  */
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

string UctoTokenizer::default_language() const {
  /// return the default language of the tokenizer
  if ( tokenizer ){
    return tokenizer->getLanguage();
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }
}

vector<folia::Word*> UctoTokenizer::add_words( folia::Sentence* s,
					       const frog_data& fd ) const {
  /// create a list of folia::Word elements under a folia::Sentence
  /*!
    \param s The parent to attach too
    \param fd The frog_data structure with the needed information
    \return a list of newly created folia::Word elements

    this function should be used when creating FoLiA output, and it assumes
    that the tokenizer allready filled in all required fields in the frog_data
    structure
   */
  string textclass = tokenizer->getOutputClass();
  string tok_set;
  string lang = fd.get_language();
  if ( tokenizer->getPassThru() || lang.empty() ){
    tok_set = "passthru";
  }
  else if ( lang != "default" ){
    tok_set = "tokconfig-" + lang;
  }
  else {
    tok_set = "tokconfig-" + default_language();
  }
  vector<folia::Word*> wv;
  if (  debug > 5 ){
    DBG << "add_words\n" << fd << endl;
    DBG << "sentence has tekst: " << s->str(textclass) << endl;
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
    args["set"] = tok_set;
    folia::Word *w;
#pragma omp critical (foliaupdate)
    {
      if (  debug > 5 ){
	DBG << "create Word(" << args << ") = " << word.word << endl;
      }
      try {
	w = new folia::Word( args, s->doc() );
      }
      catch ( const exception& e ){
	cerr << "Word(" << args << ") creation failed: " << e.what() << endl;
	exit(EXIT_FAILURE);
      }
      w->settext( word.word, textclass );
      if (  debug > 5 ){
	DBG << "add_result, create a word, done:" << w << endl;
      }
      s->append( w );
    }
    wv.push_back( w );
  }
  if (  debug > 5 ){
    DBG << "add_result, finished sentence:" << s << endl;
    DBG << "Sentence tekst: " << s->str(textclass) << endl;
  }
  return wv;
}

vector<Tokenizer::Token> UctoTokenizer::correct_words( folia::FoliaElement *elt,
						       const vector<folia::Word*>& wv ){
  /// correct Word elements in the FoLiA based on results found by the tokenizer
  /*!
    \param elt the FoliaElement which is the parent for correction
    \param wv The input Word vector, (of which elt is the parent)

    the input Word vector might represent a 'word' like "gisteren?". The
    tokenizer will split this into "gisteren" and "?" and this function
    will handle this by creating a correction with 2 words as \<new\>
   */
  if ( tokenizer ){
    vector<folia::FoliaElement*> ev( wv.begin(), wv.end() );
    return tokenizer->correct_elements( elt, ev );
  }
  else {
    throw runtime_error( "ucto tokenizer not initialized" );
  }

}
