/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2022
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
using icu::UnicodeString;

#define LOG *TiCC::Log(errLog)
#define DBG *TiCC::Log(dbgLog)

/// create a Timbl based lemmatizer
/*!
  \param errlog a LogStream for errors
  \param dbglog a LogStream for debugging
*/
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
  /// read 'token-strip' rules from a file
  /*!
    \param file name of the inputfile
    \return true when no errors were detected

    token-strip rules look like this:
    \verbatim
    SPEC(deeleigen)	QUOTE-SUFFIX	1
    SPEC(deeleigen)	WORD-WITHSUFFIX	2
    \endverbatim

    What they express is this: When the tagger assigned a tag like
    SPEC(deeleigen) AND the tokenizer assigned a class of QUOTE-SUFFIX, then
    the lemma is given by stripping the last character. or last 2 for the second
    rule.

    This assures that the lemma for \e Jan's will be \e Jan and for \e Alex'
    will be \e Alex
   */
  ifstream is( file );
  if ( !is ){
    LOG << "Unable to open file: '" << file << "'" << endl;
    return false;
  }
  UnicodeString line;
  while ( TiCC::getline( is, line ) ){
    if ( line.isEmpty() || line[0] == '#' ){
      continue;
    }
    vector<UnicodeString> parts = TiCC::split( line );
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
  /// initialize the lemmatizer using the config
  /*!
    \param config the Configuration to use
    \return true when no problems are detected
  */
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

  UnicodeString one_one_tagS = TiCC::UnicodeFromUTF8(config.lookUp( "one_one_tags", "mblem" ));
  if ( !one_one_tagS.isEmpty() ){
    vector<UnicodeString> tags = TiCC::split_at( one_one_tagS, "," );
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

  if ( _host.empty() ){
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
  else {
    string mess = check_server( _host, _port, "MBLEM" );
    if ( !mess.empty() ){
      LOG << "FAILED to find an MBLEM server:" << endl;
      LOG << mess << endl;
      LOG << "timblserver not running??" << endl;
      return false;
    }
    else {
      LOG << "using MBLEM Timbl on " << _host << ":" << _port << endl;
      return true;
    }
  }
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

UnicodeString Mblem::make_instance( const UnicodeString& in ) {
  /// convert a Unicode string into an instance for Timbl
  /*!
    \param in the UnicodeString representing 1 word to lemmatize
    \return a UnicodeString with an instance we can feed to Timbl
  */
  if (debug > 2 ) {
    DBG << "making instance from: " << in << endl;
  }
  UnicodeString instance = "";
  size_t length = in.length();
  for ( size_t i=0; i < history; i++) {
    size_t j = length - history + i;
    if ((  i < history - length )
	&& ( length<history ) ) {
      instance += "=\t";
    }
    else {
      instance += in[j];
      instance += '\t';
    }
  }
  instance += "?";
  if ( debug > 2 ){
    DBG << "inst: " << instance << endl;
  }
  return instance;
}

void Mblem::filterTag( const UnicodeString& postag ){
  /// filter all non-matching tags out of the mblem results
  /*!
    \param postag the tag, given by the CGN-tagger, that should match

    Mblem produces a range of possible solutions with tags. We use the POS tag
    given by the CGN tagger to remove all solutions with a different tag
  */
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    UnicodeString tag = it->getTag();
    if ( postag == tag ){
      if ( debug > 1 ){
	DBG << "compare cgn-tag " << postag << " with mblem-tag " << tag
	    << "\n\t==> identical tags. KEEP"  << endl;
      }
      ++it;
    }
    else {
      if ( debug > 1 ){
	DBG << "compare cgn-tag " << postag << " with mblem-tag " << tag
		       << "\n\t==> different tags. REMOVE" << endl;
      }
      it = mblemResult.erase(it);
    }
  }
  if ( (debug > 1) && mblemResult.empty() ){
    DBG << "NO CORRESPONDING TAG! " << postag << endl;
  }
}

void Mblem::makeUnique( ){
  /// filter out all results that are equal
  /*
    should be called AFTER filterTag() and cleans out doubles
  */
  auto it = mblemResult.begin();
  while( it != mblemResult.end() ){
    UnicodeString lemma = it->getLemma();
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
  /// add provenance information to the FoLiA document
  /*!
    \param doc the foLiA document we are working on
    \param main the main processor (presumably Frog) we want to add a new one to
  */
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
  /// add lemma information to the frog_data
  /*!
    \param fd The frog_data

    this handles some special cases like ABBREVIATION, the token-strip rules
    and the one-one rules.
    All 'normal' cases are handled over to the Timbl classifier
  */
  UnicodeString uword = fd.word;
  UnicodeString pos_tag = fd.tag;
  UnicodeString token_class = fd.token_class;
  if (debug > 1 ){
    DBG << "Classify " << uword << "(" << pos_tag << ") ["
	<< token_class << "]" << endl;
  }
  if ( filter ){
    uword = filter->filter( uword );
  }
  if ( token_class == "ABBREVIATION" ){
    // We dont handle ABBREVIATION's so just take the word as such
#pragma omp critical (dataupdate)
    {
      fd.lemmas.push_back( uword );
    }
    return;
  }

  auto const& it1 = token_strip_map.find( pos_tag );
  if ( it1 != token_strip_map.end() ){
    // some tag/tokenizer_class combinations are special
    // we have to strip a few letters to get a lemma
    auto const& it2 = it1->second.find( token_class );
    if ( it2 != it1->second.end() ){
      UnicodeString uword2 = UnicodeString( uword, 0, uword.length() - it2->second );
      if ( uword2.isEmpty() ){
	uword2 = uword;
      }
#pragma omp critical (dataupdate)
      {
	fd.lemmas.push_back( uword2 );
      }
      return;
    }
  }
  if ( one_one_tags.find( pos_tag ) != one_one_tags.end() ){
    // some tags are just taken as such
#pragma omp critical (dataupdate)
    {
      fd.lemmas.push_back( uword );
    }
    return;
  }
  if ( !keep_case ){
    uword.toLower();
  }
  Classify( uword );
  filterTag( pos_tag );
  makeUnique();
  if ( mblemResult.empty() ){
    // just return the word as a lemma
#pragma omp critical (dataupdate)
    {
      fd.lemmas.push_back( uword );
    }
  }
  else {
#pragma omp critical (dataupdate)
    {
      for ( auto const& it : mblemResult ){
	UnicodeString result = it.getLemma();
	fd.lemmas.push_back( result );
      }
    }
  }
}

UnicodeString Mblem::call_server( const UnicodeString& instance ){
  /// use a Timbl server to classify
  /*!
    \param instance The instance to give to Timbl
    \return the lemma rule as returned by Timbl
  */
  Sockets::ClientSocket client;
  if ( !client.connect( _host, _port ) ){
    LOG << "failed to open connection, " << _host
	<< ":" << _port << endl
	<< "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  if ( debug > 1 ){
    DBG << "calling MBLEM-server" << endl;
  }
  string in_line;
  client.read( in_line );
  json response;
  try {
    response = json::parse( in_line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << in_line << "':"
	<< e.what() << endl;
    abort();
  }
  //  LOG << "received json data:" << response.dump(2) << endl;
  if ( !_base.empty() ){
    json out_json;
    out_json["command"] = "base";
    out_json["param"] = _base;
    string out_line = out_json.dump() + "\n";
    //    LOG << "sending BASE json data:" << out_line << endl;
    client.write( out_line );
    client.read( in_line );
    try {
      response = json::parse( in_line );
    }
    catch ( const exception& e ){
      LOG << "json parsing failed on '" << in_line << "':"
	  << e.what() << endl;
      abort();
    }
    //    LOG << "received json data:" << response.dump(2) << endl;
  }
  // create json query struct
  json query;
  query["command"] = "classify";
  query["param"] = TiCC::UnicodeToUTF8(instance);
  //  LOG << "send json" << query.dump(2) << endl;
  // send it to the server
  string out_line = query.dump() + "\n";
  client.write( out_line );
  // receive json
  client.read( in_line );
  //  LOG << "received line:" << in_line << "" << endl;
  try {
    response = json::parse( in_line );
  }
  catch ( const exception& e ){
    LOG << "json parsing failed on '" << in_line << "':"
	<< e.what() << endl;
    abort();
  }
  //  LOG << "received json data:" << response.dump(2) << endl;
  string result = response["category"];
  //  LOG << "extracted result " << result << endl;
  return TiCC::UnicodeFromUTF8(result);
}

void Mblem::Classify( const UnicodeString& word ){
  /// give the lemma for 1 word
  /*!
    \param uWord a Unicode string with the word
    the internal mblemResult struct will be filled with 1 or more (alternative)
    solutions of a lemma + a POS-tag
  */
  static TiCC::UnicodeNormalizer nfc_norm;
  UnicodeString uWord = nfc_norm.normalize(word);
  mblemResult.clear();
  UnicodeString inst = make_instance(uWord);
  UnicodeString u_class;
  if ( !_host.empty() ){
    u_class = call_server( inst );
  }
  else {
    myLex->Classify( inst, u_class );
  }
  if ( debug > 1){
    DBG << "class: " << u_class  << endl;
  }
  // 1st find all alternatives
  vector<UnicodeString> parts = TiCC::split_at_first_of( u_class, "|" );
  if ( parts.size() < 1 ){
    LOG << "no alternatives found" << endl;
  }
  for ( const auto& partS : parts ) {
    UnicodeString lemma;
    UnicodeString restag;
    int pos = partS.indexOf("+");
    if ( pos == -1 ){
      // nothing to edit
      restag = partS;
      lemma = uWord;
    }
    else {
      // some edit info available, like: WW(27)+Dgekomen+Ikomen
      vector<UnicodeString> edits = TiCC::split_at( partS, "+" );
      if ( edits.empty() ){
	throw runtime_error( "invalid editstring: "
			     + TiCC::UnicodeToUTF8(partS ) );
      }
      restag = edits[0]; // the first one is the POS tag

      UnicodeString insstr;
      UnicodeString delstr;
      UnicodeString prefix;
      for ( const auto& edit : edits ){
	if ( edit == edits.front() ){
	  continue;
	}
	UnicodeString edit_val = UnicodeString( edit, 1 );
	switch ( edit[0] ){
	case 'P':
	  prefix = edit_val;
	  break;
	case 'I':
	  insstr = edit_val;
	  break;
	case 'D':
	  delstr = edit_val;
	  break;
	default:
	  LOG << "Error: strange value [" << edit[0] << "] in editstring: "
	      << edit << endl;
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
	lemma = UnicodeString( uWord, 0L, prefixpos );
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
	    UnicodeString part = UnicodeString( uWord, prefixpos, uWord.length() - delstr.length() - prefixpos );
	    lemma += part + insstr;
	  }
	}
	else if ( insstr.isEmpty() ){
	  // no replacement, just take part after the prefix
	  lemma += UnicodeString( uWord, prefixpos, uWord.length() ); // uWord;
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
    if ( debug > 1 ){
      DBG << "appending lemma " << lemma << " and tag " << restag << endl;
    }
    mblemResult.push_back( mblemData( lemma, restag ) );
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

vector<pair<UnicodeString,UnicodeString> > Mblem::getResult() const {
  /// extract the results into a list of lemma/tag pairs
  vector<pair<UnicodeString,UnicodeString> > result;
  for ( const auto& mbr : mblemResult ){
    result.push_back( make_pair( mbr.getLemma(),
				 mbr.getTag() ) );
  }
  return result;
}

void Mblem::add_lemmas( const vector<folia::Word*>& wv,
			const frog_data& fd ) const {
  /// add the lemma from 'fd' to the FoLiA list of Word
  /*!
    \param wv The folia:Word vector
    \param fd the folia_data with added lemmatizer results
  */
  ///
  for ( size_t i=0; i < wv.size(); ++i ){
    folia::KWargs args;
    args["set"] = getTagset();
    for ( const auto& lemma : fd.units[i].lemmas ){
      args["class"] = TiCC::UnicodeToUTF8(lemma);
      if ( textclass != "current" ){
	args["textclass"] = textclass;
      }
#pragma omp critical (foliaupdate)
      {
	wv[i]->addLemmaAnnotation( args ); // will create Alternative nodes
	// when there are several lemma's found
      }
    }
  }
}
