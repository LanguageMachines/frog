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

RulePart::RulePart( const string& rs, const UChar kar, bool first ):
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
    if ( !first && tag == CLEX::C ){
      // special case: a C tag can only be at first postition
      // otherwise it is a C inflection!
      inflect = "C";
    }
    else {
      string::size_type pos = s.find("/");
      if ( pos != string::npos ){
	// some inflection
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
}

Rule::Rule( const vector<string>& parts,
	    const UnicodeString& s,
	    TiCC::LogStream* ls,
	    int flag ): debugFlag( flag ), tag(CLEX::UNASS), brackets(0), myLog(ls) {
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
  os << "MBMA rule:" << endl;
  for ( size_t k=0; k < r.rules.size(); ++k ) {
    os << "\t" << r.rules[k] << endl;
  }
  os << "tag: " << r.tag << " infl:" << r.inflection << " morhemes: "
     << r.extract_morphemes() << " description: " << r.description;
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
  for ( auto const& r : rules ){
    if ( r.ResultClass == CLEX::NEUTRAL
	 && r.morpheme.isEmpty()
	 && r.inflect.empty() ){
    }
    else {
      out.push_back(r);
    }
  }
  rules.swap( out );
}

vector<string> Rule::extract_morphemes() const {
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

bool Rule::performEdits(){
  if ( debugFlag ){
    *TiCC::Log(myLog) << "FOUND rule " << this << endl;
  }
  RulePart *last = 0;
  for ( size_t k=0; k < rules.size(); ++k ) {
    RulePart *cur = &rules[k];
    if ( last == 0 )
      last = cur;
    if ( debugFlag){
      *TiCC::Log(myLog) << "edit::act=" << cur << endl;
    }
    if ( !cur->del.isEmpty() ){
      // sanity check
      for ( int j=0; j < cur->del.length(); ++j ){
	if ( rules[k+j].uchar != cur->del[j] ){
	  UnicodeString tmp(cur->del[j]);
	  *TiCC::Log(myLog) << "Hmm: deleting " << cur->del << " is impossible. ("
			<< rules[k+j].uchar << " != " << tmp
			<< ")." << endl;
	  *TiCC::Log(myLog) << "Reject rule: " << this << endl;
	  return false;
	}
      }
    }
    if ( !cur->participle ){
      for ( int j=0; j < cur->del.length(); ++j ){
	rules[k+j].uchar = "";
	// so perform the deletion on this and subsequent nodes
      }
    }

    bool inserted = false;
    if ( cur->isBasic() ){
      // encountering real POS tag
      // start a new morpheme, BUT: inserts are appended to the previous one
      // except in case of Replace edits
      if ( debugFlag ){
	*TiCC::Log(myLog) << "FOUND a basic tag " << cur->ResultClass << endl;
      }
      if ( cur->del.isEmpty() ){ // So not a replace
	last->morpheme += cur->ins;
	inserted = true;
      }
      last = cur;
    }
    else if ( cur->ResultClass != CLEX::NEUTRAL ){
      // non 0 inflection starts a new morheme
      last = cur;
    }
    if ( !inserted ){
      // insert the deletestring :-)
      last->morpheme += cur->ins;
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
    if ( !inf.empty() && !rules[i].participle ){
      // it is an inflection tag
      if (debugFlag){
	*TiCC::Log(myLog) << " inflection: >" << inf << "<" << endl;
      }
      // given the specific selections of certain inflections,
      //    select a tag!
      CLEX::Type new_tag = CLEX::select_tag( inf[0] );

      // apply the change. Remember, the idea is that an inflection is
      // far more certain of the tag of its predecessing morpheme than
      // the morpheme itself.
      // This is not always the case, but it works
      if ( new_tag != CLEX::UNASS ) {
	if ( debugFlag  ){
	  *TiCC::Log(myLog) << inf[0] << " selects " << new_tag << endl;
	}
	// go back to the previous morpheme
	size_t k = i-1;
	//	  *TiCC::Log(myLog) << "een terug is " << rule.rules[k].ResultClass << endl;
	if ( rules[k].isBasic() ){
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
  }
}

bool Rule::check_inflections() const {
  if (debugFlag){
    *TiCC::Log(myLog) << " check all inflections: " << this << endl;
  }
  // check for every inflection if it matches the previous tag
  for ( size_t i = 1; i < rules.size(); ++i ){
    // skip inflections on first letter??
    string inf = rules[i].inflect;
    if ( !inf.empty() && !rules[i].participle ){
      // it is an inflection tag
      if (debugFlag){
	*TiCC::Log(myLog) << " inflection: >" << inf << "<" << endl;
      }
      // given the specific selections of certain inflections,
      //    select a tag!
      CLEX::Type match_tag = CLEX::select_tag( inf[0] );

      if ( match_tag != CLEX::UNASS ) {
	if ( debugFlag  ){
	  *TiCC::Log(myLog) << inf[0] << " selects " << match_tag << endl;
	}
	// go back to the previous morpheme
	size_t k = i-1;
	//	  *TiCC::Log(myLog) << "een terug is " << rule.rules[k].ResultClass << endl;
	if ( rules[k].isBasic() ){
	  // now see if we can replace this class for a better one
	  if ( rules[k].ResultClass == CLEX::PN &&
	       match_tag == CLEX::N ){
	    if ( debugFlag  ){
	      *TiCC::Log(myLog) << "match PN with N" << endl;
	    }
	  }
	  else if ( match_tag == rules[k].ResultClass ){
	    if ( debugFlag  ){
	      *TiCC::Log(myLog) << " matched: " << match_tag << endl;
	    }
	  }
	  else {
	    if ( debugFlag  ){
	      *TiCC::Log(myLog) << "didn't match: " << match_tag
				<< " with " << rules[k].ResultClass << endl;
	    }
	    return false;
	  }
	}
      }
    }
  }
  return true;
}

UnicodeString Rule::getKey( bool daring ){
  if ( sortkey.isEmpty() ){
    UnicodeString tmp;
    if ( daring ){
      stringstream ss;
      ss << brackets << endl;
      tmp = UTF8ToUnicode(ss.str());
    }
    else {
      vector<string> morphs = extract_morphemes();
      // create an unique string
      for ( auto const& mor : morphs ){
	tmp += UTF8ToUnicode(mor) + "++";
      }
    }
    sortkey = tmp;
  }
  return sortkey;
}

void Rule::getCleanInflect() {
  // get the FIRST inflection and clean it up by extracting only
  //  known inflection names
  inflection = "";
  for ( const auto& rule: rules ){
    if ( !rule.inflect.empty() ){
      //      *TiCC::Log(myLog) << "x inflect:'" << rule->inflect << "'" << endl;
      string inflect;
      for ( auto const& i : rule.inflect ){
	if ( i != '/' ){
	  // check if it is a known inflection
	  //	  *TiCC::Log(myLog) << "x bekijk [" << i << "]" << endl;
	  string inf = CLEX::get_iDescr(i);
	  if ( inf.empty() ){
	    //	    *TiCC::Log(myLog) << "added unknown inflection X" << endl;
	    inflect += "X";
	  }
	  else {
	    //	    *TiCC::Log(myLog) << "added known inflection " << i
	    //	     	 << " (" << inf << ")" << endl;
	    inflect += i;
	  }
	}
      }
      //      *TiCC::Log(myLog) << "cleaned inflection " << inflect << endl;
      inflection = inflect;
      return;
    }
  }
}

void Rule::resolveBrackets( bool daring ) {
  if ( debugFlag > 5 ){
    *TiCC::Log(myLog) << "check rule for bracketing: " << this << endl;
  }
  brackets = new BracketNest( CLEX::UNASS, CompoundType::NONE, debugFlag );
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
  if ( daring ){
    brackets->resolveNouns( );
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
  }
  brackets->setCompoundType();
  tag = brackets->getFinalTag();
  description = get_tDescr( tag );
  if ( debugFlag > 4 ){
    *TiCC::Log(myLog) << "Final Bracketing:" << brackets << " with tag=" << tag << endl;
  }
}
