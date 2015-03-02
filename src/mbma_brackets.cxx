/*
  $Id: mbma_mod.cxx 18070 2015-02-18 15:05:05Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/Frog/trunk/src/mbma_mod.cxx $

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

enum class CompoundType { NONE, NN, PN, PV };

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

BracketNest::BracketNest( CLEX::Type t, int flag ): BaseBracket( t, flag ){
  status = COMPLEX; compound = CompoundType::NONE;
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
