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

#include "frog/mblem_mod.h"

#include <string>
#include <iostream>
#include <fstream>
#include "timbl/TimblAPI.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/SocketBasics.h"
#include "ticcutils/json.hpp"
#include "frog/Frog-util.h"

using namespace std;
using namespace nlohmann;

#define LOG *TiCC::Log(errLog)
#define DBG *TiCC::Log(dbgLog)

Mblem::Mblem( TiCC::LogStream *errlog, TiCC::LogStream *dbglog ):
  myLex(0),
  punctuation( "?...,:;\\'`(){}[]%#+-_=/!" ),
  history(20),
  debug(0),
  keep_case( false ),
  filter(0)
{
  errLog = new TiCC::LogStream( errlog, "mblem-" );
  if ( dbglog ){
    dbgLog = new TiCC::LogStream( dbglog, "mblem" );
  }
  else {
    dbgLog = errLog;
  }
}

bool Mblem::fill_ts_map( const string& file ){
  ifstream is( file );
  if ( !is ){
    LOG << "Unable to open file: '" << file << "'" << endl;
    return false;
  }
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    vector<string> parts = TiCC::split( line );
    if ( parts.size() != 3 ){
      LOG << "invalid line in: '" << file << "' (expected 3 parts)" << endl;
      return false;
    }
    token_strip_map[parts[0]].insert( make_pair( parts[1], TiCC::stringTo<int>( parts[2] ) ) );
  }
  if ( debug > 1 ){
    DBG << "read token strip rules from: '" << file << "'" << endl;
  }
  return true;
}

bool Mblem::init( const TiCC::Configuration& config ) {
  LOG << "Initiating lemmatizer..." << endl;
  debug = 0;
  string val = config.lookUp( "host", "mblem" );
  if ( !val.empty() ){
    // assume we must use a Timbl Server for the MBLEM
    _host = val;
    val = config.lookUp( "port", "mblem" );
    if ( val.empty() ){
      LOG << "missing 'port' settings for host= " << _host << endl;
      return false;
    }
    _port = val;
    val = config.lookUp( "base", "mblem" );
    if ( val.empty() ){
      LOG << "missing 'base' settings for host= " << _host << endl;
      return false;
    }
    _base = val;
  }
  else {
    val = config.lookUp( "base", "mblem" ) + ":"
      + config.lookUp( "port", "mblem" );
    if ( val.size() > 1 ){
      LOG << "missing 'host' settings for base:port " << val << endl;
      return false;
    }
  }
  val = config.lookUp( "debug", "mblem" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "version", "mblem" );
  if ( val.empty() ){
    _version = "1.0";
  }
  else {
    _version = val;
  }
  val = config.lookUp( "set", "mblem" );
  if ( val.empty() ){
    tagset = "http://ilk.uvt.nl/folia/sets/frog-mblem-nl";
  }
  else {
    tagset = val;
  }
  val = config.lookUp( "set", "tagger" );
  if ( val.empty() ){
    POS_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
  }
  else {
    POS_tagset = val;
  }
  string transName = config.lookUp( "transFile", "mblem" );
  if ( !transName.empty() ){
    transName = prefix( config.configDir(), transName );
    cerr << "usage of a mblem transFile is no longer needed!" << endl;
    cerr << "skipping : " << transName << endl;
  }
  string treeName = config.lookUp( "treeFile", "mblem"  );
  if ( treeName.empty() ){
    treeName = "mblem.tree";
  }
  treeName = prefix( config.configDir(), treeName );

  string charFile = config.lookUp( "char_filter_file", "mblem" );
  if ( charFile.empty() ){
    charFile = config.lookUp( "char_filter_file" );
  }
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new TiCC::UniFilter();
    filter->fill( charFile );
  }

  string tokenStripFile = config.lookUp( "token_strip_file", "mblem" );
  if ( !tokenStripFile.empty() ){
    tokenStripFile = prefix( config.configDir(), tokenStripFile );
    if ( !fill_ts_map( tokenStripFile ) ){
      return false;
    }
  }

  string one_one_tagS = config.lookUp( "one_one_tags", "mblem" );
  if ( !one_one_tagS.empty() ){
    vector<string> tags = TiCC::split_at( one_one_tagS, "," );
    for ( auto const& t : tags ){
      one_one_tags.insert( t );
    }
  }

  string par = config.lookUp( "keep_case", "mblem" );
  if ( !par.empty() ){
    keep_case = TiCC::stringTo<bool>( par );
  }

  string cls = config.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }

  string opts = config.lookUp( "timblOpts", "mblem" );
  if ( opts.empty() ){
    opts = "-a1";
  }
  // make it silent
  opts += " +vs -vf -F TABBED";
  //Read in (igtree) data
  myLex = new Timbl::TimblAPI(opts);
  return myLex->GetInstanceBase(treeName);
}

