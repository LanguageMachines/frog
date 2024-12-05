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

#include "frog/tagger_base.h"

#include <algorithm>
#include "ticcutils/SocketBasics.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/json.hpp"
#include "frog/Frog-util.h"

using namespace std;
using namespace Tagger;
using namespace icu;
using namespace nlohmann;

using TiCC::operator<<;

#define LOG *TiCC::Log(err_log)
#define DBG *TiCC::Log(dbg_log)

BaseTagger::BaseTagger( TiCC::LogStream *errlog,
			TiCC::LogStream *dbglog,
			const string& label ):
  debug(0),
  _label(label),
  tagger(NULL),
  filter(NULL)
{
  err_log = new TiCC::LogStream( errlog );
  err_log->set_message( _label + "-tagger-" );
  if ( dbglog ){
    dbg_log = new TiCC::LogStream( dbglog );
    dbg_log->set_message( _label + "-tagger-" );
  }
  else {
    dbg_log = err_log;
  }
}

BaseTagger::~BaseTagger(){
  delete tagger;
  delete filter;
  if ( err_log != dbg_log ){
    delete dbg_log;
  }
  delete err_log;
}

bool BaseTagger::fill_map( const string& file ){
  /// fill the internal token_tag_map from a file
  /*!
    \param file the filename
    \return true on succes, false otherwise

    the file should contain lines with TAB separated attribute/value pairs

    lines starting with '#' are seen as comment
  */
  ifstream is( file );
  if ( !is ){
    return false;
  }
  UnicodeString line;
  while( TiCC::getline( is, line ) ){
    if ( line.isEmpty() || line[0] == '#' ){
      continue;
    }
    vector<UnicodeString> parts = TiCC::split_at( line, "\t" );
    if ( parts.size() != 2 ){
      LOG << "invalid entry in '" << file << "'" << endl;
      LOG << "expected 2 tab-separated values, but got: '"
	  << line << "'" << endl;
      return false;
    }
    token_tag_map[ parts[0] ] = parts[1];
  }
  return true;
}

bool BaseTagger::init( const TiCC::Configuration& config ){
  /// initalize a tagger from 'config'
  /*!
    \param config the TiCC::Configuration
    \return true on succes, false otherwise

  */
  if ( tagger != 0 ){
    LOG << _label << "-tagger is already initialized!" << endl;
    return false;
  }
  string val = config.lookUp( "host", _label );
  if ( !val.empty() ){
    // assume we must use a MBT server for tagging
    _host = val;
    val = config.lookUp( "port", _label );
    if ( val.empty() ){
      LOG << "missing 'port' settings for host= " << _host << endl;
      return false;
    }
    _port = val;
    val = config.lookUp( "base", _label );
    if ( !val.empty() ){
      base = val;
    }
  }
  else {
    val = config.lookUp( "port", _label );
    if ( !val.empty() ){
      LOG << "missing 'host' settings for port= " << _port << endl;
      return false;
    }
  }
  val = config.lookUp( "debug", _label );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  switch ( debug ){
  case 0:
  case 1:
    dbg_log->set_level(LogSilent);
    break;
  case 2:
    dbg_log->set_level(LogNormal);
    break;
  case 3:
  case 4:
    dbg_log->set_level(LogDebug);
    break;
  case 5:
  case 6:
  case 7:
    dbg_log->set_level(LogHeavy);
    break;
  default:
    dbg_log->set_level(LogExtreme);
  }
  string settings;
  if ( _host.empty() ){
    val = config.lookUp( "settings", _label );
    if ( val.empty() ){
      LOG << "Unable to find settings for: " << _label << endl;
      return false;
    }
    if ( val[0] == '/' ) { // an absolute path
      settings = val;
    }
    else {
      settings = config.configDir() + val;
    }
  }
  val = config.lookUp( "version", _label );
  if ( val.empty() ){
    _version = "1.0";
  }
  else {
    _version = val;
  }
  val = config.lookUp( "set", _label );
  if ( val.empty() ){
    LOG << "missing 'set' declaration in config" << endl;
    return false;
  }
  else {
    tagset = val;
  }
  string charFile = config.lookUp( "char_filter_file", _label );
  if ( charFile.empty() ){
    charFile = config.lookUp( "char_filter_file" );
  }
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new TiCC::UniFilter();
    filter->fill( charFile );
  }
  string tokFile = config.lookUp( "token_trans_file", _label );
  if ( tokFile.empty() ){
    tokFile = config.lookUp( "token_trans_file" );
  }
  if ( !tokFile.empty()
       && tokFile != "ignore" ){
    tokFile = prefix( config.configDir(), tokFile );
    if ( !fill_map( tokFile ) ){
      LOG << "failed to load a token translation file from: '"
		   << tokFile << "'"<< endl;
      return false;
    }
  }
  string cls = config.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }
  if ( debug > 1 ){
    DBG << _label << "-tagger textclass= " << textclass << endl;
  }
  if ( _host.empty() ){
    string init_string = "-s " + settings + " -vcf";
    tagger = new MbtAPI( init_string, *dbg_log );
    return tagger->isInit();
  }
  else {
    string mess = check_server( _host, _port, _label );
    if ( !mess.empty() ){
      LOG << "FAILED to find an " << _label << " server:" << endl;
      LOG << mess << endl;
      LOG << "mbtserver not running??" << endl;
      return false;
    }
    else {
      LOG << "using " << _label << "-tagger on "
	  << _host << ":" << _port << endl;
      return true;
    }
  }
}

