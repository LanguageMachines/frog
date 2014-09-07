/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2014
  Tilburg University

  This file is part of frog.

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

#include <cstdlib>
#include <string>
#include <set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <timbl/TimblAPI.h>

#include "ucto/unicode.h"
#include "ticcutils/Configuration.h"
#include "frog/Frog.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace folia;
using namespace TiCC;

const long int LEFT =  6; // left context
const long int RIGHT = 6; // right context

Mbma::Mbma(LogStream * logstream): MTreeFilename( "dm.igtree" ), MTree(0),
	      transliterator(0), filter(0), doDaring(false) {
  mbmaLog = new LogStream( logstream, "mbma-" );
}

Mbma::~Mbma() {
  cleanUp();
  delete transliterator;
  delete filter;
  delete mbmaLog;
}

namespace CLEX {

  Type toCLEX( const string& s ){
    if ( s == "N" )
      return N;
    else if ( s == "A" )
      return A;
    else if ( s == "Q" )
      return Q;
    else if ( s == "V" )
      return V;
    else if ( s == "D" )
      return D;
    else if ( s == "O" )
      return O;
    else if ( s == "B" )
      return B;
    else if ( s == "P" )
      return P;
    else if ( s == "Y" )
      return Y;
    else if ( s == "I" )
      return I;
    else if ( s == "X" )
      return X;
    else if ( s == "Z" )
      return Z;
    else if ( s == "PN" )
      return PN;
    else if ( s == "*" )
      return AFFIX;
    else if ( s == "x" )
      return XAFFIX;
    else if ( s == "0" )
      return NEUTRAL;
    else
      return UNASS;
  }

  Type toCLEX( const char c ){
    string s;
    s += c;
    return toCLEX(s);
  }

  string toLongString( const Type& t ){
    switch ( t ){
    case N:
      return "noun";
    case A:
      return "adjective";
    case Q:
      return "quantifier/numeral";
    case V:
      return "verb";
    case D:
      return "article";
    case O:
      return "pronoun";
    case B:
      return "adverb";
    case P:
      return "preposition";
    case Y:
      return "conjunction";
    case I:
      return "interjection";
    case X:
      return "unanalysed";
    case Z:
      return "expression part";
    case PN:
      return "proper noun";
    case AFFIX:
      return "affix";
    case XAFFIX:
      return "x affix";
    case NEUTRAL:
      return "0";
    default:
      return "UNASS";
    }
  }

  string toString( const Type& t ){
    switch ( t ){
    case N:
      return "N";
    case A:
      return "A";
    case Q:
      return "Q";
    case V:
      return "V";
    case D:
      return "D";
    case O:
      return "O";
    case B:
      return "B";
    case P:
      return "P";
    case Y:
      return "Y";
    case I:
      return "I";
    case X:
      return "X";
    case Z:
      return "Z";
    case PN:
      return "PN";
    case AFFIX:
      return "*";
    case XAFFIX:
      return "x";
    case NEUTRAL:
      return "0";
    default:
      return "/";
    }
  }

  bool isBasicClass( const Type& t ){
    const string basictags = "NAQVDOBPYIXZ";
    switch ( t ){
    case N:
    case A:
    case Q:
    case V:
    case D:
    case O:
    case B:
    case P:
    case Y:
    case I:
    case X:
    case Z:
      return true;
    default:
      return false;
    }
  }

}

ostream& operator<<( ostream& os, const CLEX::Type t ){
  os << toString( t );
  return os;
}

void Mbma::fillMaps() {
  //
  // this could be done from a configfile
  //
  // first the CELEX POS tag names
  //
  tagNames[CLEX::N] = "noun";
  tagNames[CLEX::A] = "adjective";
  tagNames[CLEX::Q] = "quantifier/numeral";
  tagNames[CLEX::V] = "verb";
  tagNames[CLEX::D] = "article";
  tagNames[CLEX::O] = "pronoun";
  tagNames[CLEX::B] = "adverb";
  tagNames[CLEX::P] = "preposition";
  tagNames[CLEX::Y] = "conjunction";
  tagNames[CLEX::I] = "interjection";
  tagNames[CLEX::X] = "unanalysed";
  tagNames[CLEX::Z] = "expression part";
  tagNames[CLEX::PN] = "proper noun";
  //
  // now the inflections
  iNames['X'] = "";
  iNames['s'] = "separated";
  iNames['e'] = "singular";
  iNames['m'] = "plural";
  iNames['d'] = "diminutive";
  iNames['G'] = "genitive";
  iNames['D'] = "dative";
  iNames['P'] = "positive";
  iNames['C'] = "comparative";
  iNames['S'] = "superlative";
  iNames['E'] = "suffix-e";
  iNames['i'] = "infinitive";
  iNames['p'] = "participle";
  iNames['t'] = "present tense";
  iNames['v'] = "past tense";
  iNames['1'] = "1st person";
  iNames['2'] = "2nd person";
  iNames['3'] = "3rd person";
  iNames['I'] = "inversed";
  iNames['g'] = "imperative";
  iNames['a'] = "subjunctive";
}

// BJ: dirty hack with fixed everything to read in tag correspondences
void Mbma::init_cgn( const string& dir ) {
  string line;
  string fn = dir + "cgntags.main";
  ifstream tc( fn.c_str() );
  if ( tc ){
    while( getline( tc, line) ) {
      vector<string> tmp;
      size_t num = split_at(line, tmp, " ");
      if ( num < 2 ){
	*Log(mbmaLog) << "splitting '" << line << "' failed" << endl;
	throw ( runtime_error("panic") );
      }
      TAGconv.insert( make_pair( tmp[0], tmp[1] ) );
    }
  }
  else
    throw ( runtime_error( "unable to open:" + fn ) );
  fn = dir + "cgntags.sub";
  ifstream tc1( fn.c_str() );
  if ( tc1 ){
    while( getline(tc1, line) ) {
      vector<string> tmp;
      size_t num = split_at(line, tmp, " ");
      if ( num == 2 )
	TAGconv.insert( make_pair( tmp[0], tmp[1] ) );
    }
  }
  else
    throw ( runtime_error( "unable to open:" + fn ) );
}

Transliterator *Mbma::init_trans( ){
  UErrorCode stat = U_ZERO_ERROR;
  Transliterator *t = Transliterator::createInstance( "NFD; [:M:] Remove; NFC",
						      UTRANS_FORWARD,
						      stat );
  if ( U_FAILURE( stat ) ){
    throw runtime_error( "initFilter FAILED !" );
  }
  return t;
}


