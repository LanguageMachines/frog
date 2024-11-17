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

#include "frog/mbma_mod.h"

#include <cstdlib>
#include <string>
#include <set>
#include <iostream>
#include <algorithm>
#include <fstream>

#include "timbl/TimblAPI.h"

#include "ticcutils/Configuration.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/SocketBasics.h"
#include "ticcutils/json.hpp"
#include "frog/Frog-util.h"
#include "frog/FrogData.h"

using namespace std;
using namespace icu;
using namespace nlohmann;
using TiCC::operator<<;

const long int LEFT =  6; // left context
const long int RIGHT = 6; // right context

#define LOG *TiCC::Log(errLog)
#define DBG *TiCC::Log(dbgLog)

Mbma::Mbma( TiCC::LogStream *errlog, TiCC::LogStream *dbglog ):
  MTree(0),
  filter(0),
  debugFlag(0),
  filter_diac(false),
  doDeepMorph(false)
{
  /// create an Mbma classifier object
  /*!
    \param errlog the LogStream to use for Error messages
    \param dbglog the LogStream to use for Debug messages
  */
  errLog = new TiCC::LogStream( errlog );
  errLog->add_message( "mbma-" );
  if ( dbglog ){
    dbgLog = new TiCC::LogStream( dbglog );
    dbgLog->add_message( "mbma-" );
  }
  else {
    dbgLog = errLog;
  }
}

// define the statics
map<UnicodeString,UnicodeString> Mbma::TAGconv;
string Mbma::mbma_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbma-nl";
string Mbma::pos_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
string Mbma::clex_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-clex";

Mbma::~Mbma() {
  /// the mbma destructor
  delete MTree;
  clearAnalysis();
  delete filter;
  if ( errLog != dbgLog ){
    delete dbgLog;
  }
  delete errLog;
}


void Mbma::init_cgn( const string& main, const string& sub ) {
  /// initalize the CGN-CLEX translation tables
  /*!
    \param main the name of the main-rules file
    \param sub the name of the sub-rules file

    the main file gives translations from CGN tags to CELEX tags, like:
    \verbatim
    N N
    ADJ A
    TW Q
    WW V
    \endverbatim

    the sub file gives translations from CGN subtags to CELEX attributes, like:
    \verbatim
    comp C
    conj
    dat D
    deeleigen
    det
    dial
    dim d
    \endverbatim

    a single valued entry means that NO translation is available
  */
  ifstream tc( main );
  if ( tc ){
    UnicodeString line;
    while ( TiCC::getline( tc, line) ) {
      vector<UnicodeString> tmp = TiCC::split_at( line, " " );
      if ( tmp.size() < 2 ){
	LOG << "splitting '" << line << "' failed" << endl;
	throw ( runtime_error("panic") );
      }
      TAGconv.insert( make_pair( tmp[0], tmp[1] ) );
    }
  }
  else {
    throw ( runtime_error( "unable to open: " + main ) );
  }
  ifstream tc1( sub );
  if ( tc1 ){
    UnicodeString line;
    while( TiCC::getline( tc1, line ) ) {
      vector<UnicodeString> tmp = TiCC::split_at( line, " " );
      if ( tmp.size() == 2 ){
	TAGconv.insert( make_pair( tmp[0], tmp[1] ) );
      }
    }
  }
  else {
    throw ( runtime_error( "unable to open:" + sub ) );
  }
}