void BaseTagger::add_provenance( folia::Document& doc,
				 folia::processor *main ) const {
  /// add provenance information for this tagger. (FoLiA output only)
  /*!
    \param doc the FoLiA document to add to
    \param main the processor to use (presumably the Frog processor)
  */
  if ( !main ){
    throw logic_error( _label + "::add_provenance() without parent processor." );
  }
  folia::KWargs args;
  args["name"] = _label;
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  add_declaration( doc, proc );
}

json create_json( const vector<tag_entry>& tv ){
  /// output a vector of tag_entry structs as JSON
  /*!
    \param tv The vector of tag_entry elements, filled by the tagger
    \return a json structure
  */
  json result;
  result["command"] = "tag";
  json arr = json::array();
  for ( const auto& it : tv ){
    json one_entry;
    one_entry["word"] = TiCC::UnicodeToUTF8(it.word);
    if ( !it.enrichment.isEmpty() ){
      one_entry["enrichment"] = TiCC::UnicodeToUTF8(it.enrichment);
    }
    arr.push_back( one_entry );
  }
  result["sentence"] = arr;
  return result;
}

vector<TagResult> json_to_TR( const json& in ){
  /// convert a JSON structure to a vector of TagResult elements
  /*!
    \param in The input JSON
    \return a vector of TagResult structures

    Used by the server mode to convert incoming JSON to a structure we
    can further process
  */
  vector<TagResult> result;
  for ( auto& i : in ){
    TagResult tr;
    tr.set_word( TiCC::UnicodeFromUTF8(i["word"]) );
    if ( i.find("known") != i.end() ){
      tr.set_known( i["known"] == "true" );
    }
    tr.set_tag( TiCC::UnicodeFromUTF8(i["tag"]) );
    if ( i.find("confidence") != i.end() ){
      tr.set_confidence( i["confidence"] );
    }
    if ( i.find("distance") != i.end() ){
      tr.set_distance( i["distance"] );
    }
    if ( i.find("distribution") != i.end() ){
      tr.set_distribution(  TiCC::UnicodeFromUTF8(i["distribution"]) );
    }
    if ( i.find("enrichment") != i.end() ){
      tr.set_enrichment( TiCC::UnicodeFromUTF8(i["enrichment"]) );
    }
    result.push_back( tr );
  }
  return result;
}