bool Mbma::init( const Configuration& config ) {
  *Log(mbmaLog) << "Initiating morphological analyzer..." << endl;
  debugFlag = 0;
  string val = config.lookUp( "debug", "mbma" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debugFlag = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "daring", "mbma" );
  if ( !val.empty() )
    doDaring = TiCC::stringTo<bool>( val );
  val = config.lookUp( "version", "mbma" );
  if ( val.empty() ){
    version = "1.0";
  }
  else
    version = val;
  val = config.lookUp( "set", "mbma" );
  if ( val.empty() ){
    tagset = "http://ilk.uvt.nl/folia/sets/frog-mbma-nl";
  }
  else
    tagset = val;

  val = config.lookUp( "set", "tagger" );
  if ( val.empty() ){
    cgn_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
  }
  else
    cgn_tagset = val;

  val = config.lookUp( "clex_set", "mbma" );
  if ( val.empty() ){
    clex_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-clex";
  }
  else
    clex_tagset = val;

  string charFile = config.lookUp( "char_filter_file", "mbma" );
  if ( charFile.empty() )
    charFile = config.lookUp( "char_filter_file" );
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new Tokenizer::UnicodeFilter();
    filter->fill( charFile );
  }
  string tfName = config.lookUp( "treeFile", "mbma" );
  if ( tfName.empty() )
    tfName = "mbma.igtree";
  MTreeFilename = prefix( config.configDir(), tfName );
  fillMaps();
  init_cgn( config.configDir() );
  string dof = config.lookUp( "filter_diacritics", "mbma" );
  if ( !dof.empty() ){
    bool b = stringTo<bool>( dof );
    if ( b ){
      transliterator = init_trans();
    }
  }
  //Read in (igtree) data
  string opts = config.lookUp( "timblOpts", "mbma" );
  if ( opts.empty() )
    opts = "-a1";
  opts += " +vs -vf"; // make Timbl run quietly
  MTree = new Timbl::TimblAPI(opts);
  return MTree->GetInstanceBase(MTreeFilename);
}

void Mbma::cleanUp(){
  // *Log(mbmaLog) << "cleaning up MBMA stuff " << endl;
  delete MTree;
  MTree = 0;
  clearAnalysis();
}

vector<string> Mbma::make_instances( const UnicodeString& word ){
  vector<string> insts;
  for( long i=0; i < word.length(); ++i ) {
    if (debugFlag > 10)
      *Log(mbmaLog) << "itt #:" << i << endl;
    UnicodeString inst;
    for ( long j=i ; j <= i + RIGHT + LEFT; ++j ) {
      if (debugFlag > 10)
	*Log(mbmaLog) << " " << j-LEFT << ": ";
      if ( j < LEFT || j >= word.length()+LEFT )
	inst += '_';
      else {
	if (word[j-LEFT] == ',' )
	  inst += 'C';
	else
	  inst += word[j-LEFT];
      }
      inst += ",";
      if (debugFlag > 10)
	*Log(mbmaLog) << " : " << inst << endl;
    }
    inst += "?";
    insts.push_back( UnicodeToUTF8(inst) );
    // store res
  }
  return insts;
}

bool RulePart::isBasic() const {
  return isBasicClass( ResultClass );
}

ostream& operator<<( ostream& os, const RulePart& r ){
  if ( r.ResultClass == CLEX::UNASS &&
       r.inflect.empty() ){
    os << "INVALID! No result node, AND no inflection" << endl;
  }
  else {
    os << r.uchar << " - ";
    for ( size_t i = 0; i < r.RightHand.size(); ++i ){
      os << r.RightHand[i];
      if ( i < r.RightHand.size()-1 ){
	os << "+";
      }
    }
    if ( !r.RightHand.empty() ){
      os << " ==> ";
    }
    os << r.ResultClass << " ";
    if ( !r.inflect.empty() ){
      os << " INFLECTION: " << r.inflect;
    }
  }
  if ( r.fixpos >= 0 )
    os << " affix at pos: " << r.fixpos;
  if ( r.xfixpos >= 0 )
    os << " x-affix at pos: " << r.xfixpos;
  if ( !r.ins.isEmpty() )
    os << " insert='" << r.ins << "'";
  if ( !r.del.isEmpty() )
    os << " delete='" << r.del << "'";
  if ( !r.morpheme.isEmpty() )
    os << " morpheme ='" << r.morpheme << "'";
  return os;
}

ostream& operator<<( ostream& os, const RulePart *r ){
  return os << *r;
}

void RulePart::get_ins_del( const string& edit ){
  if (edit[0]=='D') { // delete operation
    string s = edit.substr(1);
    ins = UTF8ToUnicode( s );
  }
  else if ( edit[0]=='I') {  //insert operation
    string s = edit.substr(1);
    del = UTF8ToUnicode( s );
  }
  else if ( edit[0]=='R') { // replace operation
    string::size_type pos = edit.find( ">" );
    string s = edit.substr( 1, pos-1 );
    ins = UTF8ToUnicode( s );
    s = edit.substr( pos+1 );
    del = UTF8ToUnicode( s );
  }
}

RulePart::RulePart( const string& rs, const UChar kar ):
  ResultClass(CLEX::UNASS),
  uchar(kar),
  fixpos(-1),
  xfixpos(-1),
  participle(false)
{
  //  cerr << "extract RulePart:" << rs << endl;
  string edit;
  string s = rs;
  string::size_type ppos = rs.find("+");
  if ( ppos != string::npos ){
    // some edit info is available
    string::size_type spos = rs.find("/");
    if ( spos != string::npos ){
      // inflection too
      inflect = rs.substr( spos+1 );
      //    cerr << "inflect = " << inflect << endl;
      edit = rs.substr( ppos+1, spos-ppos-1 );
    }
    else {
      edit = rs.substr( ppos+1 );
    }
    //    cerr << "EDIT = " << edit << endl;
    get_ins_del( edit );
    s = rs.substr(0, ppos );
    participle = ( s.find( 'p' ) != string::npos ) &&
      ( del == "ge" || del == "be" );
  }
  string::size_type pos = s.find("_");
  if ( pos != string::npos ){
    ResultClass = CLEX::toCLEX( s[0] );
    // a rewrite RulePart
    if ( pos != 1 ){
      cerr << "Surprise! _ on a strange position:" << pos << " in " << s << endl;
    }
    else {
      string rhs = s.substr( pos+1 );
      //      cerr << "RHS = " << rhs << endl;
      string::size_type spos = rhs.find("/");
      if ( spos != string::npos ){
	// inflection too
	inflect = rhs.substr( spos+1 );
	// cerr << "inflect = " << inflect << endl;
	rhs = rhs.substr( 0, spos );
      }
      //      cerr << "RHS = " << rhs << endl;
      RightHand.resize( rhs.size() );
      for ( size_t i = 0; i < rhs.size(); ++i ){
	CLEX::Type tag = CLEX::toCLEX( rhs[i] );
	if ( tag == CLEX::UNASS ){
	  cerr << "Unhandled class in rhs=" << rhs << endl;
	  continue;
	}
	else {
	  //	  cerr << "found tag '" << tag << "' in " << rhs << endl;
	  RightHand[i] = tag;
	  if ( tag == CLEX::AFFIX ){
	    fixpos = i;
	  }
	  if ( tag == CLEX::XAFFIX ){
	    xfixpos = i;
	  }
	}
      }
    }
  }
  else {
    //    cerr << "normal RulePart " << s << endl;
    CLEX::Type tag = CLEX::toCLEX( s[0] );
    string::size_type pos = s.find("/");
    if ( pos != string::npos ){
      // some inflextion
      if ( tag != CLEX::UNASS ){
	// cases like 0/e 0/te2I
	ResultClass = tag;
	inflect = s.substr(pos+1);
      }
      else {
	//  E/P
	inflect = s;
      }
      //      cerr << "inflect =" << inflect << endl;
    }
    else if ( tag != CLEX::UNASS ){
      // dull case
      ResultClass = tag;
    }
    else {
      // m
      inflect = s;
      //	cerr << "inflect =" << inflect << endl;
    }
  }
}

Rule::Rule( const vector<string>& parts,
	    const UnicodeString& s,
	    int flag ): debugFlag( flag ){
  for ( size_t k=0; k < parts.size(); ++k ) {
    string this_class = parts[k];
    RulePart cur( this_class, s[k] );
    rules.push_back( cur );
  }
}

