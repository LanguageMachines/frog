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
#include "ucto/unicode.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/folia.h"
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
  case CompoundType::NA:
    return "NA";
  case CompoundType::AN:
    return "AN";
  case CompoundType::AA:
    return "AA";
  default:
    return "none";
  }
}

ostream& operator<<( ostream& os, const CompoundType& ct ){
  os << toString( ct );
  return os;
}

string toString( const Status& st ){
  switch ( st ){
  case Status::INFO:
    return "info";
  case Status::STEM:
    return "stem";
  case Status::COMPLEX:
    return "complex";
  case Status::INFLECTION:
    return "inflection";
  case Status::DERIVATIONAL:
    return "derivational";
  case Status::FAILED:
    return "failed";
  default:
    return "nostat";
  }
}

ostream& operator<<( ostream& os, const Status& st ){
  os << toString( st );
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
      if ( RightHand[i] == CLEX::AFFIX ){
	ifpos = i;
      }
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
  for ( auto const& it : parts ){
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
    if ( orig.empty() ){
      result += UTF8ToUnicode(inflect);
    }
    else {
      result += UTF8ToUnicode(orig);
    }
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
    if ( cls != CLEX::UNASS ){
      result += UTF8ToUnicode(toString(cls));
    }
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
    if ( (*rpos)->RightHand[j] == CLEX::XAFFIX){
      continue;
    }
    else if ( (*rpos)->RightHand[j] == CLEX::AFFIX){
      continue;
    }
    else if ( (*rpos)->RightHand[j] != (*it)->tag() ){
      if ( debugFlag > 5 ){
	cerr << "test MATCH FAIL (" << (*rpos)->RightHand[j]
	     << " != " << (*it)->tag() << ")" << endl;
      }
      (*rpos)->set_status(Status::FAILED);
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

CompoundType BracketNest::getCompoundType(){
  if ( debugFlag > 5 ){
    cerr << "get compoundType: " << this << endl;
    cerr << "#parts: " << parts.size() << endl;
  }
  CompoundType compound = CompoundType::NONE;
  if ( parts.size() == 1 ){
    auto part = *parts.begin();
    compound = part->getCompoundType();
  }
  else if ( parts.size() == 2 ){
    auto it = parts.begin();
    CLEX::Type tag1 = (*it)->tag();
    CompoundType cp1 = (*it)->compound();
    Status st1 = (*it)->status();
    CLEX::Type tag2 = (*++it)->tag();
    Status st2 = (*it)->status();
    if ( debugFlag > 5 ){
      cerr << "tag1 :" << tag1 << " stat1: " << st1 << " cp1: " << cp1 << endl;
      cerr << "tag2 :" << tag2 << " stat2: " << st2 << endl;
    }
    if ( st1 != Status::FAILED && st2 != Status::FAILED ){
      if ( tag1 == CLEX::N ){
	if ( tag2 == CLEX::N && st2 == Status::STEM ){
	  compound = CompoundType::NN;
	}
	else if ( st2 == Status::DERIVATIONAL
		  || st2 == Status::INFO
		  || st2 == Status::INFLECTION ){
	  compound = cp1;
	}
	else if ( tag2 == CLEX::A && st2 == Status::STEM ){
	  compound = CompoundType::NA;
	}
      }
      else if ( tag1 == CLEX::A
		&& ( st1 == Status::STEM || st1 == Status::COMPLEX) ){
	if ( tag2 == CLEX::N && st2 == Status::STEM ){
	  compound = CompoundType::AN;
	}
	else if ( st2 == Status::DERIVATIONAL
		  || st2 == Status::INFO
		  || st2 == Status::INFLECTION ){
	  compound = cp1;
	}
      }
      else if ( tag1 == CLEX::P ){
	if ( tag2 == CLEX::N ){
	  compound = CompoundType::PN;
	}
	else if ( tag2 == CLEX::V ){
	  compound = CompoundType::PV;
	}
	else if ( tag2 == CLEX::NEUTRAL || tag2 == CLEX::UNASS ){
	  compound = cp1;
	}
      }
    }
  }
  else if ( parts.size() > 2 ){
    auto it = parts.begin();
    CompoundType cp1 = (*it)->compound();
    CLEX::Type tag1 = (*it)->tag();
    Status st1 = (*it)->status();
    CLEX::Type tag2 = (*++it)->tag();
    Status st2 = (*it)->status();
    CLEX::Type tag3 = (*++it)->tag();
    Status st3 = (*it)->status();
    if ( debugFlag > 5 ){
      cerr << "tag1 :" << tag1 << " stat1: " << st1 << endl;
      cerr << "tag2 :" << tag2 << " stat2: " << st2 << endl;
      cerr << "tag3 :" << tag3 << " stat3: " << st3 << endl;
    }
    if ( st1 != Status::FAILED
	 && st2 != Status::FAILED
	 && st3 != Status::FAILED ){
      if ( tag1 == CLEX::N ){
	if ( st1 == Status::STEM || st1 == Status::COMPLEX ){
	  if ( (tag2 == CLEX::N &&
		( st2 == Status::STEM || st2 == Status::COMPLEX ) )
	       && (tag3 == CLEX::NEUTRAL || st3 == Status::INFLECTION ) ) {
	    compound = CompoundType::NN;
	  }
	  else if ( ( tag2 == CLEX::A &&
		      ( st2 == Status::STEM || st2 == Status::COMPLEX ) )
		    && ( tag3 == CLEX::A && st3 == Status::DERIVATIONAL ) ){
	    compound = CompoundType::NA;
	  }
	  else if ( st2 == Status::DERIVATIONAL && tag3 == CLEX::NEUTRAL ){
	    compound = cp1;
	  }
	  else if ( st2 == Status::DERIVATIONAL && tag3 == CLEX::N ){
	    compound = CompoundType::NN;
	  }
	}
	else {
	  if ( tag2 == CLEX::N && tag3 == CLEX::N ){
	    compound = CompoundType::NN;
	  }
	}
      }
      else if ( tag1 == CLEX::A ){
	if ( st1 == Status::STEM || st1 == Status::COMPLEX ){
	  if ( tag2 == CLEX::N
	       && ( tag3 == CLEX::NEUTRAL ||  tag3 == CLEX::UNASS ) ){
	    compound = CompoundType::AN;
	  }
	  else if ( tag2 == CLEX::A
		    && ( tag3 == CLEX::NEUTRAL ||  tag3 == CLEX::UNASS ) ){
	    compound = CompoundType::AA;
	  }
	  else if ( st2 == Status::INFLECTION && st3 == Status::INFLECTION ){
	    compound = cp1;
	  }
	}
      }
      else if ( tag1 == CLEX::P ){
	if ( tag2 == CLEX::N && tag3 == CLEX::NEUTRAL ){
	  compound = CompoundType::PN;
	}
	else if ( tag2 == CLEX::V && tag3 == CLEX::NEUTRAL ){
	  compound = CompoundType::PV;
	}
      }
    }
  }
  if ( debugFlag > 5 ){
    cerr << "   ASSIGNED :" << compound << endl;
  }
  _compound = compound;
  return compound;
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
    result = new Morpheme( args, doc );
    args.clear();
    string out = UnicodeToUTF8(morph);
    if ( out.empty() ){
      throw logic_error( "stem has empty morpheme" );
    }
    args["value"] = out;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    ++cnt;
    args.clear();
    args["set"] = clex_tagset;
    args["class"] = toString( tag() );
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
    result = new Morpheme( args, doc );
    args.clear();
    string out = UnicodeToUTF8(morph);
    if ( out.empty() ){
      out = inflect;
    }
    else {
      desc = "[" + out + "]";
    }
    if ( out.empty() ){
      throw logic_error( "Inflection and morpheme empty" );
    }
    args["value"] = out;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    ++cnt;
    args.clear();
    args["subset"] = "inflection";
    for ( const auto& inf : inflect ){
      if ( inf != '/' ){
	string d = CLEX::get_iDescr(inf);
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
  else if ( _status == Status::DERIVATIONAL || _status == Status::FAILED ){
    KWargs args;
    args["class"] = "derivational";
    args["set"] = mbma_tagset;
    result = new Morpheme( args, doc );
    args.clear();
    string out = UnicodeToUTF8(morph);
    if ( out.empty() ){
      cerr << "problem: " << this << endl;
      throw logic_error( "Derivation with empty morpheme" );
    }
    args["value"] = out;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    ++cnt;
    desc = "[" + out + "]"; // pass it up!
    for ( const auto& inf : inflect ){
      if ( inf != '/' ){
	string d = CLEX::get_iDescr( inf );
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
    args["class"] = orig;
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
  }
  else if ( _status == Status::INFO ){
    KWargs args;
    args["class"] = "inflection";
    args["set"] = mbma_tagset;
    result = new Morpheme( args, doc );
    args.clear();
    args["subset"] = "inflection";
    for ( const auto& inf : inflect ){
      if ( inf != '/' ){
	string d = CLEX::get_iDescr( inf );
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
  Morpheme *result = new Morpheme( args, doc );
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
  args["class"] = toString( tag() );
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
  for ( const auto& s : stack ){
    result->append( s );
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
      _compound = tmp->getCompoundType();
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
	 && ( (*it)->tag() == CLEX::N && (*it)->status() == Status::STEM )
	 && (*it)->RightHand.size() == 0 ){
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

void BracketNest::clearEmptyNodes(){
  // remove all nodes that don't have a morpheme or an inlection
  if ( debugFlag > 5 ){
    cerr << "clear emptyNodes: " << this << endl;
  }
  list<BaseBracket*> out;
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    if ( debugFlag > 5 ){
      cerr << "loop clear emptyNodes : " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	cerr << "nested! " << endl;
      }
      (*it)->clearEmptyNodes( );
      out.push_back( *it );
    }
    else {
      if ( (*it)->morpheme().isEmpty() &&
	   (*it)->inflection().empty() ){
	// skip
      }
      else {
	out.push_back( *it );
      }
    }
    ++it;
  }
  parts.swap( out );
  if ( debugFlag > 5 ){
    cerr << "RESULT clear emptyNodes: " << this << endl;
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
