/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2016
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

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include "ticcutils/LogStream.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/folia.h"
#include "frog/clex.h"
#include "frog/mbma_brackets.h"
#include "frog/mbma_rule.h"

using namespace std;
using namespace folia;
using TiCC::operator<<;

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
    for ( const auto& rh : r.RightHand ){
      os << rh;
      if ( &rh < &r.RightHand.back() ){
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
  if ( r.is_affix ){
    os << " affix";
  }
  else if ( r.is_glue ){
    os << " glue ";
  }
  if ( !r.ins.isEmpty() ){
    os << " insert='" << r.ins << "'";
  }
  if ( !r.del.isEmpty() ){
    os << " delete='" << r.del << "'";
  }
  if ( !r.hide.isEmpty() ){
    os << " hidden='" << r.hide << "'";
  }
  if ( !r.morpheme.isEmpty() ){
    os << " morpheme ='" << r.morpheme << "'";
  }
  return os;
}

ostream& operator<<( ostream& os, const RulePart *r ){
  return os << *r;
}

void RulePart::get_edits( const string& edit ){
  if (edit[0]=='D') { // delete operation
    string s = edit.substr(1);
    ins = UTF8ToUnicode( s );
  }
  else if ( edit[0]=='I') {  // insert operation
    string s = edit.substr(1);
    del = UTF8ToUnicode( s );
  }
  else if ( edit[0]=='H') {  // hidden morpheme
    string s = edit.substr(1);
    hide = UTF8ToUnicode( s );
  }
  else if ( edit[0]=='R') { // replace operation
    string::size_type pos = edit.find( ">" );
    string s = edit.substr( 1, pos-1 );
    ins = UTF8ToUnicode( s );
    s = edit.substr( pos+1 );
    del = UTF8ToUnicode( s );
  }
}

RulePart::RulePart( const string& rs, const UChar kar, bool first ):
  ResultClass(CLEX::UNASS),
  uchar(kar),
  is_affix(false),
  is_glue(false),
  is_participle(false)
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
    get_edits( edit );
    s = rs.substr(0, ppos );
    is_participle = ( s.find( "pv" ) != string::npos ) &&
      ( del == "ge" );
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
	    is_affix = true;
	  }
	  else if ( tag == CLEX::XAFFIX ){
	    is_affix = true;
	  }
	  else if ( tag == CLEX::GLUE ){
	    if ( i != 0 ){
	      cerr << "glue symbol '^' may only occur at first position!"
		   << endl;
	      continue;
	    }
	    is_glue = true;
	  }
	}
      }
    }
  }
  else {
    //    cerr << "normal RulePart " << s << endl;
    CLEX::Type tag0 = CLEX::toCLEX( s[0] );
    if ( !first && tag0 == CLEX::C ){
      // special case: a C tag can only be at first postition
      // otherwise it is a C inflection!
      inflect = "C";
      //      cerr << "inflect =" << inflect << endl;
    }
    else {
      string::size_type pos = s.find("/");
      CLEX::Type tag = CLEX::toCLEX( s );
      if ( pos != string::npos ){
	// some inflection
	string ts = s.substr(0, pos );
	//	cerr << "ts=" << ts << endl;
	tag = CLEX::toCLEX( ts );
	if ( tag0 != CLEX::UNASS ){
	  // cases like 0/e 0/te2I
	  ResultClass = tag;
	  inflect = s.substr(pos+1);
	}
	else {
	  //  E/P (suffix=e/postive infection)
	  inflect = s;
	}
	//	cerr << "inflect =" << inflect << endl;
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
}

Rule::Rule( const vector<string>& parts,
	    const UnicodeString& s,
	    TiCC::LogStream* ls,
	    int flag ):
  debugFlag( flag ),
  tag(CLEX::UNASS),
  orig_word(s),
  compound( Compound::Type::NONE ),
  brackets(0),
  myLog(ls),
  confidence(0.0)
{
  for ( size_t k=0; k < parts.size(); ++k ) {
    string this_class = parts[k];
    RulePart cur( this_class, s[k], k==0 );
    rules.push_back( cur );
  }
}

Rule::~Rule(){
  delete brackets;
}

ostream& operator<<( ostream& os, const Rule& r ){
  os << "MBMA rule (" << r.orig_word << "):" << endl;
  for ( const auto& rule : r.rules ){
    os << "\t" << rule << endl;
  }
  os << "tag: " << r.tag << " infl:" << r.inflection << " morhemes: "
     << r.extract_morphemes() << " description: " << r.description
     << " confidence: " << r.confidence;
  if ( r.compound != Compound::Type::NONE ){
    os << " (" << r.compound << "-compound)"<< endl;
  }
  return os;
}

ostream& operator<<( ostream& os, const Rule *r ){
  if ( r ){
    os << *r << endl;
  }
  else {
    os << "Empty MBMA rule" << endl;
  }
  return os;
}

void Rule::reduceZeroNodes(){
  vector<RulePart> out;
  for ( auto const& r : rules ){
    if ( r.ResultClass == CLEX::NEUTRAL
	 && r.morpheme.isEmpty()
	 && r.inflect.empty() ){
      // skip
    }
    else {
      out.push_back(r);
    }
  }
  rules.swap( out );
}

vector<string> Rule::extract_morphemes( ) const {
  vector<string> morphemes;
  morphemes.reserve( rules.size() );
  for ( const auto& it : rules ){
    UnicodeString morpheme = it.morpheme;
    if ( !morpheme.isEmpty() ){
      morphemes.push_back( UnicodeToUTF8(morpheme) );
    }
  }
  return morphemes;
}

string Rule::morpheme_string( bool structured ) const {
  string result;
  if ( structured ){
    UnicodeString us = brackets->put(true);
    result = UnicodeToUTF8( us );
  }
  else {
    vector<string> vec = extract_morphemes();
    for ( const auto& m : vec ){
      result += "[" + m + "]";
    }
  }
  return result;
}

bool Rule::performEdits(){
  if ( debugFlag ){
    *TiCC::Log(myLog) << "FOUND rule " << this << endl;
  }
  RulePart *last = 0;
  for ( size_t k=0; k < rules.size(); ++k ) {
    RulePart *cur = &rules[k];
    if ( last == 0 ){
      last = cur;
    }
    if ( debugFlag){
      *TiCC::Log(myLog) << "edit::act=" << cur << endl;
    }
    bool is_replace = false;
    if ( !cur->del.isEmpty() ){
      // sanity check
      for ( int j=0; j < cur->del.length(); ++j ){
	if ( (k + j) < rules.size() ){
	  if ( rules[k+j].uchar != cur->del[j] ){
	    UnicodeString tmp(cur->del[j]);
	    *TiCC::Log(myLog) << "Hmm: deleting " << cur->del << " is impossible. ("
			      << rules[k+j].uchar << " != " << tmp
			      << ")." << endl;
	    *TiCC::Log(myLog) << "Reject rule: " << this << endl;
	    return false;
	  }
	}
	else {
	  UnicodeString tmp(cur->del[j]);
	  *TiCC::Log(myLog) << "Hmm: deleting " << cur->del
			    << " is impossible. (beyond end of the rule)"
			    << endl;
	  *TiCC::Log(myLog) << "Reject rule: " << this << endl;
	  return false;
	}
      }
      is_replace = !cur->ins.isEmpty();
    }
    if ( !cur->is_participle ){
      for ( int j=0; j < cur->del.length(); ++j ){
	rules[k+j].uchar = "";
	// so perform the deletion on this and subsequent nodes
      }
    }

    bool inserted = false;
    UnicodeString part; // store to-be-inserted particles here!
    if ( !cur->hide.isEmpty() ){
      last->morpheme += cur->uchar; // add to prevvoius morheme
      cur->uchar = "";
      last = cur;
    }
    else if ( cur->isBasic() ){
      // encountering real POS tag
      // start a new morpheme, BUT: inserts are appended to the previous one
      // except in case of Replace edits
      if ( debugFlag ){
	*TiCC::Log(myLog) << "FOUND a (basic) tag " << cur->ResultClass << endl;
      }
      if ( !is_replace ){
	if ( cur->ins == "ge" ){
	  // save particle, to add it to the NEXT node!
	  part = cur->ins;
	}
	else {
	  last->morpheme += cur->ins;
	}
	inserted = true;
      }
      last = cur;
    }
    else if ( cur->ResultClass != CLEX::NEUTRAL ){
      // this MUST be an inflection. like E, C S.. It starts a new morheme
      last = cur;
    }
    if ( !inserted || !cur->hide.isEmpty() ){
      // insert the deletestring :-)
      if ( debugFlag ){
	*TiCC::Log(myLog) << "add to morpheme: '" << cur->ins
			  << cur->hide<< "'" << endl;
      }
      last->morpheme += cur->ins + cur->hide;
    }
    else if ( !part.isEmpty() ){
      if ( debugFlag ){
	*TiCC::Log(myLog) << "a part to add: " << part << endl;
      }
      last->morpheme += part;
      part.remove();
    }
    last->morpheme += cur->uchar; // might be empty because of deletion
  }
  if ( debugFlag ){
    *TiCC::Log(myLog) << "edited rule " << this << endl;
  }
  return true;
}


void Rule::resolve_inflections(){
  // resolve all clearly resolvable implicit selections of inflectional tags
  // We take ONLY the first 'hint' of the inflection to find a new CLEX Type
  // When applicable, we replace the class from the rule
  for ( size_t i = 1; i < rules.size(); ++i ){
    string inf = rules[i].inflect;
    if ( !inf.empty() && !rules[i].is_participle ){
      // it is an inflection tag
      if (debugFlag){
	*TiCC::Log(myLog) << " inflection: >" << inf << "<" << endl;
      }
      // given the specific selections of certain inflections,
      //    select a tag!
      CLEX::Type new_tag = CLEX::UNASS;
      for ( size_t i=0; i < inf.size(); ++i ){
	new_tag = CLEX::select_tag( inf[i] );
	if ( new_tag != CLEX::UNASS ){
	  if ( debugFlag  ){
	    *TiCC::Log(myLog) << inf[i] << " selects " << new_tag << endl;
	  }
	  break;
	}
      }
      if ( new_tag != CLEX::UNASS ) {
	// apply the change. Remember, the idea is that an inflection is
	// far more certain of the tag of its predecessing morpheme than
	// the morpheme itself.
	// This is not always the case, but it works
	//
	// go back to the previous morpheme
	for( size_t k=i-1; k+1 > 0; --k ){
	  //	  *TiCC::Log(myLog) << "een terug is " << rules[k].ResultClass << endl;
	  if ( rules[k].isBasic() &&
	       rules[k].ResultClass != CLEX::P ){
	    // only nodes that can get inflected (and unanalysed too)
	    // now see if we can replace this class for a better one
	    if ( rules[k].ResultClass == CLEX::PN &&
		 new_tag == CLEX::N ){
	      if ( debugFlag  ){
		*TiCC::Log(myLog) << "Don't replace PN by N" << endl;
	      }
	    }
	    else {
	      if ( debugFlag  ){
		*TiCC::Log(myLog) << " replace " << rules[k].ResultClass
				  << " by " << new_tag << endl;
	      }
	      rules[k].ResultClass = new_tag;
	    }
	    return;
	  }
	}
      }
      else {
	// this realy shouldn't happen. probably an error in the data!?
	*TiCC::Log(myLog) << "inflection: " << inf
			  << " Problem: DOESN'T select a tag" << endl;
      }
    }
  }
}

UnicodeString Rule::getKey( bool deep ){
  if ( deep ){
    if ( sortkey.isEmpty() ){
      UnicodeString tmp;
      stringstream ss;
      ss << brackets << endl;
      tmp = UTF8ToUnicode(ss.str());
      sortkey = tmp;
    }
    return sortkey;
  }
  else {
    vector<string> morphs = extract_morphemes();
    UnicodeString tmp;
    // create an unique string
    for ( auto const& mor : morphs ){
      tmp += UTF8ToUnicode(mor) + "++";
    }
    return tmp;
  }
}

void Rule::getCleanInflect() {
  // get the last inflection and clean it up by extracting only
  //  known inflection names
  if ( debugFlag > 5 ){
    *TiCC::Log(myLog) << "getCleanInflect: " << this << endl;
  }
  inflection = "";
  auto it = rules.rbegin();
  while ( it != rules.rend() ){
    RulePart rule = *it;
    if ( debugFlag > 5 ){
      *TiCC::Log(myLog) << rule << endl;
    }
    if ( !rule.inflect.empty() ){
      if ( debugFlag > 5 ){
	*TiCC::Log(myLog) << "x inflect:'" << rule.inflect << "'" << endl;
      }
      string inflect;
      for ( auto const& i : rule.inflect ){
	if ( i != '/' ){
	  // check if it is a known inflection
	  if ( debugFlag > 5 ){
	    *TiCC::Log(myLog) << "x bekijk [" << i << "]" << endl;
	  }
	  string inf = CLEX::get_iDescr(i);
	  if ( inf.empty() ){
	    *TiCC::Log(myLog) << "added unknown inflection X" << endl;
	    inflect += "X";
	  }
	  else {
	    if ( debugFlag > 5 ){
	      *TiCC::Log(myLog) << "added known inflection " << i
				<< " (" << inf << ")" << endl;
	    }
	    inflect += i;
	  }
	}
      }
      if ( debugFlag > 5 ){
	*TiCC::Log(myLog) << "cleaned inflection " << inflect << endl;
      }
      inflection = inflect;
      return;
    }
    ++it;
  }
}

void Rule::resolveBrackets( bool deep ) {
  if ( debugFlag > 5 ){
    *TiCC::Log(myLog) << "check rule for bracketing: " << this << endl;
  }
  brackets = new BracketNest( CLEX::UNASS, Compound::Type::NONE, debugFlag );
  for ( auto const& rule : rules ){
    // fill a flat result;
    BracketLeaf *tmp = new BracketLeaf( rule, debugFlag );
    if ( tmp->status() == Status::STEM && tmp->morpheme().isEmpty() ){
      delete tmp;
    }
    else {
      brackets->append( tmp );
    }
  }
  if ( debugFlag > 5 ){
    *TiCC::Log(myLog) << "STEP 1:" << brackets << endl;
  }
  if ( deep ){
    brackets->resolveGlue( );
    if ( debugFlag > 5 ){
      *TiCC::Log(myLog) << "STEP 2:" << brackets << endl;
    }
    brackets->resolveLead( );
    if ( debugFlag > 5 ){
      *TiCC::Log(myLog) << "STEP 3:" << brackets << endl;
    }
    brackets->resolveTail( );
    if ( debugFlag > 5 ){
      *TiCC::Log(myLog) << "STEP 4:" << brackets << endl;
    }
    brackets->resolveMiddle();
    if ( debugFlag > 5 ){
      *TiCC::Log(myLog) << "STEP 5:" << brackets << endl;
    }
    brackets->resolveNouns( );
    brackets->clearEmptyNodes();
  }
  tag = brackets->getFinalTag();
  description = get_tDescr( tag );
  if ( debugFlag > 4 ){
    *TiCC::Log(myLog) << "Final Bracketing:" << brackets << " with tag=" << tag << endl;
  }
}