ostream& operator<<( ostream& os, const Rule& r ){
  os << "MBMA rule:" << endl;
  for ( size_t k=0; k < r.rules.size(); ++k ) {
    os << "\t" << r.rules[k] << endl;
  }
  return os;
}

ostream& operator<<( ostream& os, const Rule *r ){
  if ( r )
    os << *r << endl;
  else
    os << "Empty MBMA rule" << endl;
  return os;
}

void Rule::reduceZeroNodes(){
  vector<RulePart> out;
  for ( size_t k=0; k < rules.size(); ++k ) {
    if ( rules[k].ResultClass == CLEX::NEUTRAL
	 && rules[k].morpheme.isEmpty()
	 && rules[k].inflect.empty() ){
    }
    else {
      out.push_back(rules[k]);
    }
  }
  rules.swap( out );
}

vector<string> Rule::extract_morphemes() const {
  vector<string> morphemes;
  vector<RulePart>::const_iterator it = rules.begin();
  while ( it != rules.end() ) {
    UnicodeString morpheme = it->morpheme;
    if ( !morpheme.isEmpty() ){
      morphemes.push_back( UnicodeToUTF8(morpheme) );
    }
    ++it;
  }
  return morphemes;
}

CLEX::Type select_tag( const char ch ){
  CLEX::Type result = CLEX::UNASS;
  switch( ch ){
  case 'm':
  case 'e':
  case 'd':
  case 'G':
  case 'D':
    result = CLEX::N;
  break;
  case 'P':
  case 'C':
  case 'S':
  case 'E':
    result = CLEX::A;
  break;
  case 'i':
  case 'p':
  case 't':
  case 'v':
  case 'g':
  case 'a':
    result = CLEX::V;
  break;
  default:
    break;
  }
  return result;
}


void Mbma::resolve_inflections( Rule& rule ){
  // resolve all clearly resolvable implicit selections of inflectional tags
  // We take ONLY the first 'hint' of the inflection to find a new CLEX Type
  // When applicable, we replace the class from the rule
  for ( size_t i = 1; i < rule.rules.size(); ++i ){
    string inf = rule.rules[i].inflect;
    if ( !inf.empty() && !rule.rules[i].participle ){
      // it is an inflection tag
      if (debugFlag){
	*Log(mbmaLog) << " inflection: >" << inf << "<" << endl;
      }
      // given the specific selections of certain inflections,
      //    select a tag!
      CLEX::Type new_tag = select_tag( inf[0] );

      // apply the change. Remember, the idea is that an inflection is
      // far more certain of the tag of its predecessing morpheme than
      // the morpheme itself.
      // This is not always the case, but it works
      if ( new_tag != CLEX::UNASS ) {
	if ( debugFlag  ){
	  *Log(mbmaLog) << inf[0] << " selects " << new_tag << endl;
	}
	// go back to the previous morpheme
	size_t k = i-1;
	//	  *Log(mbmaLog) << "een terug is " << rule.rules[k].ResultClass << endl;
	if ( rule.rules[k].isBasic() ){
	  // now see if we can replace this class for a better one
	  if ( rule.rules[k].ResultClass == CLEX::PN &&
	       new_tag == CLEX::N ){
	    if ( debugFlag  ){
	      *Log(mbmaLog) << "Don't replace PN by N" << endl;
	    }
	  }
	  else {
	    if ( debugFlag  ){
	      *Log(mbmaLog) << " replace " << rule.rules[k].ResultClass
			    << " by " << new_tag << endl;
	    }
	    rule.rules[k].ResultClass = new_tag;
	  }
	  return;
	}
      }
    }
  }
}

BracketLeaf::BracketLeaf( const RulePart& p, int flag ):
  BaseBracket(p.ResultClass, p.RightHand, flag),
  morph(p.morpheme )
{
  ifpos = -1;
  if ( !p.inflect.empty() ){
    inflect = p.inflect;
    if ( p.ResultClass == CLEX::UNASS ){
      status = INFLECTION;
    }
    else {
      status = INFO;
    }
  }
  else if ( RightHand.size() == 0 ){
    orig = toString( cls );
    status = STEM;
  }
  else {
    orig = toString( cls );
    orig += "_";
    for ( size_t i = 0; i < RightHand.size(); ++i ){
      orig += toString(RightHand[i]);
      if ( RightHand[i] == CLEX::AFFIX )
	ifpos = i;
    }
    status = DERIVATIONAL;
  }
}

BracketLeaf::BracketLeaf( CLEX::Type t, const UnicodeString& us, int flag ):
  BaseBracket( t, vector<CLEX::Type>(), flag ),
  morph( us )
{
  ifpos = -1;
  orig = toString( t );
  status = STEM;
}

BracketNest::~BracketNest(){
  for ( list<BaseBracket*>::const_iterator it = parts.begin();
	it != parts.end();
	++it ){
    delete *it;
  }
}

UnicodeString BaseBracket::put( bool noclass ) const {
  UnicodeString result = "[err?]";
  if ( !noclass ){
    UnicodeString s = UTF8ToUnicode(toString(cls));
    result += s;
  }
  return result;
}

UnicodeString BracketLeaf::put( bool noclass ) const {
  UnicodeString result = "[";
  result += morph;
  result += "]";
  if ( !noclass ){
    if ( orig.empty() )
      result += UTF8ToUnicode(inflect);
    else
      result += UTF8ToUnicode(orig);
  }
  return result;
}

UnicodeString BracketNest::put( bool noclass ) const {
  UnicodeString result = "[ ";
  for ( list<BaseBracket*>::const_iterator it = parts.begin();
	it != parts.end();
	++it ){
    result +=(*it)->put(noclass) + " ";
  }
  result += "]";
  if ( !noclass ){
    if ( cls != CLEX::UNASS )
      result += UTF8ToUnicode(toString(cls));
  }
  return result;
}

ostream& operator<< ( ostream& os, const BaseBracket& c ){
  os << c.put();
  return os;
}

ostream& operator<< ( ostream& os, const BaseBracket *c ){
  if ( c ){
    os << c->put();
  }
  else {
    os << "[EMPTY]";
  }
  return os;
}

UnicodeString BracketNest::deepmorphemes() const{
  UnicodeString res;
  for ( list<BaseBracket*>::const_iterator it = parts.begin();
	it != parts.end();
	++it ){
    res += (*it)->deepmorphemes();
  }
  return res;
}

void prettyP( ostream& os, const list<BaseBracket*>& v ){
  os << "[";
  for ( list<BaseBracket*>::const_iterator it = v.begin();
	it != v.end();
	++it ){
    os << *it << " ";
  }
  os << "]";
}

