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

#include "frog/mbma_mod.h"

#include <cstdlib>
#include <string>
#include <set>
#include <iostream>
#include <algorithm>
#include <fstream>

#include "timbl/TimblAPI.h"

#include "ticcutils/Configuration.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
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
  filter_diac(false),
  doDeepMorph(false)
{
  /// create an Mbma classifier object
  /*!
    \param errlog the LogStream to use for Error messages
    \param dbglog the LogStream to use for Debug messages
  */
  errLog = new TiCC::LogStream( errlog, "mbma-" );
  if ( dbglog ){
    dbgLog = new TiCC::LogStream( dbglog, "mbma-" );
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

vector<string> Mbma::make_instances( const icu::UnicodeString& word ){
  /// convert a Unicode string into a range of UTF8 instances for Timbl
  /*!
    \param word the UnicodeString representing 1 word to analyze
    \return a vector of UTF8 stringa with instances for Timbl
  */
  vector<string> insts;
  insts.reserve( word.length() );
  for ( long i=0; i < word.length(); ++i ) {
    if (debugFlag > 10){
      DBG << "itt #:" << i << endl;
    }
    icu::UnicodeString inst;
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
    insts.push_back( TiCC::UnicodeToUTF8(inst) );
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

Rule* Mbma::matchRule( const vector<UnicodeString>& ana,
		       const UnicodeString& word ){
  /// attempt to match an Analysis on a word
  /*!
    \param ana one analysis result, expanded from the Timbl classifier
    \param word a Unicode Word to check
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
    rule->resolveBrackets( doDeepMorph );
    rule->getCleanInflect();
    if ( debugFlag > 1 ){
      DBG << "1 added Inflection: " << rule << endl;
    }
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

vector<Rule*> Mbma::execute( const icu::UnicodeString& word,
			     const vector<UnicodeString>& classes ){
  /// attempt to find matching Rules
  /*!
    \param word a word to check
    \param classes the Timbl classifications
    \return 0 or more matching Rules
  */
  vector<vector<UnicodeString> > allParts = generate_all_perms( classes );
  if ( debugFlag > 1 ){
    UnicodeString out = "alternatives: word="
      + word + ", classes=<";
    for ( const auto& cls : classes ){
      out += cls + ",";
    }
    out += ">";
    DBG << out << endl;
    DBG << "allParts : " << allParts << endl;
  }

  vector<Rule*> accepted;
  size_t id = 0;
  // now loop over all the analysis
  for ( auto const& ana : allParts ){
    Rule *rule = matchRule( ana, word );
    if ( rule ){
      rule->ID = id++;
      accepted.push_back( rule );
    }
  }
  return accepted;
}

void Mbma::addBracketMorph( folia::Word *word,
			    const string& orig_word,
			    const BaseBracket *brackets ) const {
  /// add a (deep) Morpheme layer to the FoLiA Word
  /*!
    \param word the FoLiA Word
    \param orig_word, an UTF8 string with the oriiginal word
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
    m = brackets->createMorpheme( word->doc() );
  }
  catch( const exception& e ){
    LOG << "createMorpheme failed: " << e.what() << endl;
    throw;
  }
#pragma omp critical (foliaupdate)
  {
    m->settext( orig_word, textclass );
    ml->append( m );
  }
}

bool mbmacmp( Rule *m1, Rule *m2 ){
  /// sorting function for Rule's
  return m1->getKey(false).length() > m2->getKey(false).length();
}

struct id_cmp {
  bool operator()( const Rule *m1, const Rule *m2 ){
  /// sorting function for Rule's on ID
    return m1->ID < m2->ID;
  }
};

void Mbma::filterHeadTag( const UnicodeString& head ){
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
    for ( const auto& it : analysis ){
      DBG << ++i << " - " << it << endl;
    }
  }
  auto const tagIt = TAGconv.find( head );
  if ( tagIt == TAGconv.end() ) {
    // this should never happen
    throw folia::ValueError( "1 unknown head feature '"
			     + TiCC::UnicodeToUTF8(head) + "'" );
  }
  string celex_tag = TiCC::UnicodeToUTF8(tagIt->second);
  if (debugFlag > 1){
    DBG << "#matches: CGN:" << head << " CELEX " << celex_tag << endl;
  }
  auto ait = analysis.begin();
  while ( ait != analysis.end() ){
    string mbma_tag = CLEX::toString((*ait)->tag);
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
    for ( const auto& it : analysis ){
      DBG << ++i << " - " << it << endl;
    }
  }
}

void Mbma::filterSubTags( const vector<UnicodeString>& feats ){
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
    for ( const auto& it : bestMatches ){
      DBG << ++i << " - " << it << endl;
    }
    DBG << "" << endl;
  }
  double best_conf = -0.1;
  set<Rule*, id_cmp> highConf; // store Rule's on ID to be reproducable
  for ( const auto& it : bestMatches ){
    if ( it->confidence >= best_conf ){
      if ( it->confidence > best_conf ){
	best_conf = it->confidence;
	highConf.clear();
      }
      highConf.insert( it );
    }
  }
  // now we can remove all analysis that aren't in the set.
  auto it = analysis.begin();
  while ( it != analysis.end() ){
    if ( highConf.find( *it ) == highConf.end() ){
      delete *it;
      it = analysis.erase( it );
    }
    else {
      ++it;
    }
  }

  //
  // we still might have doubles. (different Rule's yielding the same result)
  // reduce these
  //
  map<icu::UnicodeString, Rule*> unique;
  for ( const auto& ait : highConf ){
    icu::UnicodeString tmp = ait->getKey( doDeepMorph );
    unique[tmp] = ait;
  }
  // so now we have map of 'equal' analysis.
  // create a set for reverse lookup, using the Rule.ID to distinguish Rule's
  set<Rule*, id_cmp> uniqueAna;
  for ( auto const& uit : unique ){
    uniqueAna.insert( uit.second );
  }
  // now we can remove all analysis that aren't in that set.
  it = analysis.begin();
  while ( it != analysis.end() ){
    if ( uniqueAna.find( *it ) == uniqueAna.end() ){
      delete *it;
      it = analysis.erase( it );
    }
    else {
      ++it;
    }
  }
  if ( debugFlag > 1){
    DBG << "filter: analysis before sort on length:" << endl;
    int i=0;
    for ( const auto& a_it : analysis ){
      DBG << ++i << " - " << a_it << " " << a_it->getKey(false)
	  << " (" << a_it->getKey(false).length() << ")" << endl;
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
    for ( auto const& a_it : analysis ){
      DBG << ++i << " - " << a_it << endl;
    }
    DBG << "done filtering" << endl;
  }
  return;
}

void Mbma::assign_compounds(){
  /// add compound information to the result
  for ( auto const& sit : analysis ){
    sit->compound = sit->brackets->getCompoundType();
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
  if ( doDeepMorph ){
    doc.declare( folia::AnnotationType::POS, clex_tagset, args );
  }
}

void Mbma::store_morphemes( frog_record& fd,
			    const vector<UnicodeString>& morphemes ) const {
  /// store the calculated morphemes in the FrogData
  vector<string> adapted;
  for ( const auto& m : morphemes ){
    adapted.push_back( "[" + TiCC::UnicodeToUTF8(m) + "]" );
  }
#pragma omp critical (dataupdate)
  {
    if ( fd.morph_string.empty() ){
      for ( const auto& a : adapted ){
	fd.morph_string += a;
      }
    }
    fd.morphs.push_back( adapted );
  }
}

void Mbma::store_brackets( frog_record& fd,
			   const UnicodeString& wrd,
			   const UnicodeString& head,
			   bool unanalysed ) const {
  if (debugFlag > 1){
    DBG << "store_brackets(" << wrd << "," << head << ")" << endl;
  }
  string utf8_wrd = TiCC::UnicodeToUTF8( wrd );
  string utf8_head = TiCC::UnicodeToUTF8( head );
  if ( unanalysed  ) {
    // unanalysed, so trust the TAGGER
    if (debugFlag > 1){
      DBG << "head was X, tagger gives :" << head << endl;
    }
    const auto tagIt = TAGconv.find( head );
    if ( tagIt == TAGconv.end() ) {
      // this should never happen
      throw logic_error( "2 unknown head feature '" + utf8_head + "'" );
    }
    string clex_tag = TiCC::UnicodeToUTF8(tagIt->second);
    string head_tag = CLEX::get_tDescr(CLEX::toCLEX(clex_tag));
    if (debugFlag > 1){
      DBG << "replaced X by: " << head << endl;
    }
    BaseBracket *leaf = new BracketLeaf( CLEX::toCLEX(clex_tag),
					 wrd,
					 debugFlag,
					 *dbgLog );
    if ( fd.deep_morph_string.empty() ){
      fd.deep_morph_string = "[" + utf8_wrd + "]" + head_tag;
    }
    fd.deep_morphs.push_back( leaf );
  }
  else if ( head == "LET" || head == "SPEC" ){
    BaseBracket *leaf = new BracketLeaf( CLEX::toCLEX(utf8_head),
					 wrd,
					 debugFlag,
					 *dbgLog );
    leaf->set_status( STEM );
    if ( fd.deep_morph_string.empty() ){
      fd.deep_morph_string = "[" + utf8_wrd + "]";
    }
    fd.deep_morphs.push_back( leaf );
  }
  else {
    BaseBracket *leaf = new BracketLeaf( CLEX::toCLEX(utf8_head),
					 wrd,
					 debugFlag,
					 *dbgLog );
    leaf->set_status( STEM );
    if ( fd.deep_morph_string.empty() ){
      fd.deep_morph_string = "[" + utf8_wrd + "]" + utf8_head;
    }
    fd.deep_morphs.push_back( leaf );
  }
  return;
}

BracketNest *copy_nest( const BracketNest *brackets ){
  return brackets->clone();
}

void Mbma::store_brackets( frog_record& fd,
			   const UnicodeString& orig_word,
			   const BracketNest *brackets ) const {
  if (debugFlag > 1){
    DBG << "store_brackets(" << fd.word << "," << orig_word
	<< "," << brackets << ")" << endl;
  }
  BracketNest *copy = copy_nest( brackets );
#pragma omp critical (dataupdate)
  {
    fd.deep_morphs.push_back( copy );
  }
  return;
}

void Mbma::getResult( frog_record& fd,
		      const UnicodeString& uword,
		      const UnicodeString& uhead ) const {
  if ( analysis.size() == 0 ){
    // fallback option: use the word and pretend it's a morpheme ;-)
    if ( debugFlag > 1){
      DBG << "no matches found, use the word instead: "
		    << uword << endl;
    }
    if ( doDeepMorph ){
      store_brackets( fd, uword, uhead, true );
    }
    else {
      vector<UnicodeString> tmp;
      tmp.push_back( uword );
      store_morphemes( fd, tmp );
    }
  }
  else {
    if ( doDeepMorph ){
      vector<pair<string,string>> pv = getPrettyResults();
      fd.deep_morph_string = pv[0].first;
      if ( pv[0].second == "none" ){
	fd.compound_string = "0";
      }
      else {
	fd.compound_string = pv[0].second;
      }
    }
    else {
      vector<UnicodeString> v = getResult();
      fd.morph_string = TiCC::UnicodeToUTF8(v[0]);
      fd.compound_string = "0";
    }
    for ( auto const& sit : analysis ){
      if ( doDeepMorph ){
	store_brackets( fd, uword, sit->brackets );
      }
      else {
	store_morphemes( fd, sit->extract_morphemes() );
      }
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
  if ( head == "LET" || head == "SPEC" || token_class == "ABBREVIATION" ){
    // take over the letter/word 'as-is'.
    //  also ABBREVIATION's aren't handled bij mbma-rules
    fd.clean_word = word;
    if ( doDeepMorph ){
      store_brackets( fd, word, head );
    }
    else {
      vector<UnicodeString> tmp;
      tmp.push_back( word );
      store_morphemes( fd, tmp );
    }
  }
  else {
    UnicodeString lWord = word;
    if ( head != "SPEC" ){
      lWord.toLower();
    }
    fd.clean_word = lWord;
    Classify( lWord );
    vector<UnicodeString> featVals;
    if( v.size() > 1 ){
      featVals = TiCC::split_at( v[1], "," );
    }
    filterHeadTag( head );
    filterSubTags( featVals );
    assign_compounds();
    getResult( fd, lWord, head );
  }
}

void Mbma::call_server( const vector<string>& insts,
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
  // create json struct
  json query;
  query["command"] = "classify";
  json arr = json::array();
  for ( const auto& i : insts ){
    arr.push_back( i );
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
    LOG << "json parsing failed on '" << in_line << "':"
	<< e.what() << endl;
    abort();
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

void Mbma::Classify( const icu::UnicodeString& word ){
  static TiCC::UnicodeNormalizer my_norm;
  clearAnalysis();
  icu::UnicodeString uWord = word;
  if ( filter_diac ){
    uWord = TiCC::filter_diacritics( uWord );
  }
  else {
    uWord = my_norm.normalize( uWord );
  }
  vector<string> insts = make_instances( uWord );
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
  analysis = execute( uWord, classes );
}

vector<UnicodeString> Mbma::getResult() const {
  vector<UnicodeString> result;
  for ( const auto& it : analysis ){
    result.push_back( it->morpheme_string( doDeepMorph ) );
  }
  if ( debugFlag > 1 ){
    DBG << "result of morph analyses: " << result << endl;
  }
  return result;
}

vector<pair<UnicodeString,string>> Mbma::getResults( ) const {
  vector<pair<UnicodeString,string>> result;
  for ( const auto& it : analysis ){
    UnicodeString tmp = it->morpheme_string( true );
    string cmp = toString( it->compound );
    result.push_back( make_pair(tmp,cmp) );
  }
  if ( debugFlag > 1 ){
    DBG << "result of morph analyses: ";
    for ( const auto& r : result ){
      DBG << " " << r.first << "/" << r.second << "," << endl;
    }
  }
  return result;
}

vector<pair<string,string>> Mbma::getPrettyResults( ) const {
  vector<pair<string,string>> result;
  for ( const auto& it : analysis ){
    string tmp = it->pretty_string();
    string cmp = toString( it->compound );
    result.push_back( make_pair(tmp,cmp) );
  }
  if ( debugFlag > 1 ){
    DBG << "pretty result of morph analyses: ";
    for ( const auto& r : result ){
      DBG << " " << r.first << "/" << r.second << "," << endl;
    }
  }
  return result;
}

void Mbma::add_morphemes( const vector<folia::Word*>& wv,
			  const frog_data& fd ) const {
  for ( size_t i=0; i < wv.size(); ++i ){
    if ( !doDeepMorph ){
      for ( const auto& mor : fd.units[i].morphs ) {
	folia::KWargs args;
	args["set"] = mbma_tagset;
	folia::MorphologyLayer *ml = 0;
#pragma omp critical (foliaupdate)
	{
	  ml = wv[i]->addMorphologyLayer( args );
	}
	for ( const auto& mt : mor ) {
	  folia::Morpheme *m = new folia::Morpheme( args, wv[0]->doc() );
	  string stripped = mt.substr(1,mt.size()-2);
#pragma omp critical (foliaupdate)
	  {
	    m->settext( stripped, textclass );
	    ml->append( m );
	  }
	}
      }
    }
    else {
      for ( const auto& mor : fd.units[i].deep_morphs ) {
	addBracketMorph( wv[i],
			 TiCC::UnicodeToUTF8(fd.units[i].clean_word),
			 mor );
      }
    }
  }
}