Mblem::~Mblem(){
  //    LOG << "cleaning up MBLEM stuff" << endl;
  delete filter;
  delete myLex;
  myLex = 0;
  if ( errLog != dbgLog ){
    delete dbgLog;
  }
  delete errLog;
}

string Mblem::make_instance( const icu::UnicodeString& in ) {
  if (debug > 2 ) {
    DBG << "making instance from: " << in << endl;
  }
  icu::UnicodeString instance = "";
  size_t length = in.length();
  for ( size_t i=0; i < history; i++) {
    size_t j = length - history + i;
    if (( i < history - length ) &&
	(length<history))
      instance += "=\t";
    else {
      instance += in[j];
      instance += '\t';
    }
  }
  instance += "?";
  string result = TiCC::UnicodeToUTF8(instance);
  if ( debug > 2 ){
    DBG << "inst: " << instance << endl;
  }
  return result;
}

void Mblem::filterTag( const string& postag ){
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    string tag = it->getTag();
    if ( postag == tag ){
      if ( debug > 1 ){
	DBG << "compare cgn-tag " << postag << " with mblem-tag " << tag
	    << "\n\t==> identical tags"  << endl;
      }
      ++it;
    }
    else {
      if ( debug > 1 ){
	DBG << "compare cgn-tag " << postag << " with mblem-tag " << tag
		       << "\n\t==> different tags" << endl;
      }
      it = mblemResult.erase(it);
    }
  }
  if ( (debug > 1) && mblemResult.empty() ){
    DBG << "NO CORRESPONDING TAG! " << postag << endl;
  }
}

void Mblem::makeUnique( ){
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    string lemma = it->getLemma();
    auto it2 = it+1;
    while( it2 != mblemResult.end() ){
      if (debug > 1){
	DBG << "compare lemma " << lemma << " with " << it2->getLemma() << " ";
      }
      if ( lemma == it2->getLemma() ){
	if ( debug > 1){
	  DBG << "equal " << endl;
	}
	it2 = mblemResult.erase(it2);
      }
      else {
	if ( debug > 1){
	  DBG << "NOT equal! " << endl;
	}
	++it2;
      }
    }
    ++it;
  }
  if (debug > 1){
    DBG << "final result after filter and unique" << endl;
    for ( const auto& mbr : mblemResult ){
      DBG << "lemma alt: " << mbr.getLemma()
	  << "\ttag alt: " << mbr.getTag() << endl;
    }
  }
}

void Mblem::add_provenance( folia::Document& doc,
			    folia::processor *main ) const {
  string _label = "mblem";
  if ( !main ){
    throw logic_error( "mblem::add_provenance() without arguments." );
  }
  folia::KWargs args;
  args["name"] = _label;
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  args.clear();
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::LEMMA, tagset, args );
}