bool BracketNest::testMatch( list<BaseBracket*>& result,
			     const list<BaseBracket*>::iterator& rpos,
			     list<BaseBracket*>::iterator& bpos ){
  if ( debugFlag > 5 ){
    cerr << "test MATCH " << endl;
  }
  bpos = result.end();
  size_t len = (*rpos)->RightHand.size();
  if ( len == 0 || len > result.size() ){
    if ( debugFlag > 5 ){
      cerr << "test MATCH FAIL (no RHS or RHS > result)" << endl;
    }
    return false;
  }
  size_t fpos = (*rpos)->infixpos();
  if ( debugFlag > 5 ){
    cerr << "test MATCH, fpos=" << fpos << " en len=" << len << endl;
  }
  list<BaseBracket*>::iterator it = rpos;
  while ( fpos > 0 ){
    --fpos;
    --it;
  }
  size_t j = 0;
  bpos = it;
  for (; j < len && it != result.end(); ++j, ++it ){
    if ( debugFlag > 5 ){
      cerr << "test MATCH vergelijk " << *it << " met " << (*rpos)->RightHand[j] << endl;
    }
    if ( (*rpos)->RightHand[j] == CLEX::XAFFIX)
      continue;
    else if ( (*rpos)->RightHand[j] == CLEX::AFFIX)
      continue;
    else if ( (*rpos)->RightHand[j] != (*it)->tag() ){
      if ( debugFlag > 5 ){
	cerr << "test MATCH FAIL (" << (*rpos)->RightHand[j]
	     << " != " << (*it)->tag() << ")" << endl;
      }
      bpos = it;
      return false;
    }
  }
  if ( j < len ){
    if ( debugFlag > 5 ){
      cerr << "test MATCH FAIL (j < len)" << endl;
    }
    bpos = result.end();
    return false;
  }
  if ( debugFlag > 5 ){
    cerr << "test MATCH OK" << endl;
  }
  return true;
}


list<BaseBracket*>::iterator BracketNest::resolveAffix( list<BaseBracket*>& result,
							const list<BaseBracket*>::iterator& rpos ){
  if ( debugFlag > 5 ){
    cerr << "resolve affix" << endl;
  }
  list<BaseBracket*>::iterator bit;
  bool matched = testMatch( result, rpos, bit );
  if ( matched ){
    if ( debugFlag > 5 ){
      cerr << "OK een match" << endl;
    }
    size_t len = (*rpos)->RightHand.size();
    if ( len == result.size() ){
      // the rule matches exact what we have.
      // leave it
      list<BaseBracket*>::iterator it = rpos;
      return ++it;
    }
    else {
      list<BaseBracket*>::iterator it = bit--;
      BaseBracket *tmp = new BracketNest( (*rpos)->tag(), debugFlag );
      for ( size_t j = 0; j < len; ++j ){
	tmp->append( *it );
	if ( debugFlag > 5 ){
	  cerr << "erase " << *it << endl;
	}
	it = result.erase(it);
      }
      if ( debugFlag > 5 ){
	cerr << "new node:" << tmp << endl;
      }
      result.insert( ++bit, tmp );
      return bit;
    }
  }
  else {
    // the affix derivation failed.
    if ( bit == result.end() ){
      // no hacks yet
      bit = rpos;
      return ++bit;
    }
    list<BaseBracket*>::iterator it = rpos;
    if ( debugFlag > 5 ){
      cerr << "it = " << *it << endl;
      cerr << "bit = " << *bit << endl;
    }
    if ( (*bit)->RightHand.size() > 1 ){
      if ( debugFlag > 5 ){
	cerr << "undo splitup case 1" << endl;
      }
      // We 'undo' the splitup and construct a leaf with the combined morphemes
      UnicodeString mor;
      CLEX::Type tag = (*it)->tag();
      while ( it != result.end() ){
	if ( (*it)->inflection() != "" && tag != CLEX::UNASS ){
	  // so we DO continue when there is inflection and NO tag (like 'pt')
	  // in : N,0,0,0,pt,0,Q_Q*,0,0,0,0,0/m
	  break;
	}
	if ( debugFlag > 5 ){
	  cerr << "append:" << *it << endl;
	}
	mor += (*it)->morpheme();
	tag = (*it)->tag(); // remember the 'last' tag
	if ( debugFlag > 5 ){
	  cerr << "erase " << *it << endl;
	}
	it = result.erase(it);
      }
      BaseBracket *tmp = new BracketLeaf( tag, mor, debugFlag );
      if ( debugFlag > 5 ){
	cerr << "new node: " << tmp << endl;
      }
      result.insert( it, tmp );
      if ( debugFlag > 5 ){
	cerr << "result = " << result << endl;
      }
      return ++it;
    }
    else {
      if ( debugFlag > 5 ){
	cerr << "undo splitup case 2" << endl;
      }
      // We 'undo' the splitup and construct a leaf with the combined morphemes
      UnicodeString mor;
      CLEX::Type tag = (*bit)->tag();
      ++it;
      if (  bit == it ){
	if ( debugFlag > 5 ){
	  cerr << "escape with result = " << result << endl;
	}
	return ++bit;
      }
      while ( bit != it ){
	if ( debugFlag > 5 ){
	  cerr << "loop :" << *bit << endl;
	}
	if ( (*bit)->inflection() != "" && tag != CLEX::UNASS ){
	  // so we DO continue when there is inflection and NO tag (like 'pt')
	  // in : N,0,0,0,pt,0,Q_Q*,0,0,0,0,0/m
	  break;
	}
	if ( debugFlag > 5 ){
	  cerr << "append:" << *bit << " morpheme=" <<  (*bit)->deepmorphemes() << endl;
	}
	mor += (*bit)->deepmorphemes();
	tag = (*bit)->tag(); // remember the 'last' tag
	if ( debugFlag > 5 ){
	  cerr << "erase " << *bit << endl;
	}
	bit = result.erase(bit);
      }
      BaseBracket *tmp = new BracketLeaf( tag, mor, debugFlag );
      if ( debugFlag > 5 ){
	cerr << "new node: " << tmp << endl;
      }
      result.insert( it, tmp );
      if ( debugFlag > 5 ){
	cerr << "result = " << result << endl;
      }
      return ++bit;
    }
  }
}

void BracketNest::resolveNouns( ){
  if ( debugFlag > 5 ){
    cerr << "resolve NOUNS in:" << this << endl;
  }
  list<BaseBracket*>::iterator it = parts.begin();
  list<BaseBracket*>::iterator prev = it++;
  while ( it != parts.end() ){
    if ( (*prev)->tag() == CLEX::N && (*prev)->RightHand.size() == 0
	 && (*it)->tag() == CLEX::N && (*it)->RightHand.size() == 0 ){
      BaseBracket *tmp = new BracketNest( CLEX::N, debugFlag );
      tmp->append( *prev );
      tmp->append( *it );
      if ( debugFlag > 5 ){
	cerr << "current result:" << parts << endl;
	cerr << "new node:" << tmp << endl;
	cerr << "erase " << *prev << endl;
      }
      prev = parts.erase(prev);
      if ( debugFlag > 5 ){
	cerr << "erase " << *prev << endl;
      }
      prev = parts.erase(prev);
      prev = parts.insert( prev, tmp );
      if ( debugFlag > 5 ){
	cerr << "current result:" << parts << endl;
      }
      it = prev;
      ++it;
    }
    else {
      prev = it++;
    }
  }
  if ( debugFlag > 5 ){
    cerr << "resolve NOUNS result:" << this << endl;
  }
}

void BracketNest::resolveLead( ){
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // search for rules with a * at the begin
    if ( debugFlag > 5 ){
      cerr << "search leading *: bekijk: " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	cerr << "nested! " << endl;
      }
      (*it)->resolveLead();
      ++it;
    }
    else {
      if ( (*it)->infixpos() == 0 ){
	it = resolveAffix( parts, it );
      }
      else {
	++it;
      }
    }
  }
}

