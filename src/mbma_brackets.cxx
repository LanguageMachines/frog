/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
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

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include "ucto/unicode.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/LogStream.h"
#include "libfolia/document.h"
#include "frog/clex.h"
#include "frog/mbma_rule.h"
#include "frog/mbma_brackets.h"

using namespace std;
using namespace TiCC;
using namespace folia;

ostream& operator<<( ostream& os, const CompoundType ct ){
  if ( ct == CompoundType::NN )
    os << "NN";
  else if ( ct == CompoundType::PN )
    os << "PN";
  else if ( ct == CompoundType::PV )
    os << "PV";
  else
    os << "none";
  return os;
}

BracketLeaf::BracketLeaf( const RulePart& p, int flag ):
  BaseBracket(p.ResultClass, p.RightHand, flag),
  morph(p.morpheme )
{
  ifpos = -1;
  if ( !p.inflect.empty() ){
    inflect = p.inflect;
    if ( p.ResultClass == CLEX::UNASS ){
      _status = Status::INFLECTION;
    }
    else {
      _status = Status::INFO;
    }
  }
  else if ( RightHand.size() == 0 ){
    orig = toString( cls );
    _status = Status::STEM;
  }
  else {
    orig = toString( cls );
    orig += "_";
    for ( size_t i = 0; i < RightHand.size(); ++i ){
      orig += toString(RightHand[i]);
      if ( RightHand[i] == CLEX::AFFIX )
	ifpos = i;
    }
    _status = Status::DERIVATIONAL;
  }
}

BracketLeaf::BracketLeaf( CLEX::Type t, const UnicodeString& us, int flag ):
  BaseBracket( t, vector<CLEX::Type>(), flag ),
  morph( us )
{
  ifpos = -1;
  orig = toString( t );
  _status = Status::STEM;
}

BracketNest::BracketNest( CLEX::Type t,
			  CompoundType c,
			  int flag ): BaseBracket( t, flag ){
  _status = Status::COMPLEX;
  _compound = c;
}

BaseBracket *BracketNest::append( BaseBracket *t ){
  parts.push_back( t );
  return this;
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
  if ( _compound == CompoundType::NN ){
    result += " NN-compound";
  }
  else if ( _compound == CompoundType::PN ){
    result += " PN-compound";
  }
  if ( _compound == CompoundType::PV ){
    result += " PV-compound";
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

void BracketNest::setCompoundType(){
  if ( parts.size() == 1 ){
    auto part = *parts.begin();
    part->setCompoundType();
    _compound = part->compound();
  }
  else if ( parts.size() == 2
	    || parts.size() == 3 ){
    auto it = parts.begin();
    CLEX::Type tag1 = (*it)->tag();
    CompoundType cp1 = (*it)->compound();
    CLEX::Type tag2 = (*++it)->tag();
    if ( debugFlag > 5 ){
      cerr << "tag1 :" << tag1 << endl;
      cerr << "tag2 :" << tag2 << endl;
    }
    CLEX::Type tag3 = CLEX::NEUTRAL;
    if ( ++it != parts.end() ){
      tag3 = (*it)->tag();
      if ( debugFlag > 5 ){
	cerr << "extra tag :" << tag3 << endl;
      }
    }
    if ( tag3 != CLEX::NEUTRAL && tag3 != CLEX::UNASS ){
      return;
    }
    if ( tag1 == CLEX::N ){
      if ( tag2 ==CLEX::N ){
	_compound = CompoundType::NN;
      }
      else if ( tag2 == CLEX::NEUTRAL || tag2 == CLEX::UNASS ){
	_compound = cp1;
      }
    }
    else if ( tag1 == CLEX::P ){
      if ( tag2 == CLEX::N ){
	_compound = CompoundType::PN;
      }
      else if ( tag2 == CLEX::V ){
	_compound = CompoundType::PV;
      }
      else if ( tag2 == CLEX::NEUTRAL || tag2 == CLEX::UNASS ){
	_compound = cp1;
      }
    }
  }
  else {
    return;
  }
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
      BracketNest *tmp
	= new BracketNest( (*rpos)->tag(), CompoundType::NONE, debugFlag );
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
      tmp->setCompoundType();
      result.insert( ++bit, tmp );
      return bit;
    }
  }
  else {
    // the affix derivation failed.
    // we should try to start at the next node
    bit = rpos;
    return ++bit;
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
      BaseBracket *tmp
	= new BracketNest( CLEX::N, CompoundType::NN, debugFlag );
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
  BracketNest *brackets
    = new BracketNest( CLEX::UNASS, CompoundType::NONE, debugFlag );
  for ( size_t k=0; k < rules.size(); ++k ) {
    // fill a flat result;
    BracketLeaf *tmp = new BracketLeaf( rules[k], debugFlag );
    if ( tmp->status() == Status::STEM && tmp->morpheme().isEmpty() ){
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
  brackets->setCompoundType();
  tag = brackets->getFinalTag();
  if ( debugFlag > 4 ){
    cerr << "Final Bracketing:" << brackets << endl;
  }
  return brackets;
}