void Mblem::Classify( frog_record& fd ){
  icu::UnicodeString uword;
  string pos;
  string token_class;
  uword = TiCC::UnicodeFromUTF8(fd.word);
  pos = fd.tag;
  token_class = fd.token_class;
  if (debug > 1 ){
    DBG << "Classify " << uword << "(" << pos << ") ["
	<< token_class << "]" << endl;
  }
  if ( filter ){
    uword = filter->filter( uword );
  }
  if ( token_class == "ABBREVIATION" ){
    // We dont handle ABBREVIATION's so just take the word as such
    string word = TiCC::UnicodeToUTF8(uword);
#pragma omp critical (dataupdate)
    {
      fd.lemmas.push_back( word );
    }
    return;
  }
  auto const& it1 = token_strip_map.find( pos );
  if ( it1 != token_strip_map.end() ){
    // some tag/tokenizer_class combinations are special
    // we have to strip a few letters to get a lemma
    auto const& it2 = it1->second.find( token_class );
    if ( it2 != it1->second.end() ){
      icu::UnicodeString uword2 = icu::UnicodeString( uword, 0, uword.length() - it2->second );
      if ( uword2.isEmpty() ){
	uword2 = uword;
      }
      string word = TiCC::UnicodeToUTF8(uword2);
#pragma omp critical (dataupdate)
      {
	fd.lemmas.push_back( word );
      }
      return;
    }
  }
  if ( one_one_tags.find(pos) != one_one_tags.end() ){
    // some tags are just taken as such
    string word = TiCC::UnicodeToUTF8(uword);
#pragma omp critical (dataupdate)
    {
      fd.lemmas.push_back( word );
    }
    return;
  }
  if ( !keep_case ){
    uword.toLower();
  }
  Classify( uword );
  filterTag( pos );
  makeUnique();
  if ( mblemResult.empty() ){
    // just return the word as a lemma
    string word = TiCC::UnicodeToUTF8(uword);
#pragma omp critical (dataupdate)
    {
      fd.lemmas.push_back( word );
    }
  }
  else {
#pragma omp critical (dataupdate)
    {
      for ( auto const& it : mblemResult ){
	string result = it.getLemma();
	fd.lemmas.push_back( result );
      }
    }
  }
}