bool Mbma::init( const TiCC::Configuration& config ) {
  /// initialize the Mbma analyzer using the config
  /*!
    \param config the Configuration to use
    \return true when no problems are detected
  */
  LOG << "Initiating morphological analyzer..." << endl;
  debugFlag = 0;
  string val = config.lookUp( "host", "mbma" );
  if ( !val.empty() ){
    // assume we must use a Timbl Server for the MBMA
    _host = val;
    val = config.lookUp( "port", "mbma" );
    if ( val.empty() ){
      LOG << "missing 'port' settings for host= " << _host << endl;
      return false;
    }
    _port = val;
    val = config.lookUp( "base", "mbma" );
    if ( val.empty() ){
      LOG << "missing 'base' settings for host= " << _host << endl;
      return false;
    }
    _base = val;
  }
  else {
    val = config.lookUp( "base", "mbma" ) + ":"
      + config.lookUp( "port", "mbma" );
    if ( val.size() > 1 ){
      LOG << "missing 'host' settings for base:port " << val << endl;
      return false;
    }
  }
  val = config.lookUp( "debug", "mbma" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debugFlag = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "deep-morph", "mbma" );
  if ( !val.empty() ){
    doDeepMorph = TiCC::stringTo<bool>( val );
  }
  val = config.lookUp( "version", "mbma" );
  if ( val.empty() ){
    _version = "1.0";
  }
  else {
    _version = val;
  }
  val = config.lookUp( "set", "mbma" );
  if ( !val.empty() ){
    mbma_tagset = val;
  }

  val = config.lookUp( "set", "tagger" );
  if ( !val.empty() ){
    pos_tagset = val;
  }
  val = config.lookUp( "clex_set", "mbma" );
  if ( !val.empty() ){
    clex_tagset = val;
  }
  string cgn_clex_main;
  val = config.lookUp( "cgn_clex_main", "mbma" );
  if ( val.empty() ){
    cgn_clex_main = "cgntags.main";
  }
  else {
    cgn_clex_main = val;
  }
  cgn_clex_main = prefix( config.configDir(), cgn_clex_main );

  string cgn_clex_sub;
  val = config.lookUp( "cgn_clex_sub", "mbma" );
  if ( val.empty() ){
    cgn_clex_sub = "cgntags.sub";
  }
  else {
    cgn_clex_sub = val;
  }
  cgn_clex_sub = prefix( config.configDir(), cgn_clex_sub );
  init_cgn( cgn_clex_main, cgn_clex_sub );

  string charFile = config.lookUp( "char_filter_file", "mbma" );
  if ( charFile.empty() ){
    charFile = config.lookUp( "char_filter_file" );
  }
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new TiCC::UniFilter();
    filter->fill( charFile );
  }
  string dof = config.lookUp( "filter_diacritics", "mbma" );
  if ( !dof.empty() ){
    filter_diac = TiCC::stringTo<bool>( dof );
  }

  string cls = config.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }

  if ( _host.empty() ){
    // so classic monolytic run
    string tfName = config.lookUp( "treeFile", "mbma" );
    if ( tfName.empty() ){
      tfName = "mbma.igtree"; // the default
    }
    MTreeFilename = prefix( config.configDir(), tfName );
    //Read in (igtree) data
    string opts = config.lookUp( "timblOpts", "mbma" );
    if ( opts.empty() ){
      opts = "-a1";
    }
    opts += " +vs -vf"; // make Timbl run quietly
    MTree = new Timbl::TimblAPI(opts);
    return MTree->GetInstanceBase(MTreeFilename);
  }
  else {
    string mess = check_server( _host, _port, "MBMA" );
    if ( !mess.empty() ){
      LOG << "FAILED to find an MBMA server:" << endl;
      LOG << mess << endl;
      LOG << "timblserver not running??" << endl;
      return false;
    }
    else {
      LOG << "using MBMA Timbl on " << _host << ":" << _port << endl;
      return true;
    }
  }
}

vector<UnicodeString> Mbma::make_instances( const UnicodeString& word ){
  /// convert a Unicode string into a range of UTF8 instances for Timbl
  /*!
    \param word the UnicodeString representing 1 word to analyze
    \return a vector of UTF8 stringa with instances for Timbl
  */
  vector<UnicodeString> insts;
  insts.reserve( word.length() );
  for ( long i=0; i < word.length(); ++i ) {
    if (debugFlag > 10){
      DBG << "itt #:" << i << endl;
    }
    UnicodeString inst;
    for ( long j=i ; j <= i + RIGHT + LEFT; ++j ) {
      if (debugFlag > 10){
	DBG << " " << j-LEFT << ": ";
      }
      if ( j < LEFT || j >= word.length()+LEFT ){
	inst += '_';
      }
      else {
	if (word[j-LEFT] == ',' ){
	  inst += 'C';
	}
	else {
	  inst += word[j-LEFT];
	}
      }
      inst += ",";
      if (debugFlag > 10){
	DBG << " : " << inst << endl;
      }
    }
    inst += "?";
    insts.push_back( inst );
  }
  return insts;
}

UnicodeString find_class( unsigned int step,
			  const vector<UnicodeString>& classes,
			  unsigned int nranal ){
  UnicodeString result = classes[0];
  if ( nranal > 1 ){
    if ( classes.size() > 1 ){
      if ( classes.size() > step ){
	result = classes[step];
      }
      else {
	result = classes.back();
      }
    }
  }
  return result;
}