json BaseTagger::read_from_client( Sockets::ClientSocket& client ) const {
  string input_line;
  client.read( input_line );
  json response;
  try {
    response = json::parse( input_line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << input_line << "':"
	<< e.what() << endl;
    abort();
  }
  DBG << "received json data:" << response << endl;
  return response;
}

void BaseTagger::write_to_client( json& j_out,
				  Sockets::ClientSocket& client ) const {
  string output_line = j_out.dump() + "\n";
  DBG << "sending json data:" << output_line << endl;
  client.write( output_line );
}

vector<TagResult> BaseTagger::call_server( const vector<tag_entry>& tv ) const {
  /// Connect to a MBT server, send and receive JSON and translate to a
  /// TagResult list
  /*!
    \param tv the tag_entry we would like to be serviced
    \return a vector of TagResult elements

    We set up a connection to the configured server, send a query in JSON
    and on succesful receiving back a JSON result we convert it back into
    a TagResult vector

    \note So this is a one-shot operation. No connection to the MBT server is
    kept open.
  */
  Sockets::ClientSocket client;
  if ( !client.connect( _host, _port ) ){
    LOG << "failed to open connection, " << _label << "::" << _host
	<< ":" << _port << endl
	<< "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  DBG << "calling " << _label << "-server, base=" << base << endl;
  json response = read_from_client( client );
  if ( response["status"] != "ok" ){
    LOG << "the client isn't OK" << endl;
    abort();
  }
  if ( !base.empty() ){
    json out_json;
    out_json["command"] = "base";
    out_json["param"] = base;
    DBG << "created json" << out_json << endl;
    write_to_client( out_json, client );
    json base_response = read_from_client( client );
    if ( base_response["status"] != "ok" ){
      LOG << "the client isn't OK" << endl;
      abort();
    }
  }
  // create json query struct
  json my_json = create_json( tv );
  DBG << "created json" << my_json << endl;
  // send it to the server
  write_to_client( my_json, client );
  // receive json
  my_json = read_from_client( client );
  return json_to_TR( my_json );
}

vector<TagResult> BaseTagger::tagLine( const icu::UnicodeString& line ){
  /// tag a string into a vector of TagResult elements
  /*!
    \param line a (UTF8 encoded) string, may be multilined and include Enrichments
    \return a vector of TagResult

    Depending on the configuration, the input is send to the local MBT tagger
    or the associated MBT server.
  */
  if ( !_host.empty() ){
    vector<tag_entry> to_do;
    vector<UnicodeString> v = TiCC::split( line );
    for ( const auto& w : v ){
      UnicodeString word = w;
      if ( filter ){
	word = filter->filter( word );
      }
      word = filter_spaces( word );
      tag_entry entry;
      entry.word = word;
      to_do.push_back( entry );
    }
    return tag_entries( to_do );
  }
  else if ( !tagger ){
    throw runtime_error( _label + "-tagger is not initialized" );
  }
  if ( debug > 1 ){
    DBG << "TAGGING LINE: " << line << endl;
  }
  return tagger->TagLine( line );
}

vector<TagResult> BaseTagger::tag_entries( const vector<tag_entry>& to_do ){
  /// tag a vector of teag_entry into a vector of TagResult elements
  /*!
    \param to_do a vector of tag_entry elements representing 1 sentence
  */
  if ( debug > 1 ){
    DBG << "TAGGING TEXT_BLOCK\n" << endl;
    for ( const auto& it : to_do ){
      DBG << it.word << "\t" << it.enrichment << "\t" << endl;
    }
  }
  if ( !_host.empty() ){
    DBG << "calling server" << endl;
    return call_server(to_do);
  }
  else {
    if ( !tagger ){
      throw runtime_error( _label + "-tagger is not initialized" );
    }
    UnicodeString block;
    for ( const auto& e: to_do ){
      block += e.word;
      if ( !e.enrichment.isEmpty() ){
	block += "\t" + e.enrichment;
	block += "\t??\n";
      }
      else {
	block += " ";
      }
    }
    block += "<utt>\n"; // should use tagger.eosmark??
    return tagger->TagLine( block );
  }
}

UnicodeString BaseTagger::set_eos_mark( const icu::UnicodeString& eos ){
  /// set the EOS marker for the tagger
  /*!
    \param eos the eos marker as a UnicodeString
    \return the old value
  */
  if ( !_host.empty() ){
    // just ignore??
    LOG << "cannot change EOS mark for external server!" << endl;
    return "";
  }
  if ( tagger ){
    return tagger->set_eos_mark( eos );
  }
  throw runtime_error( _label + "-tagger is not initialized" );
}

vector<tag_entry> BaseTagger::extract_sentence( const frog_data& sent ){
  /// extract a tag_entry list from a frog_data structure
  /*!
    \param sent the frog_data structure to convert
    \return a list of tag_entry elemements
  */
  vector<tag_entry> result;
  for ( const auto& sword : sent.units ){
    UnicodeString word = sword.word;
    if ( filter ){
      word = filter->filter( word );
    }
    word = filter_spaces( word );
    tag_entry entry;
    entry.word = word;
    result.push_back( entry );
  }
  return result;
}

ostream& operator<<( ostream& os, const tag_entry& e ){
  /// output a tag_entry (debugging only)
  /*!
    \param os the output stream
    \param e the element to output
   */
  os << e.word;
  if ( !e.enrichment.isEmpty() ){
    os << "\t" << e.enrichment;
  }
  os << endl;
  return os;
}

void BaseTagger::Classify( frog_data& sent ){
  /// Tag one sentence, give in frog_data format
  /*!
    \param sent the frog_data structure to analyze

    When tagging succeeds, 'sent' will be extended with the tag results
   */
  _words.clear();
  vector<tag_entry> to_do = extract_sentence( sent );
  if ( debug > 1 ){
    DBG << _label << "-tagger in: " << to_do << endl;
  }
  _tag_result = tag_entries( to_do );
  if ( _tag_result.size() != sent.size() ){
    LOG << _label << "-tagger mismatch between number of words and the tagger result." << endl;
    LOG << "words according to sentence: " << endl;
    for ( size_t w = 0; w < sent.size(); ++w ) {
      LOG << "w[" << w << "]= " << sent.units[w].word << endl;
    }
    LOG << "words according to " << _label << "-tagger: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      LOG << "word[" << i << "]=" << _tag_result[i].word() << endl;
    }
    throw runtime_error( _label + "-tagger is confused" );
  }
  if ( debug > 1 ){
    DBG << _label + "-tagger out: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      DBG << "[" << i << "] : word=" << _tag_result[i].word()
	  << " tag=" << _tag_result[i].assigned_tag()
	  << " confidence=" << _tag_result[i].confidence() << endl;
    }
  }
  post_process( sent );
}
