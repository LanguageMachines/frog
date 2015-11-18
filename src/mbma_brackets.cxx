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

string toString( const CompoundType& ct ){
  switch ( ct ){
  case CompoundType::NN:
    return "NN";
  case CompoundType::PN:
    return "PN";
  case CompoundType::PV:
    return "PV";
  default:
    return "none";
  }
}

ostream& operator<<( ostream& os, const CompoundType ct ){
  os << toString( ct );
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
  for ( auto const it : parts ){
    delete it;
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
  for ( auto const& it : parts ){
    result += it->put(noclass) + " ";
  }
  result += "]";
  if ( !noclass ){
    if ( cls != CLEX::UNASS )
      result += UTF8ToUnicode(toString(cls));
  }
  if ( _compound != CompoundType::NONE ){
    result += " " + UTF8ToUnicode(toString(_compound)) + "-compound";
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
  for ( auto const& it : parts ){
    res += it->deepmorphemes();
  }
  return res;
}

void prettyP( ostream& os, const list<BaseBracket*>& v ){
  os << "[";
  for ( auto const& it : v ){
    os << it << " ";
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
    Status st1 = (*it)->status();
    CLEX::Type tag2 = (*++it)->tag();
    Status st2 = (*it)->status();
    if ( ( st1 != Status::STEM && st1 != Status::COMPLEX )
	 || ( st2 != Status::STEM && st2 != Status::COMPLEX ) )
      return;
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

Morpheme *BracketLeaf::createMorpheme( Document *doc,
				       const string& mbma_tagset,
				       const string& clex_tagset ) const {
  string desc;
  int cnt = 0;
  return createMorpheme( doc, mbma_tagset, clex_tagset, desc, cnt );
}

Morpheme *BracketLeaf::createMorpheme( Document *doc,
				       const string& mbma_tagset,
				       const string& clex_tagset,
				       string& desc,
				       int& cnt ) const {
  Morpheme *result = 0;
  desc.clear();
  if ( _status == Status::COMPLEX ){
    abort();
  }
  if ( _status == Status::STEM ){
    KWargs args;
    args["set"] = mbma_tagset;
    args["class"] = "stem";
    result = new Morpheme( doc, args );
    args.clear();
    string out = UnicodeToUTF8(morph);
    args["value"] = out;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    ++cnt;
    args.clear();
    args["set"] = clex_tagset;
    args["cls"] = toString( tag() );
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
    desc = "[" + out + "]" + CLEX::get_tDescr( tag() ); // spread the word upwards!
  }
  else if ( _status == Status::INFLECTION ){
    KWargs args;
    args["class"] = "affix";
    args["set"] = mbma_tagset;
    result = new Morpheme( doc, args );
    args.clear();
    string out = UnicodeToUTF8(morph);
    if ( out.empty() )
      out = inflect;
    else
      desc = "[" + out + "]";
    args["value"] = out;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    ++cnt;
    args.clear();
    args["subset"] = "inflection";
    for ( size_t i=0; i < inflect.size(); ++i ){
      if ( inflect[i] != '/' ){
	string d = CLEX::get_iDescr(inflect[i]);
	args["class"] = d;
	desc += "/" + d;
	folia::Feature *feat = new folia::Feature( args );
#pragma omp critical(foliaupdate)
	{
	  result->append( feat );
	}
      }
    }
  }
  else if ( _status == Status::DERIVATIONAL ){
    KWargs args;
    args["class"] = "derivational";
    args["set"] = mbma_tagset;
    result = new Morpheme( doc, args );
    args.clear();
    string out = UnicodeToUTF8(morph);
    args["value"] = out;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    ++cnt;
    desc = "[" + out + "]"; // pass it up!
    for ( size_t i=0; i < inflect.size(); ++i ){
      if ( inflect[i] != '/' ){
	string d = CLEX::get_iDescr(inflect[i]);
	desc += "/" + d;
      }
    }
    args.clear();
    args["subset"] = "structure";
    args["class"]  = desc;
#pragma omp critical(foliaupdate)
    {
      folia::Feature *feat = new folia::Feature( args );
      result->append( feat );
    }
    args.clear();
    args["set"] = clex_tagset;
    args["cls"] = orig;
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
  }
  else if ( _status == Status::INFO ){
    KWargs args;
    args["class"] = "inflection";
    args["set"] = mbma_tagset;
    result = new Morpheme( doc, args );
    args.clear();
    args["subset"] = "inflection";
    for ( size_t i=0; i < inflect.size(); ++i ){
      if ( inflect[i] != '/' ){
	string d = CLEX::get_iDescr(inflect[i]);
	desc += "/" + d;
	args["class"] = d;
	folia::Feature *feat = new folia::Feature( args );
#pragma omp critical(foliaupdate)
	{
	  result->append( feat );
	}
      }
    }
  }
  return result;
}

Morpheme *BracketNest::createMorpheme( Document *doc,
				       const string& mbma_tagset,
				       const string& clex_tagset ) const {
  string desc;
  int cnt = 0;
  return createMorpheme( doc, mbma_tagset, clex_tagset, desc, cnt );
}

Morpheme *BracketNest::createMorpheme( Document *doc,
				       const string& mbma_tagset,
				       const string& clex_tagset,
				       string& desc,
				       int& cnt ) const {
  KWargs args;
  args["class"] = "complex";
  args["set"] = mbma_tagset;
  Morpheme *result = new Morpheme( doc, args );
  string mor;
  cnt = 0;
  desc.clear();
  vector<Morpheme*> stack;
  for ( auto const& it : parts ){
    string deeper_desc;
    int deep_cnt = 0;
    Morpheme *m = it->createMorpheme( doc,
				      mbma_tagset,
				      clex_tagset,
				      deeper_desc,
				      deep_cnt );
    if ( m ){
      desc += deeper_desc;
      cnt += deep_cnt;
      stack.push_back( m );
    }
  }
  if ( cnt > 1 ){
    desc = "[" + desc + "]" + CLEX::get_tDescr( tag() );
  }
  cnt = 1;
  args.clear();
  args["subset"] = "structure";
  args["class"]  = desc;
#pragma omp critical(foliaupdate)
  {
    folia::Feature *feat = new folia::Feature( args );
    result->append( feat );
  }
  args.clear();
  args["set"] = clex_tagset;
  args["cls"] = toString( tag() );
  PosAnnotation *pos = 0;
#pragma omp critical(foliaupdate)
  {
    pos = result->addPosAnnotation( args );
  }
  CompoundType ct = compound();
  if ( ct != CompoundType::NONE ){
    args.clear();
    args["subset"] = "compound";
    args["class"]  = toString(ct);
#pragma omp critical(foliaupdate)
    {
      folia::Feature *feat = new folia::Feature( args );
      pos->append( feat );
    }
  }
#pragma omp critical(foliaupdate)
  for ( size_t i=0; i < stack.size(); ++i ){
    result->append( stack[i] );
  }
  return result;
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
  cls = CLEX::UNASS;
  auto it = parts.rbegin();
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