vector<vector<UnicodeString> > generate_all_perms( const vector<UnicodeString>& classes ){
  /// determine all alternative analyses, remember the largest
  /// and store every part in a vector of string vectors
  /*!
    \param classes A vector of possibilities generated by Timbl

    example: Timbl has given 3 results for the word 'gek':

    classes={"A|N|V","0","0/P|0/e|0/te1|0/te2I"} (from the word 'gek')

    This is converted to
    { {"A","N","V","V"}, {"0","0","0","0" }, {"0/P","0/e","0/te1","0/te2I"} }

    As we can see, the "0" from classes is expanded by duplication 4 times.
    The "A|N|V" is expanded by duplicating the last "V".

    an then for index 1 to 4 the results are collected:

    { {"A","0","0/P"}, {N","0", "0/e"}, {"V","0", "0/te1"}, {"V", "0","0/te2I"} }
   */
  size_t largest_anal=1;
  vector<vector<UnicodeString> > classParts;
  classParts.reserve( classes.size() );
  for ( const auto& uclass : classes ){
    vector<UnicodeString> parts = TiCC::split_at( uclass, "|" );
    size_t num = parts.size();
    if ( num > 1 ){
      classParts.push_back( parts );
      if ( num > largest_anal ){
	largest_anal = num;
      }
    }
    else {
      // only one, create a dummy
      vector<UnicodeString> dummy;
      dummy.push_back( uclass );
      classParts.push_back( dummy );
    }
  }
  //
  // now expand the result
  vector<vector<UnicodeString> > result;
  result.reserve( largest_anal );
  for ( size_t step=0; step < largest_anal; ++step ){
    vector<UnicodeString> item;
    for ( const auto& cp : classParts ){
      item.push_back( find_class( step, cp, largest_anal ) );
    }
    result.push_back( item );
  }
  return result;
}

void Mbma::clearAnalysis(){
  for ( const auto& a: analysis ){
    delete a;
  }
  analysis.clear();
}

Rule* Mbma::matchRule( const std::vector<icu::UnicodeString>& ana,
		       const icu::UnicodeString& word,
		       bool keep_V2I ){
  /// attempt to match an Analysis on a word
  /*!
    \param ana one analysis result, expanded from the Timbl classifier
    \param word a Unicode Word to check
    \param keep_V2I a boolean that determines if we want to keep Inversed
                    second person variants.
    \return a matched Rule or 0
  */
  Rule *rule = new Rule( ana, word, *errLog, *dbgLog, debugFlag );
  if ( rule->performEdits() ){
    rule->reduceZeroNodes();
    if ( debugFlag > 1 ){
      DBG << "after reduction: " << rule << endl;
    }
    rule->resolve_inflections();
    if ( debugFlag > 1 ){
      DBG << "after resolving: " << rule << endl;
    }
    rule->getCleanInflect( keep_V2I );
    if ( debugFlag > 1 ){
      DBG << "1 added Inflection: " << rule << endl;
    }
    rule->resolveBrackets();
    return rule;
  }
  else {
    if ( debugFlag > 1 ){
      DBG << "rejected rule: " << rule << endl;
    }
    delete rule;
    return 0;
  }
}

bool check_next( const UnicodeString& tag ){
  vector<UnicodeString> v = TiCC::split_at_first_of( tag, "()" );
  if ( v.size() != 2
       || v[0] != "VNW" ){
    return false;
  }
  else {
    bool result = v[1].indexOf( ",2," ) == -1;
    return result;
  }
}

vector<Rule*> Mbma::execute( const icu::UnicodeString& word,
			     const icu::UnicodeString& next_tag,
			     const vector<icu::UnicodeString>& classes ){
  /// attempt to find matching Rules
  /*!
    \param word a word to check
    \param next_tag The tag of the word following this word (when available)
    \param classes the Timbl classifications
    \return 0 or more matching Rules
  */
  vector<vector<UnicodeString> > allParts = generate_all_perms( classes );
  if ( debugFlag > 1 ){
    UnicodeString out = "alternatives: word="
      + word + ", classes=<" + TiCC::join( classes, "," ) + ">";
    DBG << out << endl;
    DBG << "allParts : " << allParts << endl;
  }
  bool both_V2_and_V2I = false;
  for ( const auto& cls : classes ){
    if ( cls.indexOf( "te2|" ) != -1
	 && cls.indexOf( "te2I" ) != -1 ){
      both_V2_and_V2I = true;
      break;
    }
  }
  if ( debugFlag > 1 ){
    if ( both_V2_and_V2I ){
      DBG << "found a special V2VI one: " << word << " in : "
	  << TiCC::join( classes, "," ) << endl;
    }
  }
  bool keep_V2I = false;
  if ( !both_V2_and_V2I ){
    keep_V2I = check_next( next_tag );
  }
  vector<Rule*> accepted;
  size_t id = 0;
  // now loop over all the analysis
  for ( auto const& ana : allParts ){
    Rule *rule = matchRule( ana, word, keep_V2I );
    if ( rule ){
      rule->ID = id++;
      accepted.push_back( rule );
    }
  }
  return accepted;
}