string Mblem::call_server( const string& instance ){
  Sockets::ClientSocket client;
  if ( !client.connect( _host, _port ) ){
    LOG << "failed to open connection, " << _host
	<< ":" << _port << endl
	<< "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  DBG << "calling MBLEM-server" << endl;
  string line;
  client.read( line );
  json response;
  try {
    response = json::parse( line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << line << "':"
	<< e.what() << endl;
    abort();
  }
  //  LOG << "received json data:" << response.dump(2) << endl;
  if ( !_base.empty() ){
    json out_json;
    out_json["command"] = "base";
    out_json["param"] = _base;
    string line = out_json.dump() + "\n";
    //    LOG << "sending BASE json data:" << line << endl;
    client.write( line );
    client.read( line );
    json response;
    try {
      response = json::parse( line );
    }
    catch ( const exception& e ){
      LOG << "json parsing failed on '" << line << "':"
	  << e.what() << endl;
      abort();
    }
    //    LOG << "received json data:" << response.dump(2) << endl;
  }
  // create json struct
  json query;
  query["command"] = "classify";
  query["param"] = instance;
  //  LOG << "send json" << query.dump(2) << endl;
  // send it to the server
  line = query.dump() + "\n";
  client.write( line );
  // receive json
  client.read( line );
  //  LOG << "received line:" << line << "" << endl;
  try {
    response = json::parse( line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << line << "':"
	<< e.what() << endl;
    abort();
  }
  //  LOG << "received json data:" << response.dump(2) << endl;
  string result = response["category"];
  //  LOG << "extracted result " << result << endl;
  return result;
}

void Mblem::Classify( const icu::UnicodeString& uWord ){
  mblemResult.clear();
  string inst = make_instance(uWord);
  string classString;
  if ( !_host.empty() ){
    classString = call_server( inst );
  }
  else {
    myLex->Classify( inst, classString );
  }
  if ( debug > 1){
    DBG << "class: " << classString  << endl;
  }
  // 1st find all alternatives
  vector<string> parts;
  int numParts = TiCC::split_at( classString, parts, "|" );
  if ( numParts < 1 ){
    LOG << "no alternatives found" << endl;
  }
  int index = 0;
  while ( index < numParts ) {
    string partS = parts[index++];
    icu::UnicodeString lemma;
    string restag;
    string::size_type pos = partS.find("+");
    if ( pos == string::npos ){
      // nothing to edit
      restag = partS;
      lemma = uWord;
    }
    else {
      // some edit info available, like: WW(27)+Dgekomen+Ikomen
      vector<string> edits = TiCC::split_at( partS, "+" );
      if ( edits.empty() ){
	throw runtime_error( "invalid editstring: " + partS );
      }
      restag = edits[0]; // the first one is the POS tag

      icu::UnicodeString insstr;
      icu::UnicodeString delstr;
      icu::UnicodeString prefix;
      for ( const auto& edit : edits ){
	if ( edit == edits.front() ){
	  continue;
	}
	switch ( edit[0] ){
	case 'P':
	  prefix = TiCC::UnicodeFromUTF8( edit.substr( 1 ) );
	  break;
	case 'I':
	  insstr = TiCC::UnicodeFromUTF8( edit.substr( 1 ) );
	  break;
	case 'D':
	  delstr = TiCC::UnicodeFromUTF8( edit.substr( 1 ) );
	  break;
	default:
	  LOG << "Error: strange value in editstring: " << edit
			 << endl;
	}
      }
      if (debug > 1){
	DBG << "pre-prefix word: '" << uWord << "' prefix: '"
	    << prefix << "'" << endl;
      }
      int prefixpos = 0;
      if ( !prefix.isEmpty() ) {
	// Whenever Toads makemblem is improved, (the infamous
	// 'tegemoetgekomen' example), this should probably
	// become prefixpos = uWord.lastIndexOf(prefix);
	prefixpos = uWord.indexOf(prefix);
	if (debug > 1){
	  DBG << "prefixpos = " << prefixpos << endl;
	}
	// repair cases where there's actually not a prefix present
	if (prefixpos > uWord.length()-2) {
	  prefixpos = 0;
	  prefix.remove();
	}
      }

      if (debug > 1){
	DBG << "prefixpos = " << prefixpos << endl;
      }
      if (prefixpos >= 0) {
	lemma = icu::UnicodeString( uWord, 0L, prefixpos );
	prefixpos = prefixpos + prefix.length();
      }
      if (debug > 1){
	DBG << "post word: "<< uWord
	    << " lemma: " << lemma
	    << " prefix: " << prefix
	    << " delstr: " << delstr
	    << " insstr: " << insstr
	    << endl;
      }
      if ( uWord.endsWith( delstr ) ){
	if ( uWord.length() > delstr.length() ){
	  // chop delstr from the back, but do not delete the whole word
	  if ( uWord.length() == delstr.length() + 1  && insstr.isEmpty() ){
	    // do not mangle short words too
	    lemma += uWord;
	  }
	  else {
	    icu::UnicodeString part = icu::UnicodeString( uWord, prefixpos, uWord.length() - delstr.length() - prefixpos );
	    lemma += part + insstr;
	  }
	}
	else if ( insstr.isEmpty() ){
	  // no replacement, just take part after the prefix
	  lemma += icu::UnicodeString( uWord, prefixpos, uWord.length() ); // uWord;
	}
	else {
	  // but replace if possible
	  lemma += insstr;
	}
      }
      else if ( lemma.isEmpty() ){
	lemma = uWord;
      }
    }
    if ( !classMap.empty() ){
      // translate TAG(number) stuf back to CGN things
      auto const& it = classMap.find(restag);
      if ( debug > 1 ){
	DBG << "looking up " << restag << endl;
      }
      if ( it != classMap.end() ){
	restag = it->second;
	if ( debug > 1 ){
	  DBG << "found " << restag << endl;
	}
      }
      else {
	LOG << "problem: found no translation for "
	    << restag << " using it 'as-is'" << endl;
      }
    }
    if ( debug > 1 ){
      DBG << "appending lemma " << lemma << " and tag " << restag << endl;
    }
    mblemResult.push_back( mblemData( TiCC::UnicodeToUTF8(lemma), restag ) );
  } // while
  if ( debug > 1) {
    DBG << "stored lemma and tag options: " << mblemResult.size()
	<< " lemma's and " << mblemResult.size() << " tags:" << endl;
    for ( const auto& mbr : mblemResult ){
      DBG << "lemma alt: " << mbr.getLemma()
	  << "\ttag alt: " << mbr.getTag() << endl;
    }
  }
}

vector<pair<string,string> > Mblem::getResult() const {
  vector<pair<string,string> > result;
  for ( const auto& mbr : mblemResult ){
    result.push_back( make_pair( mbr.getLemma(),
				 mbr.getTag() ) );
  }
  return result;
}

void Mblem::add_lemmas( const vector<folia::Word*>& wv,
			const frog_data& fd ) const {
  for ( size_t i=0; i < wv.size(); ++i ){
    folia::KWargs args;
    args["set"] = getTagset();
    for ( const auto& lemma : fd.units[i].lemmas ){
      args["class"] = lemma;
      if ( textclass != "current" ){
	args["textclass"] = textclass;
      }
#pragma omp critical (foliaupdate)
      {
	wv[i]->addLemmaAnnotation( args );
      }
    }
  }
}