void BracketNest::resolveTail(){
  list<BaseBracket *>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // search for rules with a * at the end
    if ( debugFlag > 5 ){
      cerr << "search trailing *: bekijk: " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	cerr << "nested! " << endl;
      }
      (*it)->resolveTail();
      ++it;
    }
    else {
      size_t len = (*it)->RightHand.size();
      if ( (*it)->infixpos() > 0
	   && (*it)->infixpos() == signed(len)-1 ){
	if ( debugFlag > 5 ){
	  cerr << "found trailing * " << *it << endl;
	  cerr << "infixpos=" << (*it)->infixpos() << endl;
	  cerr << "len=" << len << endl;
	}
	it = resolveAffix( parts, it );
      }
      else {
	++it;
      }
    }
  }
}

void BracketNest::resolveMiddle(){
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // now search for other rules with a * in the middle
    if ( debugFlag > 5 ){
      cerr << "hoofd infix loop bekijk: " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	cerr << "nested! " << endl;
      }
      (*it)->resolveMiddle( );
      ++it;
    }
    else {
      size_t len = (*it)->RightHand.size();
      if ( (*it)->infixpos() > 0
	   && (*it)->infixpos() < signed(len)-1 ){
	it = resolveAffix( parts, it );
      }
      else {
	++it;
      }
    }
  }
}

CLEX::Type BracketNest::getFinalTag() {
  // cerr << "get Final Tag from: " << this << endl;
  cls = CLEX::X;
  list<BaseBracket*>::const_reverse_iterator it = parts.rbegin();
  while ( it != parts.rend() ){
    // cerr << "bekijk: " << *it << endl;
    if ( (*it)->isNested()
	 || ( (*it)->inflection().empty()
	      && !(*it)->morpheme().isEmpty() ) ){
      cls = (*it)->tag();
      // cerr << "final tag = " << cls << endl;
      break;
    }
    ++it;
  }
  //  cerr << "final tag = X " << endl;
  return cls;
}

BracketNest *Rule::resolveBrackets( bool daring, CLEX::Type& tag  ) {
  if ( debugFlag > 5 ){
    cerr << "check rule for bracketing: " << this << endl;
  }
  BracketNest *brackets = new BracketNest( CLEX::UNASS, debugFlag );
  for ( size_t k=0; k < rules.size(); ++k ) {
    // fill a flat result;
    BracketLeaf *tmp = new BracketLeaf( rules[k], debugFlag );
    if ( tmp->stat() == STEM && tmp->morpheme().isEmpty() ){
      delete tmp;
    }
    else {
      brackets->append( tmp );
    }
  }
  if ( debugFlag > 5 ){
    cerr << "STEP 1:" << brackets << endl;
  }
  if ( daring ){
    brackets->resolveNouns( );
    if ( debugFlag > 5 ){
      cerr << "STEP 2:" << brackets << endl;
    }
    brackets->resolveLead( );
    if ( debugFlag > 5 ){
      cerr << "STEP 3:" << brackets << endl;
    }
    brackets->resolveTail( );
    if ( debugFlag > 5 ){
      cerr << "STEP 4:" << brackets << endl;
    }
    brackets->resolveMiddle();
  }
  tag = brackets->getFinalTag();
  if ( debugFlag > 5 ){
    cerr << "Final Bracketing:" << brackets << endl;
  }
  return brackets;
}

bool Mbma::performEdits( Rule& rule ){
  if ( debugFlag){
    *Log(mbmaLog) << "FOUND rule " << rule << endl;
  }
  RulePart *last = 0;
  for ( size_t k=0; k < rule.rules.size(); ++k ) {
    RulePart *cur = &rule.rules[k];
    if ( last == 0 )
      last = cur;
    if ( debugFlag){
      *Log(mbmaLog) << "edit::act=" << cur << endl;
    }
    if ( !cur->del.isEmpty() && cur->ins != "eer" ){
      // sanity check
      for ( int j=0; j < cur->del.length(); ++j ){
	if ( rule.rules[k+j].uchar != cur->del[j] ){
	  UnicodeString tmp(cur->del[j]);
	  *Log(mbmaLog) << "Hmm: deleting " << cur->del << " is impossible. ("
			<< rule.rules[k+j].uchar << " != " << tmp
			<< ")." << endl;
	  *Log(mbmaLog) << "Reject rule: " << rule << endl;
	  return false;
	}
      }
    }
    if ( !cur->participle ){
      for ( int j=0; j < cur->del.length(); ++j ){
	rule.rules[k+j].uchar = "";
      }
    }

    bool inserted = false;
    bool e_except = false;
    if ( cur->isBasic() ){
      // encountering real POS tag
      // start a new morpheme, BUT: inserts are appended to the previous one
      // except for Replace edits, exception on that again: "eer" inserts
      if ( debugFlag ){
	*Log(mbmaLog) << "FOUND a basic tag " << cur->ResultClass << endl;
      }
      if ( cur->del.isEmpty() || cur->ins == "eer" ){
	if ( !cur->del.isEmpty() && cur->ins == "eer" ){
	  if ( debugFlag > 5 ){
	    *Log(mbmaLog) << "special 'eer' exception." << endl;
	    *Log(mbmaLog) << rule << endl;
	  }
	}
	if ( cur->ins == "ere" ){
	  if ( debugFlag > 5 ){
	    *Log(mbmaLog) << "special 'ere' exception." << endl;
	    *Log(mbmaLog) << rule << endl;
	  }
	  // strange exception
	  last->morpheme += "er";
	  e_except = true;
	}
	else {
	  last->morpheme += cur->ins;
	}
	inserted = true;
      }
      last = cur;
    }
    else if ( cur->ResultClass != CLEX::NEUTRAL && !cur->inflect.empty() ){
      // non 0 inflection starts a new morheme
      last = cur;
    }
    if ( !inserted ){
      // insert the deletestring :-)
      last->morpheme += cur->ins;
    }
    if ( e_except ) {
      // fix exception
      last->morpheme += "e";
    }
    last->morpheme += cur->uchar; // might be empty because of deletion
  }
  if ( debugFlag ){
    *Log(mbmaLog) << "edited rule " << rule << endl;
  }
  return true;
}

MBMAana::MBMAana( const Rule& r, bool daring ): rule(r) {
  CLEX::Type Tag;
  brackets = rule.resolveBrackets( daring, Tag );
  infl = rule.getCleanInflect();
  map<CLEX::Type,string>::const_iterator tit = tagNames.find( Tag );
  if ( tit == tagNames.end() ){
    // unknown tag
    tag = toString(CLEX::X);
    description = "unknown";
  }
  else {
    tag = toString(Tag);
    description = tit->second;
  }
}

string Rule::getCleanInflect() const {
  // get the FIRST inflection and clean it up by extracting only
  //  known inflection names
  vector<RulePart>::const_iterator it = rules.begin();
  while ( it != rules.end() ) {
    if ( !it->inflect.empty() ){
      //      cerr << "x inflect:'" << it->inflect << "'" << endl;
      string inflect;
      for ( size_t i=0; i< it->inflect.length(); ++i ) {
	if ( it->inflect[i] != '/' ){
	  // check if it is a known inflection
	  //	    cerr << "x bekijk [" << it->inflect[i] << "]" << endl;
	  map<char,string>::const_iterator csIt = iNames.find(it->inflect[i]);
	  if ( csIt == iNames.end() ){
	    // cerr << "added unknown inflection X" << endl;
	    inflect += "X";
	  }
	  else {
	    // cerr << "added known inflection " << csIt->first
	    // 	 << " (" << csIt->second << ")" << endl;
	    inflect += it->inflect[i];
	  }
	}
      }
      //      cerr << "cleaned inflection " << inflect << endl;
      return inflect;
    }
    ++it;
  }
  return "";
}