void Mbma::addBracketMorph( folia::Word *word,
			    const UnicodeString& orig_word,
			    const BaseBracket *brackets ) const {
  /// add a (deep) Morpheme layer to the FoLiA Word
  /*!
    \param word the FoLiA Word
    \param orig_word, a Unicode string with the oriiginal word
    \param brackets the nested structure describing the morphemes
   */
  if (debugFlag > 1){
    DBG << "addBracketMorph(" << word << "," << orig_word << ","
	<< brackets << ")" << endl;
  }
  folia::KWargs args;
  args["set"] = mbma_tagset;
  folia::MorphologyLayer *ml;
#pragma omp critical (foliaupdate)
  {
    try {
      ml = word->addMorphologyLayer( args );
    }
    catch( const exception& e ){
      LOG << e.what() << " addBracketMorph failed." << endl;
      throw;
    }
  }
  folia::Morpheme *m = 0;
  try {
    m = brackets->createMorpheme( word->doc(), textclass );
  }
  catch( const exception& e ){
    LOG << "createMorpheme failed: " << e.what() << endl;
    throw;
  }
#pragma omp critical (foliaupdate)
  {
    m->setutext( orig_word, textclass );
    ml->append( m );
  }
}

bool mbmacmp( const Rule *m1, const Rule *m2 ){
  /// sorting function for Rule's
  return m1->getKey().length() > m2->getKey().length();
}

struct id_cmp {
  bool operator()( const Rule *m1, const Rule *m2 ) const {
  /// sorting function for Rule's on ID
    return m1->ID < m2->ID;
  }
};

