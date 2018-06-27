/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2018
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
#include "frog/Frog-util.h"

using namespace std;
using TiCC::operator<<;

const long int LEFT =  6; // left context
const long int RIGHT = 6; // right context

#define LOG *TiCC::Log(mbmaLog)

Mbma::Mbma( TiCC::LogStream * logstream):
  MTreeFilename( "dm.igtree" ),
  MTree(0),
  filter(0),
  filter_diac(false),
  doDeepMorph(false)
{
  mbmaLog = new TiCC::LogStream( logstream, "mbma-" );
}

// define the statics
map<string,string> Mbma::TAGconv;
string Mbma::mbma_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbma-nl";
string Mbma::pos_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
string Mbma::clex_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-clex";

Mbma::~Mbma() {
  cleanUp();
  delete filter;
  delete mbmaLog;
}


void Mbma::init_cgn( const string& main, const string& sub ) {
  ifstream tc( main );
  if ( tc ){
    string line;
    while( getline( tc, line) ) {
      vector<string> tmp;
      size_t num = TiCC::split_at(line, tmp, " ");
      if ( num < 2 ){
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
    string line;
    while( getline(tc1, line) ) {
      vector<string> tmp;
      size_t num = TiCC::split_at(line, tmp, " ");
      if ( num == 2 ){
	TAGconv.insert( make_pair( tmp[0], tmp[1] ) );
      }
    }
  }
  else {
    throw ( runtime_error( "unable to open:" + sub ) );
  }
}

bool Mbma::init( const TiCC::Configuration& config ) {
  LOG << "Initiating morphological analyzer..." << endl;
  debugFlag = 0;
  string val = config.lookUp( "debug", "mbma" );
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
    version = "1.0";
  }
  else {
    version = val;
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
  string tfName = config.lookUp( "treeFile", "mbma" );
  if ( tfName.empty() ){
    tfName = "mbma.igtree";
  }
  MTreeFilename = prefix( config.configDir(), tfName );
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

  //Read in (igtree) data
  string opts = config.lookUp( "timblOpts", "mbma" );
  if ( opts.empty() ){
    opts = "-a1";
  }
  opts += " +vs -vf"; // make Timbl run quietly
  MTree = new Timbl::TimblAPI(opts);
  return MTree->GetInstanceBase(MTreeFilename);
}

void Mbma::cleanUp(){
  // LOG << "cleaning up MBMA stuff " << endl;
  delete MTree;
  MTree = 0;
  clearAnalysis();
}

vector<string> Mbma::make_instances( const icu::UnicodeString& word ){
  vector<string> insts;
  insts.reserve( word.length() );
  for ( long i=0; i < word.length(); ++i ) {
    if (debugFlag > 10){
      LOG << "itt #:" << i << endl;
    }
    icu::UnicodeString inst;
    for ( long j=i ; j <= i + RIGHT + LEFT; ++j ) {
      if (debugFlag > 10){
	LOG << " " << j-LEFT << ": ";
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
	LOG << " : " << inst << endl;
      }
    }
    inst += "?";
    insts.push_back( TiCC::UnicodeToUTF8(inst) );
  }
  return insts;
}

string find_class( unsigned int step,
		   const vector<string>& classes,
		   unsigned int nranal ){
  string result = classes[0];
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

vector<vector<string> > generate_all_perms( const vector<string>& classes ){
  // determine all alternative analyses, remember the largest
  // and store every part in a vector of string vectors
  size_t largest_anal=1;
  vector<vector<string> > classParts;
  classParts.reserve( classes.size() );
  for ( const auto& cl : classes ){
    vector<string> parts = TiCC::split_at( cl, "|" );
    size_t num = parts.size();
    if ( num > 1 ){
      classParts.push_back( parts );
      if ( num > largest_anal ){
	largest_anal = num;
      }
    }
    else {
      // only one, create a dummy
      vector<string> dummy;
      dummy.push_back( cl );
      classParts.push_back( dummy );
    }
  }
  //
  // now expand the result
  vector<vector<string> > result;
  result.reserve( largest_anal );
  for ( size_t step=0; step < largest_anal; ++step ){
    vector<string> item;
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

Rule* Mbma::matchRule( const std::vector<std::string>& ana,
		       const icu::UnicodeString& word ){
  Rule *rule = new Rule( ana, word, *mbmaLog, debugFlag );
  if ( rule->performEdits() ){
    rule->reduceZeroNodes();
    if ( debugFlag ){
      LOG << "after reduction: " << rule << endl;
    }
    rule->resolve_inflections();
    if ( debugFlag ){
      LOG << "after resolving: " << rule << endl;
    }
    rule->resolveBrackets( doDeepMorph );
    rule->getCleanInflect();
    if ( debugFlag ){
      LOG << "1 added Inflection: " << rule << endl;
    }
    return rule;
  }
  else {
    if ( debugFlag ){
      LOG << "rejected rule: " << rule << endl;
    }
    delete rule;
    return 0;
  }
}

vector<Rule*> Mbma::execute( const icu::UnicodeString& word,
			     const vector<string>& classes ){
  vector<vector<string> > allParts = generate_all_perms( classes );
  if ( debugFlag ){
    string out = "alternatives: word="
      + TiCC::UnicodeToUTF8(word) + ", classes=<";
    for ( const auto& cls : classes ){
      out += cls + ",";
    }
    out += ">";
    LOG << out << endl;
    LOG << "allParts : " << allParts << endl;
  }

  vector<Rule*> accepted;
  // now loop over all the analysis
  for ( auto const& ana : allParts ){
    Rule *rule = matchRule( ana, word );
    if ( rule ){
      accepted.push_back( rule );
    }
  }
  return accepted;
}

void Mbma::addMorph( folia::Word *word,
		     const vector<string>& morphs ) const {
  folia::KWargs args;
  args["set"] = mbma_tagset;
  folia::MorphologyLayer *ml;
#pragma omp critical (foliaupdate)
  {
    try {
      ml = word->addMorphologyLayer( args );
    }
    catch( const exception& e ){
      LOG << e.what() << " addMorph failed." << endl;
      throw;
    }
  }
  addMorph( ml, morphs );
}

void Mbma::addBracketMorph( folia::Word *word,
			    const string& wrd,
			    const string& tag ) const {
  if (debugFlag){
    LOG << "addBracketMorph(" << wrd << "," << tag << ")" << endl;
  }
  string celex_tag = tag;
  string head = tag;
  if ( head == "LET" || head == "SPEC" ){
    head.clear();
  }
  else if ( head == "X" ) {
    // unanalysed, so trust the TAGGER
#pragma omp critical (foliaupdate)
    {
      const auto pos = word->annotation<folia::PosAnnotation>( pos_tagset );
      head = pos->feat("head");
    }
    if (debugFlag){
      LOG << "head was X, tagger gives :" << head << endl;
    }
    const auto tagIt = TAGconv.find( head );
    if ( tagIt == TAGconv.end() ) {
      // this should never happen
      throw folia::ValueError( "unknown head feature '" + head + "'" );
    }
    celex_tag = tagIt->second;
    head = CLEX::get_tDescr(CLEX::toCLEX(tagIt->second));
    if (debugFlag){
      LOG << "replaced X by: " << head << endl;
    }
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
  args["class"] = "stem";
  folia::Morpheme *result = 0;
#pragma omp critical (foliaupdate)
  {
    result = new folia::Morpheme( args, word->doc() );
    result->settext( wrd, textclass );
  }
  args.clear();
  args["subset"] = "structure";
  args["class"]  = "[" + wrd + "]" + head;
#pragma omp critical (foliaupdate)
  {
    folia::Feature *feat = new folia::Feature( args );
    result->append( feat );
  }
  args.clear();
  args["set"] = clex_tagset;
  args["class"] = celex_tag;
#pragma omp critical (foliaupdate)
  {
    result->addPosAnnotation( args );
  }
#pragma omp critical (foliaupdate)
  {
    ml->append( result );
  }
}

void Mbma::addBracketMorph( folia::Word *word,
			    const string& orig_word,
			    const BracketNest *brackets ) const {
  if (debugFlag){
    LOG << "addBracketMorph(" << word << "," << orig_word << "," << brackets << ")" << endl;
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
  if ( m ){
#pragma omp critical (foliaupdate)
    {
      m->settext( orig_word, textclass );
      ml->append( m );
    }
  }
}

void Mbma::addMorph( folia::MorphologyLayer *ml,
		     const vector<string>& morphs ) const {
  for ( const auto& mor : morphs ){
    folia::KWargs args;
    args["set"] = mbma_tagset;
#pragma omp critical (foliaupdate)
    {
      folia::Morpheme *m = new folia::Morpheme( args, ml->doc() );
      m->settext( mor, textclass );
      ml->append( m );
    }
  }
}

bool mbmacmp( Rule *m1, Rule *m2 ){
  return m1->getKey(false).length() > m2->getKey(false).length();
}

void Mbma::filterHeadTag( const string& head ){
  // first we select only the matching heads
  if (debugFlag){
    LOG << "filter with head: " << head << endl;
    LOG << "filter: analysis is:" << endl;
    int i=0;
    for ( const auto& it : analysis ){
      LOG << ++i << " - " << it << endl;
    }
  }
  map<string,string>::const_iterator tagIt = TAGconv.find( head );
  if ( tagIt == TAGconv.end() ) {
    // this should never happen
    throw folia::ValueError( "unknown head feature '" + head + "'" );
  }
  string celex_tag = tagIt->second;
  if (debugFlag){
    LOG << "#matches: CGN:" << head << " CELEX " << celex_tag << endl;
  }
  auto ait = analysis.begin();
  while ( ait != analysis.end() ){
    string mbma_tag = CLEX::toString((*ait)->tag);
    if ( celex_tag == mbma_tag ){
      if (debugFlag){
	LOG << "comparing " << celex_tag << " with "
		      << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 1.0;
      ++ait;
    }
    else if ( celex_tag == "N" && mbma_tag == "PN" ){
      if (debugFlag){
	LOG << "comparing " << celex_tag << " with "
		      << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 1.0;
      ++ait;
    }
    else if ( ( celex_tag == "B" && mbma_tag == "A" )
	      || ( celex_tag == "A" && mbma_tag == "B" ) ){
      if (debugFlag){
	LOG << "comparing " << celex_tag << " with "
		      << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 0.8;
      ++ait;
    }
    else if ( celex_tag == "A" && mbma_tag == "V" ){
      if (debugFlag){
	LOG << "comparing " << celex_tag << " with "
		      << mbma_tag << " (OK)" << endl;
      }
      (*ait)->confidence = 0.5;
      ++ait;
    }
    else {
      if (debugFlag){
	LOG << "comparing " << celex_tag << " with "
		      << mbma_tag << " (rejected)" << endl;
      }
      delete *ait;
      ait = analysis.erase( ait );
    }
  }
  if (debugFlag){
    LOG << "filter: analysis after head filter:" << endl;
    int i=0;
    for ( const auto& it : analysis ){
      LOG << ++i << " - " << it << endl;
    }
  }
}

void Mbma::filterSubTags( const vector<string>& feats ){
  if ( analysis.size() < 1 ){
    if (debugFlag ){
      LOG << "analysis is empty so skip next filter" << endl;
    }
    return;
  }
  // ok there are several analysis left.
  // try to select on the features:
  //
  // find best match
  // loop through all subfeatures of the tag
  // and match with inflections from each m
  set<Rule *> bestMatches;
  int max_count = 0;
  for ( const auto& q : analysis ){
    int match_count = 0;
    string inflection = q->inflection;
    if ( inflection.empty() ){
      bestMatches.insert(q);
      continue;
    }
    if (debugFlag){
      LOG << "matching " << inflection << " with " << feats << endl;
    }
    for ( const auto& feat : feats ){
      map<string,string>::const_iterator conv_tag_p = TAGconv.find( feat );
      if (conv_tag_p != TAGconv.end()) {
	string c = conv_tag_p->second;
	if (debugFlag){
	  LOG << "found " << feat << " ==> " << c << endl;
	}
	if ( inflection.find( c ) != string::npos ){
	  if (debugFlag){
	    LOG << "it is in the inflection " << endl;
	  }
	  match_count++;
	}
      }
    }
    if (debugFlag){
      LOG << "score: " << match_count << " max was " << max_count << endl;
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
  if ( debugFlag ){
    LOG << "filter: best matches before sort on confidence:" << endl;
    int i=0;
    for ( const auto& it : bestMatches ){
      LOG << ++i << " - " << it << endl;
    }
    LOG << "" << endl;
  }
  double best_conf = -0.1;
  set<Rule*> highConf;
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
  // create a set for reverse lookup.
  set<Rule*> uniqueAna;
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
  if ( debugFlag ){
    LOG << "filter: analysis before sort on length:" << endl;
    int i=0;
    for ( const auto& it : analysis ){
      LOG << ++i << " - " << it << " " << it->getKey(false)
	  << " (" << it->getKey(false).length() << ")" << endl;
    }
    LOG << "" << endl;
  }
  // Now we have a small list of unique and differtent analysis.
  // We assume the 'longest' analysis to be the best.
  // So we prefer '[ge][maak][t]' over '[gemaak][t]'
  // Therefor we sort on (unicode) string length
  sort( analysis.begin(), analysis.end(), mbmacmp );

  if ( debugFlag){
    LOG << "filter: definitive analysis:" << endl;
    int i=0;
    for ( auto const& it : analysis ){
      LOG << ++i << " - " << it << endl;
    }
    LOG << "done filtering" << endl;
  }
  return;
}

void Mbma::assign_compounds(){
  for ( auto const& sit : analysis ){
    sit->compound = sit->brackets->getCompoundType();
  }
}

void Mbma::getFoLiAResult( folia::Word *fword,
			   const icu::UnicodeString& uword ) const {
  if ( analysis.size() == 0 ){
    // fallback option: use the word and pretend it's a morpheme ;-)
    if ( debugFlag ){
      LOG << "no matches found, use the word instead: "
		    << uword << endl;
    }
    if ( doDeepMorph ){
      addBracketMorph( fword, TiCC::UnicodeToUTF8(uword), "X" );
    }
    else {
      vector<string> tmp;
      tmp.push_back( TiCC::UnicodeToUTF8(uword) );
      addMorph( fword, tmp );
    }
  }
  else {
    for ( auto const& sit : analysis ){
      if ( doDeepMorph ){
	addBracketMorph( fword, TiCC::UnicodeToUTF8(uword), sit->brackets );
      }
      else {
	addMorph( fword, sit->extract_morphemes() );
      }
    }
  }
}

void Mbma::addDeclaration( folia::Document& doc ) const {
  doc.declare( folia::AnnotationType::MORPHOLOGICAL, mbma_tagset,
	       "annotator='frog-mbma-" +  version +
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
  if ( doDeepMorph ){
    doc.declare( folia::AnnotationType::POS, clex_tagset,
		 "annotator='frog-mbma-" +  version +
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void Mbma::Classify( folia::Word* sword ){
  if ( sword->isinstance(folia::PlaceHolder_t) ){
    return;
  }
  icu::UnicodeString uWord;
  folia::PosAnnotation *pos;
  string head;
  string token_class;
#pragma omp critical (foliaupdate)
  {
    uWord = sword->text( textclass );
    pos = sword->annotation<folia::PosAnnotation>( pos_tagset );
    head = pos->feat("head");
    string txtcls = sword->textclass();
    if ( txtcls == textclass ){
      // so only use the word class is the textclass of the word
      // matches the wanted text
      token_class = sword->cls();
    }
  }
  if (debugFlag ){
    LOG << "Classify " << uWord << "(" << pos << ") ["
		  << token_class << "]" << endl;
  }
  if ( filter ){
    uWord = filter->filter( uWord );
  }
  string word_s = TiCC::UnicodeToUTF8( uWord );
  vector<string> parts = TiCC::split( word_s );
  word_s.clear();
  for ( const auto& p : parts ){
    word_s += p;
  }
  uWord = TiCC::UnicodeFromUTF8( word_s );
  if ( head == "LET" || head == "SPEC" || token_class == "ABBREVIATION" ){
    // take over the letter/word 'as-is'.
    //  also ABBREVIATION's aren't handled bij mbma-rules
    string word = TiCC::UnicodeToUTF8( uWord );
    if ( doDeepMorph ){
      addBracketMorph( sword, word, head );
    }
    else {
      vector<string> tmp;
      tmp.push_back( word );
      addMorph( sword, tmp );
    }
  }
  else {
    icu::UnicodeString lWord = uWord;
    if ( head != "SPEC" ){
      lWord.toLower();
    }
    Classify( lWord );
    vector<string> featVals;
#pragma omp critical (foliaupdate)
    {
      vector<folia::Feature*> feats = pos->select<folia::Feature>();
      featVals.reserve( feats.size() );
      for ( const auto& feat : feats )
	featVals.push_back( feat->cls() );
    }
    filterHeadTag( head );
    filterSubTags( featVals );
    assign_compounds();
    getFoLiAResult( sword, lWord );
  }
}

void Mbma::Classify( const icu::UnicodeString& word ){
  clearAnalysis();
  icu::UnicodeString uWord = word;
  if ( filter_diac ){
    uWord = TiCC::filter_diacritics( uWord );
  }
  vector<string> insts = make_instances( uWord );
  vector<string> classes;
  classes.reserve( insts.size() );
  int i = 0;
  for ( auto const& inst : insts ) {
    string ans;
    MTree->Classify( inst, ans );
    if ( debugFlag ){
      LOG << "itt #" << i+1 << " " << insts[i] << " ==> " << ans
		    << ", depth=" << MTree->matchDepth() << endl;
      ++i;
    }
    classes.push_back( ans);
  }

  // fix for 1st char class ==0
  if ( classes[0] == "0" ){
    classes[0] = "X";
  }
  analysis = execute( uWord, classes );
}

vector<string> Mbma::getResult() const {
  vector<string> result;
  for ( const auto& it : analysis ){
    string tmp = it->morpheme_string( doDeepMorph );
    result.push_back( tmp );
  }
  if ( debugFlag ){
    LOG << "result of morph analyses: " << result << endl;
  }
  return result;
}

vector<pair<string,string>> Mbma::getResults( ) const {
  vector<pair<string,string>> result;
  for ( const auto& it : analysis ){
    string tmp = it->morpheme_string( true );
    string cmp = toString( it->compound );
    result.push_back( make_pair(tmp,cmp) );
  }
  if ( debugFlag ){
    LOG << "result of morph analyses: ";
    for ( const auto& r : result ){
      LOG << " " << r.first << "/" << r.second << "," << endl;
    }
  }
  return result;
}