#define OLD_STEP

#ifdef OLD_STEP
string find_class( unsigned int step,
		   const vector<string>& classes,
		   unsigned int nranal ){
  string result = classes[0];
  if ( nranal > 1 ){
    if ( classes.size() > 1 ){
      if ( classes.size() > step )
	result = classes[step];
      else
	result = "0";
    }
  }
  return result;
}

vector<vector<string> > generate_all_perms( const vector<string>& classes ){
  // determine all alternative analyses, remember the largest
  // and store every part in a vector of string vectors
  int largest_anal=1;
  vector<vector<string> > classParts;
  classParts.resize( classes.size() );
  for ( unsigned int j=0; j< classes.size(); ++j ){
    vector<string> parts;
    int num = split_at( classes[j], parts, "|" );
    if ( num > 0 ){
      classParts[j] = parts;
      if ( num > largest_anal )
	largest_anal = num;
    }
    else {
      // only one, create a dummy
      vector<string> dummy;
      dummy.push_back( classes[j] );
      classParts[j] = dummy;
    }
  }
  //
  // now expand the result
  vector<vector<string> > result;
  for ( int step=0; step < largest_anal; ++step ){
    vector<string> item(classParts.size());
    for ( size_t k=0; k < classParts.size(); ++k ) {
      item[k] = find_class( step, classParts[k], largest_anal );
    }
    result.push_back( item );
  }
  return result;
}
#else

bool next_perm( vector< vector<string>::const_iterator >& its,
		const vector<vector<string> >& parts ){
  for ( size_t i=0; i < parts.size(); ++i ){
    ++its[i];
    if ( its[i] == parts[i].end() ){
      if ( i == parts.size() -1 )
	return false;
      its[i] = parts[i].begin();
    }
    else
      return true;
  }
  return false;
}

vector<vector<string> > generate_all_perms( const vector<string>& classes ){

  // determine all alternative analyses
  // store every part in a vector of string vectors
  vector<vector<string> > classParts;
  classParts.resize( classes.size() );
  for ( unsigned int j=0; j< classes.size(); ++j ){
    vector<string> parts;
    int num = split_at( classes[j], parts, "|" );
    if ( num > 0 ){
      classParts[j] = parts;
    }
    else {
      // only one, create a dummy
      vector<string> dummy;
      dummy.push_back( classes[j] );
      classParts[j] = dummy;
    }
  }
  //
  // now expand
  vector< vector<string>::const_iterator > its( classParts.size() );
  for ( size_t i=0; i<classParts.size(); ++i ){
    its[i] = classParts[i].begin();
  }
  vector<vector<string> > result;
  bool more = true;
  while ( more ){
    vector<string> item(classParts.size());
    for( size_t j=0; j< classParts.size(); ++j ){
      item[j] = *its[j];
    }
    result.push_back( item );
    more = next_perm( its, classParts );
  }
  return result;
}
#endif

void Mbma::clearAnalysis(){
  for ( size_t i=0; i < analysis.size(); ++i ){
    delete analysis[i];
  }
  analysis.clear();
}

void Mbma::execute( const UnicodeString& word,
		    const vector<string>& classes ){
  clearAnalysis();
  vector<vector<string> > allParts = generate_all_perms( classes );
  if ( debugFlag ){
    string out = "alternatives: word=" + UnicodeToUTF8(word) + ", classes=<";
    for ( size_t i=0; i < classes.size(); ++i )
      out += classes[i] + ",";
    out += ">";
    *Log(mbmaLog) << out << endl;
    *Log(mbmaLog) << "allParts : " << allParts << endl;
  }

  // now loop over all the analysis
  for ( unsigned int step=0; step < allParts.size(); ++step ) {
    Rule rule( allParts[step], word, debugFlag );
    if ( performEdits( rule ) ){
      rule.reduceZeroNodes();
      if ( debugFlag ){
	*Log(mbmaLog) << "after reduction: " << rule << endl;
      }
      resolve_inflections( rule );
      if ( debugFlag ){
	*Log(mbmaLog) << "after resolving: " << rule << endl;
      }
      MBMAana *tmp = new MBMAana( rule, doDaring );
      if ( debugFlag ){
	*Log(mbmaLog) << "1 added Inflection: " << tmp << endl;
      }
      analysis.push_back( tmp );
    }
    else if ( debugFlag ){
      *Log(mbmaLog) << "rejected rule: " << rule << endl;
    }
  }
}

void Mbma::addAltMorph( Word *word,
			const vector<string>& morphs ) const {
  Alternative *alt = new Alternative();
  MorphologyLayer *ml = new MorphologyLayer();
#pragma omp critical(foliaupdate)
  {
    alt->append( ml );
    word->append( alt );
  }
  addMorph( ml, morphs );
}

void Mbma::addMorph( Word *word,
		     const vector<string>& morphs ) const {
  MorphologyLayer *ml = new MorphologyLayer();
#pragma omp critical(foliaupdate)
  {
    word->append( ml );
  }
  addMorph( ml, morphs );
}

void Mbma::addBracketMorph( Word *word,
			    const string& wrd,
			    const string& tag ) const {
  //  *Log(mbmaLog) << "addBracketMorph(" << wrd << "," << tag << ")" << endl;
  MorphologyLayer *ml = new MorphologyLayer();
#pragma omp critical(foliaupdate)
  {
    word->append( ml );
  }
  KWargs args;
  args["class"] = "stem";
  Morpheme *result = new Morpheme( word->doc(), args );
  args.clear();
  args["value"] = wrd;
  args["offset"] = "0";
  TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
  {
    result->append( t );
  }
  args.clear();
  args["value"] =  "[" + wrd + "]";
  Description *d = new Description( args );
#pragma omp critical(foliaupdate)
  {
    result->append( d );
  }
  args.clear();
  args["set"] = clex_tagset;
  args["cls"] = tag;
#pragma omp critical(foliaupdate)
  {
    result->addPosAnnotation( args );
  }
#pragma omp critical(foliaupdate)
  {
    ml->append( result );
  }
}

void Mbma::addBracketMorph( Word *word,
			    const BracketNest *brackets ) const {
  MorphologyLayer *ml = new MorphologyLayer();
#pragma omp critical(foliaupdate)
  {
    word->append( ml );
  }
  Morpheme *m = brackets->createMorpheme( word->doc(), clex_tagset );
  if ( m ){
#pragma omp critical(foliaupdate)
    {
      ml->append( m );
    }
  }
}

void Mbma::addAltBracketMorph( Word *word,
			       const BracketNest *brackets ) const {
  Alternative *alt = new Alternative();
  MorphologyLayer *ml = new MorphologyLayer();
#pragma omp critical(foliaupdate)
  {
    alt->append( ml );
    word->append( alt );
  }
  Morpheme *m = brackets->createMorpheme( word->doc(), clex_tagset );
  if ( m ){
#pragma omp critical(foliaupdate)
    {
      ml->append( m );
    }
  }
}

Morpheme *BracketLeaf::createMorpheme( Document *doc,
				       const string& tagset ) const {
  string desc;
  int offset = 0;
  return createMorpheme( doc, tagset, offset, desc );
}