void Mbma::filterHeadTag( const icu::UnicodeString& head ){
  /// reduce the Mbms analysis by removing all solutions where the head is not
  /// matched
  /*!
    \param head the head-tag that is required
    matching does not mean equality. We are a forgivingful in the sense that
    \verbatim
    N matches PN
    A matches B and vv
    A matches V
    \endverbatim
  */
  // first we select only the matching heads
  if (debugFlag > 1){
    DBG << "filter with head: " << head << endl;
    DBG << "filter: analysis is:" << endl;
    int i=0;
    for ( const auto *it : analysis ){
      DBG << ++i << " - " << it << endl;
    }
  }
  auto const tagIt = TAGconv.find( head );
  if ( tagIt == TAGconv.end() ) {
    // this should never happen
    throw folia::ValueError( "1 unknown head feature '"
			     + TiCC::UnicodeToUTF8(head) + "'" );
  }
  UnicodeString celex_tag = tagIt->second;
  if (debugFlag > 1){
    DBG << "#matches: CGN:" << head << " CELEX " << celex_tag << endl;
  }
  auto ait = analysis.begin();
  while ( ait != analysis.end() ){
    UnicodeString mbma_tag = CLEX::toUnicodeString((*ait)->tag);
    if ( celex_tag == mbma_tag ){
      if (debugFlag > 1){
	DBG << "comparing " << celex_tag << " with "
	    << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 1.0;
      ++ait;
    }
    else if ( celex_tag == "N" && mbma_tag == "PN" ){
      if (debugFlag > 1){
	DBG << "comparing " << celex_tag << " with "
	    << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 1.0;
      ++ait;
    }
    else if ( ( celex_tag == "B" && mbma_tag == "A" )
	      || ( celex_tag == "A" && mbma_tag == "B" ) ){
      if (debugFlag > 1){
	DBG << "comparing " << celex_tag << " with "
	    << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 0.8;
      ++ait;
    }
    else if ( celex_tag == "A" && mbma_tag == "V" ){
      if (debugFlag > 1){
	DBG << "comparing " << celex_tag << " with "
	    << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 0.5;
      ++ait;
    }
    else {
      if (debugFlag > 1){
	DBG << "comparing " << celex_tag << " with "
	    << mbma_tag << " (rejected)" << endl;
      }
      delete *ait;
      ait = analysis.erase( ait );
    }
  }
  if (debugFlag > 1){
    DBG << "filter: analysis after head filter:" << endl;
    int i=0;
    for ( const auto *it : analysis ){
      DBG << ++i << " - " << it << endl;
    }
  }
}

void Mbma::filterSubTags( const vector<icu::UnicodeString>& feats ){
  /// reduce the analyses set based on sub-features
  /*!
    \param feats a list of subfeatures
    when a candidate Rule has inflexion it should match a feature.

    Other criteria: only take the highest confidence, and remove Rules that
    yield the same analysis.

    Example:
    \verbatim
    The word 'appel' is according to Mbma an N with inflection e ('enkelvoud')
    The tagger will assign an N too, with features [soort,ev,basis,zijd,stan]
    one of the features, 'ev' will match the 'e' inflection after translation
    so this is a good reading
    \endverbatim
  */
  if ( analysis.size() < 1 ){
    if (debugFlag > 1){
      DBG << "analysis is empty so skip next filter" << endl;
    }
    return;
  }
  // ok there are several analysis left.
  // try to select on the features:
  //
  // find best match
  // loop through all subfeatures of the tag
  // and match with inflections from each m
  set<Rule *, id_cmp> bestMatches; // store rules on ID, maybe overkill?
  int max_count = 0;
  for ( const auto& q : analysis ){
    int match_count = 0;
    UnicodeString inflection = q->inflection;
    if ( inflection.isEmpty() ){
      bestMatches.insert(q);
      continue;
    }
    if (debugFlag>1){
      DBG << "matching " << inflection << " with " << feats << endl;
    }
    for ( const auto& feat : feats ){
      auto const conv_tag_p = TAGconv.find( feat );
      if (conv_tag_p != TAGconv.end()) {
	UnicodeString c = conv_tag_p->second;
	if (debugFlag > 1){
	  DBG << "found " << feat << " ==> " << c << endl;
	}
	if ( inflection.indexOf( c ) != -1 ){
	  if (debugFlag >1){
	    DBG << "it is in the inflection " << endl;
	  }
	  match_count++;
	}
      }
    }
    if (debugFlag > 1 ){
      DBG << "score: " << match_count << " max was " << max_count << endl;
    }
    if (match_count >= max_count) {
      if (match_count > max_count) {
	max_count = match_count;
	bestMatches.clear();
      }
      bestMatches.insert(q);
    }
  }
  //
  // now filter on confidence:
  //
  if ( debugFlag > 1){
    DBG << "filter: best matches before sort on confidence:" << endl;
    int i=0;
    for ( const auto *it : bestMatches ){
      DBG << ++i << " - " << it << endl;
    }
    DBG << "" << endl;
  }
  double best_conf = -0.1;
  set<const Rule*, id_cmp> highConf; // store Rule's on ID to be reproducable
  for ( const auto *it : bestMatches ){
    if ( it->confidence >= best_conf ){
      if ( it->confidence > best_conf ){
	best_conf = it->confidence;
	highConf.clear();
      }
      highConf.insert( it );
    }
  }
  // now we can remove all analysis that aren't in the set.
  auto ana = analysis.begin();
  while ( ana != analysis.end() ){
    if ( highConf.find( *ana ) == highConf.end() ){
      delete *ana;
      ana = analysis.erase( ana );
    }
    else {
      ++ana;
    }
  }

  //
  // we still might have doubles. (different Rule's yielding the same result)
  // reduce these
  //
  if ( debugFlag > 1){
    DBG << "filter: analysis before sort key:" << endl;
    int i=0;
    for ( const auto *a_it : analysis ){
      DBG << ++i << " - " << a_it << " " << a_it->getKey()
	  << " (" << a_it->getKey().length() << ")" << endl;
    }
    DBG << endl;
  }
  map<icu::UnicodeString, const Rule*> unique;
  // create a map of "key's" to Rule. The key is the string representation
  // of the Rule's flattened result, like [ver][zeker][ing][s][ageer][ent]
  // (with inflection info for Deep Morph analysis)
  for ( const auto& ait : highConf ){
    icu::UnicodeString tmp = ait->getKey( doDeepMorph );
    unique.emplace(tmp,ait);
  }
  // so now we have map of 'equal' analysis.
  // create a set for reverse lookup, using the Rule.ID to distinguish Rule's
  set<const Rule*, id_cmp> uniqueAna;
  for ( auto const& uit : unique ){
    uniqueAna.insert( uit.second );
  }
  // now we can remove all analysis that aren't in that set.
  ana = analysis.begin();
  while ( ana != analysis.end() ){
    if ( uniqueAna.find( *ana ) == uniqueAna.end() ){
      delete *ana;
      ana = analysis.erase( ana );
    }
    else {
      ++ana;
    }
  }
  if ( debugFlag > 1){
    DBG << "filter: analysis before sort on length:" << endl;
    int i=0;
    for ( const auto *a_it : analysis ){
      DBG << ++i << " - " << a_it << " " << a_it->getKey()
	  << " (" << a_it->getKey().length() << ")" << endl;
    }
    DBG << "" << endl;
  }
  // Now we have a small list of unique and different analysis.
  // We assume the 'longest' analysis to be the best.
  // So we prefer '[ge][maak][t]' over '[gemaak][t]'
  // Therefor we sort on (unicode) string length
  sort( analysis.begin(), analysis.end(), mbmacmp );

  if ( debugFlag > 1){
    DBG << "filter: definitive analysis:" << endl;
    int i=0;
    for ( auto const *a_it : analysis ){
      DBG << ++i << " - " << a_it << endl;
    }
    DBG << "done filtering" << endl;
  }
  return;
}

void Mbma::assign_compounds(){
  /// add compound information to the result
  for ( auto const& sit : analysis ){
    sit->compound = sit->brackets->speculateCompoundType();
  }
}

void Mbma::add_provenance( folia::Document& doc,
			   folia::processor *main ) const {
  /// add provenance information to the FoLiA document
  /*!
    \param doc the foLiA document we are working on
    \param main the main processor (presumably Frog) we want to add a new one to
  */
  string _label = "mbma";
  if ( !main ){
    throw logic_error( "mbma::add_provenance() without arguments." );
  }
  folia::KWargs args;
  args["name"] = _label;
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  args.clear();
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::MORPHOLOGICAL, mbma_tagset, args );
  doc.declare( folia::AnnotationType::POS, clex_tagset, args );
}

void Mbma::store_morphemes( frog_record& fd,
			    const vector<UnicodeString>& morphemes ) const {
  /// store the calculated morphemes in the FrogData
  if ( fd.morph_string.isEmpty() ){
    // not yet filled
    UnicodeString out;
    for ( const auto& m : morphemes ){
      out += "[" + m + "]";
    }
#pragma omp critical (dataupdate)
    {
      fd.morph_string = out;
    }
  }
}

void Mbma::store_brackets( frog_record& fd,
			   const UnicodeString& wrd,
			   const UnicodeString& head,
			   bool unanalysed ) const {
  if (debugFlag > 1){
    DBG << "store_brackets(" << wrd << "," << head << ")" << endl;
  }
  if ( unanalysed  ) {
    // unanalysed, so trust the TAGGER
    if (debugFlag > 1){
      DBG << "head was X, tagger gives :" << head << endl;
    }
    const auto tagIt = TAGconv.find( head );
    if ( tagIt == TAGconv.end() ) {
      // this should never happen
      throw logic_error( "2 unknown head feature '"
			 + TiCC::UnicodeToUTF8( head ) + "'" );
    }
    CLEX::Type clex_tag = CLEX::toCLEX( tagIt->second );
    if (debugFlag > 1){
      DBG << "replaced X by: " << head << endl;
    }
    BaseBracket *leaf = new BracketLeaf( clex_tag,
					 wrd,
					 debugFlag,
					 *dbgLog );
    if ( fd.morph_string.isEmpty() ){
      fd.morph_string = "[" + wrd + "]";
      if ( doDeepMorph ){
	UnicodeString head_desc = CLEX::get_tag_descr(clex_tag);
	fd.morph_string += head_desc;
      }
    }
    fd.morph_structure.push_back( leaf );
  }
  else if ( head == "LET" || head == "SPEC" ){
    BaseBracket *leaf = new BracketLeaf( CLEX::toCLEX(head),
					 wrd,
					 debugFlag,
					 *dbgLog );
    leaf->set_status( STEM );
    if ( fd.morph_string.isEmpty() ){
      fd.morph_string = "[" + wrd + "]";
    }
    fd.morph_structure.push_back( leaf );
  }
  else {
    DBG << "case 3 " << endl;
    BaseBracket *leaf = new BracketLeaf( CLEX::toCLEX(head),
					 wrd,
					 debugFlag,
					 *dbgLog );
    leaf->set_status( STEM );
    if ( fd.morph_string.isEmpty() ){
      fd.morph_string = "[" + wrd + "]";
    }
    if ( doDeepMorph ){
      fd.morph_string += head;
    }
    fd.morph_structure.push_back( leaf );
  }
  return;
}

void Mbma::store_brackets( frog_record& fd,
			   const UnicodeString& orig_word,
			   const BracketNest *brackets ) const {
  if (debugFlag > 1){
    DBG << "store_brackets(" << fd.word << "," << orig_word
	<< "," << brackets << ")" << endl;
  }
#pragma omp critical (dataupdate)
  {
    fd.morph_structure.push_back( brackets );
  }
  return;
}

UnicodeString flatten( const UnicodeString& in ){
  /// helper function to 'flatten out' bracketed morpheme strings
  /*!
    \param in a bracketed string of morphemes
    \return a string with multiple '[' and ']' reduced to single occurrences
  */
  string s = TiCC::UnicodeToUTF8( in );
  string::size_type bpos = s.find_first_not_of( " [" );
  //  deb << "  FLATTEN: '" << s << "'" << endl;
  string result;
  if ( bpos != string::npos ){
    string::size_type epos = s.find_first_of( "]", bpos );
    result += "[" + s.substr( bpos, epos-bpos ) + "]";
    //    deb << "substring: '" <<  s.substr( bpos, epos-bpos ) << "'" << endl;
    bpos = s.find_first_of( "[", epos+1 );
    bpos = s.find_first_not_of( " [", bpos );
    while ( bpos != string::npos ){
      epos = s.find_first_of( "]", bpos );
      if ( epos == string::npos ){
	break;
      }
      result += "[" + s.substr( bpos, epos-bpos ) + "]";
      //      deb << "substring: '" <<  s.substr( bpos, epos-bpos ) << "'" << endl;
      bpos = s.find_first_of( "[", epos+1 );
      bpos = s.find_first_not_of( " [", bpos );
    }
  }
  else {
    result = s;
  }
  //  deb << "FLATTENED: '" << result << "'" << endl;
  return TiCC::UnicodeFromUTF8(result);
}

void Mbma::storeResult( frog_record& fd,
			const UnicodeString& uword,
			const UnicodeString& uhead ) const {
  if ( analysis.size() == 0 ){
    // fallback option: use the word and pretend it's a morpheme ;-)
    if ( debugFlag > 1){
      DBG << "no matches found, use the word instead: "
		    << uword << endl;
    }
    store_brackets( fd, uword, uhead, true );
    vector<UnicodeString> tmp;
    tmp.push_back( uword );
    store_morphemes( fd, tmp );
  }
  else {
    vector<pair<UnicodeString,string>> pv = getResults();
    if ( doDeepMorph ){
      fd.morph_string = pv[0].first;
    }
    else {
      fd.morph_string = flatten( pv[0].first );
    }
    if ( pv[0].second == "none" ){
      fd.compound_string = "0";
    }
    else {
      fd.compound_string = pv[0].second;
    }
    for ( auto& sit : analysis ){
      store_brackets( fd, uword, sit->brackets );
      store_morphemes( fd, sit->extract_morphemes() );
      sit->brackets = NULL;
    }
  }
}

void Mbma::Classify( frog_record& fd ){
  UnicodeString word = fd.word;
  UnicodeString tag = fd.tag;
  UnicodeString token_class = fd.token_class;
  vector<UnicodeString> v = TiCC::split_at_first_of( tag, "()" );
  UnicodeString head = v[0];
  if (debugFlag >1 ){
    DBG << "Classify " << word << "(" << head << ") ["
	<< token_class << "]" << endl;
  }
  // HACK! for now remove any whitespace!
  vector<UnicodeString> parts = TiCC::split( word );
  word = TiCC::join( parts, "" );
  if ( filter ){
    word = filter->filter( word );
  }
  if ( head == "LET"
       || head == "SPEC"
       || token_class == "ABBREVIATION" ){
    // take over the letter/word 'as-is'.
    //  also ABBREVIATION's aren't handled bij mbma-rules
    fd.clean_word = word;
    store_brackets( fd, word, head );
    vector<UnicodeString> tmp;
    tmp.push_back( word );
    store_morphemes( fd, tmp );
  }
  else {
    UnicodeString lWord = word;
    lWord.toLower();
    fd.clean_word = lWord;
    Classify( lWord, fd.next_tag );
    vector<UnicodeString> featVals;
    if ( v.size() > 1 ){
      featVals = TiCC::split_at( v[1], "," );
    }
    filterHeadTag( head );
    filterSubTags( featVals );
    assign_compounds();
    storeResult( fd, lWord, head );
  }
}

void Mbma::call_server( const vector<UnicodeString>& insts,
			vector<UnicodeString>& classes ){
  Sockets::ClientSocket client;
  if ( !client.connect( _host, _port ) ){
    LOG << "failed to open connection, " << _host
	<< ":" << _port << endl
	<< "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  if ( debugFlag > 1 ){
    DBG << "calling MBMA-server" << endl;
  }
  string in_line;
  client.read( in_line );
  json response;
  try {
    response = json::parse( in_line );
  }
  catch ( const exception& e ){
    string err_msg = "json parsing failed on '" + in_line + "':" + e.what();
    LOG << err_msg << endl;
    throw( runtime_error( err_msg ) );
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
      string err_msg = "json parsing failed on '" + in_line + "':" + e.what();
      LOG << err_msg << endl;
      throw( runtime_error( err_msg ) );
    }
    //    LOG << "received json data:" << response.dump(2) << endl;
  }
  // create json struct
  json query;
  query["command"] = "classify";
  json arr = json::array();
  for ( const auto& i : insts ){
    arr.push_back( TiCC::UnicodeToUTF8(i) );
  }
  query["params"] = arr;
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
    string err_msg = "json parsing failed on '" + in_line + "':" + e.what();
    LOG << err_msg << endl;
    throw( runtime_error( err_msg ) );
  }
  //  LOG << "received json data:" << response.dump(2) << endl;
  assert( response.size() == insts.size() );
  if ( response.size() == 1 ){
    classes.push_back( TiCC::UnicodeFromUTF8(response["category"]) );
  }
  else {
    for ( const auto& it : response.items() ){
      classes.push_back( TiCC::UnicodeFromUTF8(it.value()["category"]) );
    }
  }
}

void Mbma::Classify( const icu::UnicodeString& word,
		     const icu::UnicodeString& next_tag ){
  clearAnalysis();
  icu::UnicodeString uWord = word;
  if ( filter_diac ){
    uWord = TiCC::filter_diacritics( uWord );
  }
  vector<UnicodeString> insts = make_instances( uWord );
  vector<UnicodeString> classes;
  classes.reserve( insts.size() );
  //  LOG << "made instances: " << insts << endl;
  if ( !_host.empty() ){
    call_server( insts, classes );
  }
  else {
    int i = 0;
    for ( auto const& inst : insts ) {
      UnicodeString ans;
      MTree->Classify( inst, ans );
      if ( debugFlag > 1){
	DBG << "itt #" << i+1 << " " << insts[i] << " ==> " << ans
	    << ", depth=" << MTree->matchDepth() << endl;
	++i;
      }
      classes.push_back( ans );
    }
  }

  // fix for 1st char class ==0
  if ( classes[0] == "0" ){
    classes[0] = "X";
  }
  analysis = execute( uWord, next_tag, classes );
}

vector<pair<UnicodeString,string>> Mbma::getResults( bool shrt ) const {
  vector<pair<UnicodeString,string>> result;
  for ( const auto *it : analysis ){
    UnicodeString us = it->pretty_string( shrt );
    string cmp = toString( it->compound );
    result.push_back( make_pair(us, cmp) );
  }
  if ( debugFlag > 1 ){
    DBG << "result of morph analyses: ";
    for ( const auto& r : result ){
      DBG << " " << r << "," << endl;
    }
  }
  return result;
}

void Mbma::add_folia_morphemes( const vector<folia::Word*>& wv,
				const frog_data& fd ) const {
  for ( size_t i=0; i < wv.size(); ++i ){
    for ( const auto& mor : fd.units[i].morph_structure ) {
      addBracketMorph( wv[i], fd.units[i].clean_word, mor );
    }
  }
}