Morpheme *BracketLeaf::createMorpheme( Document *doc,
				       const string& tagset,
				       int& offset,
				       string& desc ) const {
  Morpheme *result = 0;
  desc.clear();
  if ( status == STEM ){
    KWargs args;
    args["class"] = "stem";
    result = new Morpheme( doc, args );
    args.clear();
    string out = UnicodeToUTF8(morph);
    args["value"] = out;
    args["offset"] = toString(offset);
    offset += out.length();
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    args.clear();
    desc = "[" + out + "]";
    args["value"] = desc;
    Description *d = new Description( args );
#pragma omp critical(foliaupdate)
    {
      result->append( d );
    }
    args.clear();
    args["set"]  = tagset;
    args["cls"] = toString( tag() );
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
  }
  else if ( status == INFLECTION ){
    KWargs args;
    args["class"] = "inflection";
    result = new Morpheme( doc, args );
    args.clear();
    string out = UnicodeToUTF8(morph);
    if ( out.empty() )
      out = inflect;
    else
      desc = "[" + out + "]";
    args["value"] = out;
    args["offset"] = toString(offset);
    TextContent *t = new TextContent( args );
    offset += out.length();
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    args.clear();
    string inf_desc;
    for ( size_t i=0; i < inflect.size(); ++i ){
      string d = iNames[inflect[i]];
      if ( i > 0 )
	inf_desc += ", ";
      inf_desc += d;
    }
    args.clear();
    args["value"] = inf_desc;
    Description *d = new Description( args );
#pragma omp critical(foliaupdate)
    {
      result->append( d );
    }
  }
  else if ( status == DERIVATIONAL ){
    KWargs args;
    args["class"] = "derivational";
    result = new Morpheme( doc, args );
    args.clear();
    string out = UnicodeToUTF8(morph);
    args["value"] = out;
    args["offset"] = toString(offset);
    TextContent *t = new TextContent( args );
    offset += out.length();
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    args.clear();
    desc = "[" + out + "]";
    args["value"] = desc;
    Description *d = new Description( args );
#pragma omp critical(foliaupdate)
    {
      result->append( d );
    }
    args.clear();
    args["set"]  = tagset;
    args["cls"]  = orig;
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
  }
  else {
    KWargs args;
    args["class"] = "inflection";
    result = new Morpheme( doc, args );
    string inf_desc;
    for ( size_t i=0; i < inflect.size(); ++i ){
      string d = iNames[inflect[i]];
      if ( i > 0 )
	inf_desc += ", ";
      inf_desc += d;
    }
    args.clear();
    args["value"] = inf_desc;
    Description *d = new Description( args );
#pragma omp critical(foliaupdate)
    {
      result->append( d );
    }
  }
  return result;
}

Morpheme *BracketNest::createMorpheme( Document *doc,
				       const string& tagset ) const {
  string desc;
  int offset = 0;
  return createMorpheme( doc, tagset, offset, desc );
}

Morpheme *BracketNest::createMorpheme( Document *doc,
				       const string& tagset,
				       int& of,
				       string& desc ) const {
  KWargs args;
  args["class"] = "complex";
  Morpheme *result = new Morpheme( doc, args );
  list<BaseBracket*>::const_iterator it = parts.begin();
  string mor;
  int cnt = 0;
  desc.clear();
  vector<Morpheme*> stack;
  int offset = 0;
  while ( it != parts.end() ){
    string deeper_desc;
    Morpheme *m = (*it)->createMorpheme( doc, tagset, offset, deeper_desc );
    if ( m ){
      string tmp;
      try {
	tmp = m->str();
      }
      catch (...){
      };
      if ( !tmp.empty() ){
	mor += tmp;
	desc += deeper_desc;
	++cnt;
      }
      stack.push_back( m );
    }
    ++it;
  }
  args.clear();
  args["value"] = mor;
  args["offset"] = toString(of);
  of += offset;
  TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
  {
    result->append( t );
  }
  args.clear();
  if ( cnt > 1 )
    desc = "[" + desc + "]";
  args["value"] = desc;
  Description *d = new Description( args );
#pragma omp critical(foliaupdate)
  {
    result->append( d );
  }
  args.clear();
  args["set"]  = tagset;
  args["cls"]  = toString( tag() );
#pragma omp critical(foliaupdate)
  {
    result->addPosAnnotation( args );
  }
#pragma omp critical(foliaupdate)
  for ( size_t i=0; i < stack.size(); ++i ){
    result->append( stack[i] );
  }
  return result;
}

void Mbma::addMorph( MorphologyLayer *ml,
		     const vector<string>& morphs ) const {
  int offset = 0;
  for ( size_t p=0; p < morphs.size(); ++p ){
    Morpheme *m = new Morpheme();
#pragma omp critical(foliaupdate)
    {
      ml->append( m );
    }
    KWargs args;
    args["value"] = morphs[p];
    args["offset"] = toString(offset);
    TextContent *t = new TextContent( args );
    offset += morphs[p].length();
#pragma omp critical(foliaupdate)
    {
      m->append( t );
    }
  }
}

void Mbma::filterTag( const string& head,  const vector<string>& feats ){
  // first we select only the matching heads
  if (debugFlag){
    *Log(mbmaLog) << "filter with tag, head: " << head
		  << " feats: " << feats << endl;
    *Log(mbmaLog) << "filter:start, analysis is:" << endl;
    int i=1;
    for(vector<MBMAana*>::const_iterator it=analysis.begin(); it != analysis.end(); it++)
      *Log(mbmaLog) << i++ << " - " << *it << endl;
  }
  map<string,string>::const_iterator tagIt = TAGconv.find( head );
  if ( tagIt == TAGconv.end() ) {
    // this should never happen
    throw ValueError( "unknown head feature '" + head + "'" );
  }
  if (debugFlag){
    *Log(mbmaLog) << "#matches: " << tagIt->first << " " << tagIt->second << endl;
  }
  vector<MBMAana*>::iterator ait = analysis.begin();
  while ( ait != analysis.end() ){
    string tagI = (*ait)->getTag();
    if ( tagIt->second == tagI || ( tagIt->second == "N" && tagI == "PN" ) ){
      if (debugFlag)
	*Log(mbmaLog) << "comparing " << tagIt->second << " with "
		      << tagI << " (OK)" << endl;
      ait++;
    }
    else {
      if (debugFlag)
	*Log(mbmaLog) << "comparing " << tagIt->second << " with "
		      << tagI << " (rejected)" << endl;
      delete *ait;
      ait = analysis.erase( ait );
    }
  }
  if (debugFlag){
    *Log(mbmaLog) << "filter: analysis after first step" << endl;
    int i=1;
    for(vector<MBMAana*>::const_iterator it=analysis.begin(); it != analysis.end(); it++)
      *Log(mbmaLog) << i++ << " - " << *it << endl;
  }

  if ( analysis.size() < 1 ){
    if (debugFlag ){
      *Log(mbmaLog) << "analysis has size: " << analysis.size()
		    << " so skip next filter" << endl;
    }
    return;
  }
  // ok there are several analysis left.
  // try to select on the features:
  //
  // find best match
  // loop through all subfeatures of the tag
  // and match with inflections from each m
  set<MBMAana *> bestMatches;
  int max_count = 0;
  for ( size_t q=0; q < analysis.size(); ++q ) {
    int match_count = 0;
    string inflection = analysis[q]->getInflection();
    if (debugFlag){
      *Log(mbmaLog) << "matching " << inflection << " with " << feats << endl;
    }
    for ( size_t u=0; u < feats.size(); ++u ) {
      map<string,string>::const_iterator conv_tag_p = TAGconv.find(feats[u]);
      if (conv_tag_p != TAGconv.end()) {
	string c = conv_tag_p->second;
	if (debugFlag){
	  *Log(mbmaLog) << "found " << feats[u] << " ==> " << c << endl;
	}
	if ( inflection.find( c ) != string::npos ){
	  if (debugFlag){
	    *Log(mbmaLog) << "it is in the inflection " << endl;
	  }
	  match_count++;
	}
      }
    }
    if (debugFlag)
      *Log(mbmaLog) << "score: " << match_count << " max was " << max_count << endl;
    if (match_count >= max_count) {
      if (match_count > max_count) {
	max_count = match_count;
	bestMatches.clear();
      }
      bestMatches.insert(analysis[q]);
    }
  }
  // so now we have "the" best matches.
  // Weed the rest
  vector<MBMAana*> res;
  set<MBMAana*>::const_iterator bit = bestMatches.begin();
  while ( bit != bestMatches.end() ){
    res.push_back( *bit );
    ++bit;
  }
  if (debugFlag){
    *Log(mbmaLog) << "filter: analysis after second step" << endl;
    int i=1;
    for(vector<MBMAana*>::const_iterator it=res.begin(); it != res.end(); it++)
      *Log(mbmaLog) << i++ << " - " <<  *it << endl;
    *Log(mbmaLog) << "start looking for doubles" << endl;
  }
  //
  // but now we still might have doubles
  //
  map<string, MBMAana*> unique;
  vector<MBMAana*>::iterator it=res.begin();
  while (  it != res.end() ){
    string tmp;
    if ( doDaring ){
      stringstream ss;
      ss << (*it)->getBrackets() << endl;
      tmp = ss.str();
    }
    else {
      vector<string> v = (*it)->getMorph();
      // create an unique key
      for ( size_t p=0; p < v.size(); ++p ) {
	tmp += v[p] + "+";
      }
    }
    unique[tmp] = *it;
    ++it;
  }
  vector<MBMAana*> result;
  map<string, MBMAana*>::const_iterator uit=unique.begin();
  while ( uit != unique.end() ){
    result.push_back( uit->second );
    if (debugFlag){
      *Log(mbmaLog) << "Final Bracketing: " << uit->second->getBrackets() << endl;
    }
    ++uit;
  }
  analysis = result;
  if (debugFlag){
    *Log(mbmaLog) << "filter: definitive analysis:" << endl;
    int i=1;
    for(vector<MBMAana*>::const_iterator it=analysis.begin(); it != analysis.end(); it++)
      *Log(mbmaLog) << i++ << " - " << *it << endl;
    *Log(mbmaLog) << "done filtering" << endl;
  }
  return;
}

void Mbma::getFoLiAResult( Word *fword, const UnicodeString& uword ) const {
  if ( analysis.size() == 0 ){
    // fallback option: use the word and pretend it's a morpheme ;-)
    if ( debugFlag ){
      *Log(mbmaLog) << "no matches found, use the word instead: "
		    << uword << endl;
    }
    if ( doDaring ){
      addBracketMorph( fword, UnicodeToUTF8(uword), "UNK" );
    }
    else {
      vector<string> tmp;
      tmp.push_back( UnicodeToUTF8(uword) );
      addMorph( fword, tmp );
    }
  }
  else {
    vector<MBMAana*>::const_iterator sit = analysis.begin();
    while( sit != analysis.end() ){
      if ( sit == analysis.begin() ){
	if ( doDaring )
	  addBracketMorph( fword, (*sit)->getBrackets() );
	else
	  addMorph( fword, (*sit)->getMorph() );
      }
      else {
	if ( doDaring )
	  addAltBracketMorph( fword, (*sit)->getBrackets() );
	else
	  addAltMorph( fword, (*sit)->getMorph() );
      }
      ++sit;
    }
  }
}

void Mbma::addDeclaration( Document& doc ) const {
  doc.declare( AnnotationType::MORPHOLOGICAL, tagset,
	       "annotator='frog-mbma-" +  version +
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
  if ( doDaring ){
    doc.declare( AnnotationType::POS, clex_tagset,
		 "annotator='frog-mbma-" +  version +
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

UnicodeString Mbma::filterDiacritics( const UnicodeString& in ) const {
  if ( transliterator ){
    UnicodeString result = in;
    transliterator->transliterate( result );
    return result;
  }
  else
    return in;
}

void Mbma::Classify( Word* sword ){
  if ( sword->isinstance(PlaceHolder_t) )
    return;
  UnicodeString uWord;
  PosAnnotation *pos;
  string head;
  string token_class;
#pragma omp critical(foliaupdate)
  {
    uWord = sword->text();
    pos = sword->annotation<PosAnnotation>( cgn_tagset );
    head = pos->feat("head");
    token_class = sword->cls();
  }
  if (debugFlag)
    *Log(mbmaLog) << "Classify " << uWord << "(" << pos << ") ["
		  << token_class << "]" << endl;
  if ( filter )
    uWord = filter->filter( uWord );
  if ( head == "LET" || head == "SPEC" ){
    // take over the letter/word 'as-is'.
    string word = UnicodeToUTF8( uWord );
    if ( doDaring ){
      addBracketMorph( sword, word, head );
    }
    else {
      vector<string> tmp;
      tmp.push_back( word );
      addMorph( sword, tmp );
    }
  }
  else {
    UnicodeString lWord = uWord;
    if ( head != "SPEC" )
      lWord.toLower();
    Classify( lWord );
    vector<string> featVals;
#pragma omp critical(foliaupdate)
    {
      vector<folia::Feature*> feats = pos->select<folia::Feature>();
      for ( size_t i = 0; i < feats.size(); ++i )
	featVals.push_back( feats[i]->cls() );
    }
    filterTag( head, featVals );
    getFoLiAResult( sword, lWord );
  }
}

void Mbma::Classify( const UnicodeString& word ){
  UnicodeString uWord = filterDiacritics( word );
  vector<string> insts = make_instances( uWord );
  vector<string> classes;
  for( size_t i=0; i < insts.size(); ++i ) {
    string ans;
    MTree->Classify( insts[i], ans );
    if ( debugFlag ){
      *Log(mbmaLog) << "itt #" << i << " " << insts[i] << " ==> " << ans
		    << ", depth=" << MTree->matchDepth() << endl;
    }
    classes.push_back( ans);
  }

  // fix for 1st char class ==0
  if ( classes[0] == "0" )
    classes[0] = "X";
  execute( uWord, classes );
}

vector<vector<string> > Mbma::getResult() const {
  vector<vector<string> > result;
  for (vector<MBMAana*>::const_iterator it=analysis.begin();
       it != analysis.end();
       it++ ){
    if ( doDaring ){
      stringstream ss;
      ss << (*it)->getBrackets()->put( true ) << endl;
      vector<string> mors;
      mors.push_back( ss.str() );
      result.push_back( mors );
    }
    else {
      vector<string> mors = (*it)->getMorph();
      if ( debugFlag ){
	*Log(mbmaLog) << "Morphs " << mors << endl;
      }
      result.push_back( mors );
    }
  }
  if ( debugFlag ){
    *Log(mbmaLog) << "result of morph analyses: " << result << endl;
  }
  return result;
}

ostream& operator<< ( ostream& os, const MBMAana& a ){
  os << "tag: " << a.tag << " infl:" << a.infl << " morhemes: "
     << a.rule.extract_morphemes() << " description: " << a.description;
  return os;
}

ostream& operator<< ( ostream& os, const MBMAana *a ){
  if ( a )
    os << *a;
  else
    os << "no-mbma";
  return os;
}
