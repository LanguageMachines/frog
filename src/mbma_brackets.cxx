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
#include "frog/mbma_mod.h"
#include "frog/mbma_brackets.h"

using namespace std;
using namespace TiCC;
using namespace folia;

string toString( const Compound::Type& ct ){
  switch ( ct ){
  case Compound::Type::NN:
    return "NN";
  case Compound::Type::NNN:
    return "NNN";
  case Compound::Type::NVN:
    return "NVN";
  case Compound::Type::NA:
    return "NA";
  case Compound::Type::NB:
    return "NB";
  case Compound::Type::NP:
    return "NP";
  case Compound::Type::NV:
    return "NV";
  case Compound::Type::AN:
    return "AN";
  case Compound::Type::AA:
    return "AA";
  case Compound::Type::AB:
    return "AB";
  case Compound::Type::AP:
    return "AP";
  case Compound::Type::AV:
    return "AV";
  case Compound::Type::BN:
    return "BN";
  case Compound::Type::BA:
    return "BA";
  case Compound::Type::BB:
    return "BB";
  case Compound::Type::BP:
    return "BP";
  case Compound::Type::BV:
    return "BV";
  case Compound::Type::PN:
    return "PN";
  case Compound::Type::PA:
    return "PA";
  case Compound::Type::PB:
    return "PB";
  case Compound::Type::PP:
    return "PP";
  case Compound::Type::PV:
    return "PV";
  case Compound::Type::VN:
    return "VN";
  case Compound::Type::VA:
    return "VA";
  case Compound::Type::VB:
    return "VB";
  case Compound::Type::VP:
    return "VP";
  case Compound::Type::VV:
    return "VV";
  case Compound::Type::NONE:
    return "none";
  }
  return "DEADLY";
}

Compound::Type stringToCompound( const string& s ){
  if ( s == "NN" ){
    return Compound::Type::NN;
  }
  else if ( s == "NNN" ){
    return Compound::Type::NNN;
  }
  else if ( s == "NVN" ){
    return Compound::Type::NVN;
  }
  else if ( s == "NA" ){
    return Compound::Type::NA;
  }
  else if ( s == "NB" ){
    return Compound::Type::NB;
  }
  else if ( s == "NP" ){
    return Compound::Type::NP;
  }
  else if ( s == "NV" ){
    return  Compound::Type::NV;
  }
  else if ( s == "AN" ){
    return  Compound::Type::AN;
  }
  else if ( s == "AA" ){
    return  Compound::Type::AA;
  }
  else if ( s == "AB" ){
    return  Compound::Type::AB;
  }
  else if ( s == "AP" ){
    return  Compound::Type::AP;
  }
  else if ( s == "AV" ){
    return  Compound::Type::AV;
  }
  else if ( s == "BN" ){
    return  Compound::Type::BN;
  }
  else if ( s == "BA" ){
    return  Compound::Type::BA;
  }
  else if ( s == "BB" ){
    return  Compound::Type::BB;
  }
  else if ( s == "BP" ){
    return  Compound::Type::BP;
  }
  else if ( s == "BV" ){
    return  Compound::Type::BV;
  }
  else if ( s == "PN" ){
    return  Compound::Type::PN;
  }
  else if ( s == "PA" ){
    return  Compound::Type::PA;
  }
  else if ( s == "PB" ){
    return  Compound::Type::PB;
  }
  else if ( s == "PP" ){
    return  Compound::Type::PP;
  }
  else if ( s == "PV" ){
    return  Compound::Type::PV;
  }
  else if ( s == "VN" ){
    return  Compound::Type::VN;
  }
  else if ( s == "VA" ){
    return  Compound::Type::VA;
  }
  else if ( s == "VB" ){
    return  Compound::Type::VB;
  }
  else if ( s == "VP" ){
    return  Compound::Type::VP;
  }
  else if ( s == "VV" ){
    return  Compound::Type::VV;
  }
  else if ( s.empty() || s == "none" ){
    return  Compound::Type::NONE;
  }
  else {
    throw runtime_error( "no such compound:" + s );
  }
}

ostream& operator<<( ostream& os, const Compound::Type& ct ){
  os << toString( ct );
  return os;
}

string toString( const Status& st ){
  switch ( st ){
  case Status::INFO:
    return "info";
  case Status::STEM:
    return "stem";
  case Status::PARTICLE:
    return "particle";
  case Status::PARTICIPLE:
    return "participle";
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
  glue(false),
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
    if ( ( p.ResultClass == CLEX::N
	   || p.ResultClass == CLEX::V
	   || p.ResultClass == CLEX::A )
	 &&
	 ( morph == "be" || morph == "ge" || morph == "ver" || morph == "ex" ) ){
      _status = Status::PARTICLE;
    }
    else {
      _status = Status::STEM;
    }
  }
  else {
    orig = toString( cls );
    orig += "_";
    glue = p.is_glue;
    for ( size_t i = 0; i < RightHand.size(); ++i ){
      orig += toString(RightHand[i]);
      if ( RightHand[i] == CLEX::AFFIX ){
	ifpos = i;
      }
    }
    if ( morph == "be" || morph == "ge" || morph == "ver" || morph == "ex" ){
      _status = Status::PARTICIPLE;
    }
    else {
      _status = Status::DERIVATIONAL;
    }
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
			  Compound::Type c,
			  int flag ): BaseBracket( t, flag ),
				      _compound( c )
{
  _status = Status::COMPLEX;
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

UnicodeString BaseBracket::put( bool full ) const {
  UnicodeString result = "[err?]";
  if ( full ){
    UnicodeString s = UTF8ToUnicode(toString(cls));
    result += s;
  }
  return result;
}

UnicodeString BracketLeaf::put( bool full ) const {
  UnicodeString result;
  if ( !morph.isEmpty() ){
    result += "[";
    result += morph;
    result += "]";
  }
  if ( full ){
    if ( orig.empty() ){
      result += UTF8ToUnicode(inflect);
    }
    else {
      result += UTF8ToUnicode(orig);
    }
  }
  return result;
}

UnicodeString BracketNest::put( bool full ) const {
  UnicodeString result = "[ ";
  for ( auto const& it : parts ){
    UnicodeString m = it->put( full );
    if ( !m.isEmpty() ){
      result +=  m + " ";
    }
  }
  result += "]";
  if ( full ){
    if ( cls != CLEX::UNASS ){
      result += UTF8ToUnicode(toString(cls));
    }
    if ( _compound != Compound::Type::NONE ){
      result += " " + UTF8ToUnicode(toString(_compound)) + "-compound";
    }
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
    cerr << "test MATCH: rpos= " << *rpos << endl;
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
    if ( (*rpos)->RightHand[j] == CLEX::XAFFIX ){
      continue;
    }
    else if ( (*rpos)->RightHand[j] == CLEX::AFFIX ){
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

Compound::Type construct( const vector<CLEX::Type> tags ){
  string s;
  for ( const auto& t : tags ){
    s += toString( t );
  }
  try {
    return stringToCompound( s );
  }
  catch (...){
    return Compound::Type::NONE;
  }
}

Compound::Type construct( const CLEX::Type tag1, const CLEX::Type tag2 ){
  vector<CLEX::Type> v;
  v.push_back( tag1 );
  v.push_back( tag2 );
  return construct( v );
}

Compound::Type BracketNest::getCompoundType(){
  if ( debugFlag > 5 ){
    cerr << "get compoundType: " << this << endl;
    cerr << "#parts: " << parts.size() << endl;
  }
  Compound::Type compound = Compound::Type::NONE;
  if ( parts.size() == 1 ){
    auto part = *parts.begin();
    compound = part->getCompoundType();
  }
  else if ( parts.size() == 2 ){
    auto it = parts.begin();
    CLEX::Type tag1 = (*it)->tag();
    Compound::Type cp1 = (*it)->compound();
    Status st1 = (*it)->status();
    CLEX::Type tag2 = (*++it)->tag();
    Compound::Type cp2 = (*it)->compound();
    Status st2 = (*it)->status();
    if ( debugFlag > 5 ){
      cerr << "tag1 :" << tag1 << " stat1: " << st1 << " cp1: " << cp1 << endl;
      cerr << "tag2 :" << tag2 << " stat2: " << st2 << " cp2: " << cp2 << endl;
    }
    if ( st1 != Status::FAILED
	 && st2 != Status::FAILED ){
      if ( tag1 == CLEX::N
	   && st1 != Status::DERIVATIONAL
	   && st1 != Status::PARTICLE
	   && st1 != Status::PARTICIPLE ){
	if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
	else if ( st2 == Status::DERIVATIONAL
		  || st2 == Status::INFO
		  || st2 == Status::INFLECTION ){
	  compound = cp1;
	}
      }
      else if ( tag1 == CLEX::A
		&& st1 != Status::PARTICLE
		&& st1 != Status::PARTICIPLE ){
	if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
	else if ( st2 == Status::DERIVATIONAL
		  || st2 == Status::INFO
		  || st2 == Status::INFLECTION ){
	  compound = cp1;
	}
      }
      else if ( tag1 == CLEX::B ){
	if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
      }
      else if ( tag1 == CLEX::P ){
	if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
	else if ( tag2 == CLEX::NEUTRAL || tag2 == CLEX::UNASS ){
	  compound = cp1;
	}
      }
      else if ( tag1 == CLEX::V ){
	if ( st1 != Status::PARTICLE
	     && st1 != Status::PARTICIPLE ){
	  if ( st1 == Status::DERIVATIONAL ){
	    compound = cp2;
	  }
	  else if ( st2 == Status::STEM ){
	    compound = construct( tag1, tag2 );
	  }
	}
	else if ( st2 == Status::COMPLEX ) {
	  compound = construct( tag1, tag2 );
	}
      }
    }
  }
  else if ( parts.size() > 2 ){
    auto it = parts.begin();
    Compound::Type cp1 = (*it)->compound();
    CLEX::Type tag1 = (*it)->tag();
    Status st1 = (*it)->status();
    Compound::Type cp2 = (*++it)->compound();
    CLEX::Type tag2 = (*it)->tag();
    Status st2 = (*it)->status();
    Compound::Type cp3 = (*++it)->compound();
    CLEX::Type tag3 = (*it)->tag();
    Status st3 = (*it)->status();
    if ( debugFlag > 5 ){
      cerr << "tag1 :" << tag1 << " stat1: " << st1 << " cp1: " << cp1 << endl;
      cerr << "tag2 :" << tag2 << " stat2: " << st2 << " cp2: " << cp2 << endl;
      cerr << "tag3 :" << tag3 << " stat3: " << st3 << " cp3: " << cp3 << endl;
    }
    if ( st1 != Status::FAILED
	 && st2 != Status::FAILED
	 && st3 != Status::FAILED
	 && st1 != Status::PARTICLE
	 && st1 != Status::PARTICIPLE ){
      if ( tag1 == CLEX::N ){
	if ( st2 == Status::STEM &&
	     ( st3 == Status::INFLECTION || tag3 == CLEX::NEUTRAL ) ){
	  compound = construct( tag1, tag2 );
	}
	else if ( st1 == Status::STEM || st1 == Status::COMPLEX ){
	  if ( (tag2 == CLEX::N &&
		( st2 == Status::STEM || st2 == Status::COMPLEX ) )
	       && (tag3 == CLEX::NEUTRAL || st3 == Status::INFLECTION ) ) {
	    compound = Compound::Type::NN;
	  }
	  else if ( ( tag2 == CLEX::V && st2 == Status::STEM )
		    && ( tag3 == CLEX::N && st3 == Status::STEM ) ){
	    compound = Compound::Type::NVN;
	  }
	  else if ( ( tag2 == CLEX::A &&
		      ( st2 == Status::STEM || st2 == Status::COMPLEX ) )
		    && ( tag3 == CLEX::A && st3 == Status::DERIVATIONAL ) ){
	    compound = Compound::Type::NA;
	  }
	  else if ( st2 == Status::DERIVATIONAL && tag3 == CLEX::NEUTRAL ){
	    compound = cp1;
	  }
	  else if ( st2 == Status::INFLECTION && tag3 == CLEX::NEUTRAL ){
	    compound = cp1;
	  }
	  else if (  st2 == Status::DERIVATIONAL
		     && tag3 == CLEX::N ){
	    if  ( cp3 == Compound::Type::NN ||
		  cp3 == Compound::Type::NNN ) {
	      compound = Compound::Type::NNN;
	    }
	    else {
	      compound = Compound::Type::NN;
	    }
	  }
	  else if ( st3 == Status::DERIVATIONAL
		    && tag3 == CLEX::N ){
	    compound = Compound::Type::NN;
	  }
	}
	else if ( tag2 == CLEX::N && tag3 == CLEX::N ){
	  compound = Compound::Type::NNN;
	}
      }
      else if ( tag1 == CLEX::A ){
	if ( st2 == Status::STEM &&
	     ( st3 == Status::INFLECTION || tag3 == CLEX::NEUTRAL ) ){
	  compound = construct( tag1, tag2 );
	}
	else if ( st1 == Status::STEM || st1 == Status::COMPLEX ){
	  if ( tag2 == CLEX::N
	       && ( tag3 == CLEX::NEUTRAL ||  tag3 == CLEX::UNASS ) ){
	    compound = Compound::Type::AN;
	  }
	  else if ( tag2 == CLEX::A
		    && ( tag3 == CLEX::NEUTRAL ||  tag3 == CLEX::UNASS ) ){
	    compound = Compound::Type::AA;
	  }
	  else if ( st2 == Status::INFLECTION && st3 == Status::INFLECTION ){
	    compound = cp1;
	  }
	}
      }
      else if ( tag1 == CLEX::P ){
	if ( st2 == Status::STEM &&
	     ( st3 == Status::INFLECTION || tag3 == CLEX::NEUTRAL ) ){
	  compound = construct( tag1, tag2 );
	}
	else if ( st2 == Status::COMPLEX ){
	  compound = cp2;
	}
	else if ( tag3 == CLEX::NEUTRAL ){
	  compound = construct( tag1, tag2 );
	}
	else if ( st3 == Status::DERIVATIONAL ){
	  compound = construct( tag1, tag3 );
	}
      }
      else if ( tag1 == CLEX::B && st1 == Status::STEM ){
      	if ( ( st2 == Status::STEM
	       && ( st3 == Status::INFLECTION || tag3 == CLEX::NEUTRAL ) ) ){
	  compound = construct( tag1, tag2 );
	}
      	else if ( st2 == Status::COMPLEX ){
	  compound = cp2;
	}
      }
      else if ( tag1 == CLEX::V ){
      	if ( st2 == Status::STEM &&
	     ( st3 == Status::INFLECTION || tag3 == CLEX::NEUTRAL ) ){
	  compound = construct( tag1, tag2 );
	}
      	else if ( st3 == Status::STEM &&
	     ( st2 == Status::INFLECTION ) ){
	  compound = construct( tag1, tag3 );
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

Morpheme *BracketLeaf::createMorpheme( Document *doc ) const {
  string desc;
  int cnt = 0;
  return createMorpheme( doc, desc, cnt );
}

Morpheme *BracketLeaf::createMorpheme( Document *doc,
				       string& desc,
				       int& cnt ) const {
  Morpheme *result = 0;
  desc.clear();
  if ( _status == Status::COMPLEX ){
    abort();
  }
  if ( _status == Status::STEM ){
    KWargs args;
    args["set"] = Mbma::mbma_tagset;
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
    args["set"] = Mbma::clex_tagset;
    args["class"] = toString( tag() );
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
    desc = "[" + out + "]" + CLEX::get_tDescr( tag() ); // spread the word upwards!
  }
  if ( _status == Status::PARTICLE ){
    KWargs args;
    args["set"] = Mbma::mbma_tagset;
    args["class"] = "particle";
    result = new Morpheme( args, doc );
    args.clear();
    string out = UnicodeToUTF8(morph);
    if ( out.empty() ){
      throw logic_error( "particle has empty morpheme" );
    }
    args["value"] = out;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      result->append( t );
    }
    ++cnt;
    args.clear();
    args["set"] = Mbma::clex_tagset;
    args["class"] = toString( tag() );
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
    desc = "[" + out + "]"; // spread the word upwards! maybe add 'part' ??
  }
  else if ( _status == Status::INFLECTION ){
    KWargs args;
    args["class"] = "affix";
    args["set"] = Mbma::mbma_tagset;
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
	if ( !d.empty() ){
	  // happens sometimes when there is fawlty data
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
  }
  else if ( _status == Status::DERIVATIONAL
	    || _status == Status::PARTICIPLE
	    || _status == Status::FAILED ){
    KWargs args;
    args["class"] = "derivational";
    args["set"] = Mbma::mbma_tagset;
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
    string tag = orig;
    string::size_type pos = tag.find( "^" );
    if ( pos != string::npos ){
      // A glue tag! Override orig with the implicit tag!
      tag = orig[pos+1];
      desc += CLEX::get_tDescr( CLEX::toCLEX(tag) );
    }
    for ( const auto& inf : inflect ){
      if ( inf != '/' ){
	string d = CLEX::get_iDescr( inf );
	if ( !d.empty() ){
	  // happens sometimes when there is fawlty data
	  desc += "/" + d;
	}
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
    args["set"] = Mbma::clex_tagset;
    args["class"] = tag;
#pragma omp critical(foliaupdate)
    {
      result->addPosAnnotation( args );
    }
  }
  else if ( _status == Status::INFO ){
    KWargs args;
    args["class"] = "inflection";
    args["set"] = Mbma::mbma_tagset;
    result = new Morpheme( args, doc );
    args.clear();
    args["subset"] = "inflection";
    for ( const auto& inf : inflect ){
      if ( inf != '/' ){
	string d = CLEX::get_iDescr( inf );
	if ( !d.empty() ){
	  // happens sometimes when there is fawlty data
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
  }
  return result;
}

Morpheme *BracketNest::createMorpheme( Document *doc ) const {
  string desc;
  int cnt = 0;
  return createMorpheme( doc, desc, cnt );
}

Morpheme *BracketNest::createMorpheme( Document *doc,
				       string& desc,
				       int& cnt ) const {
  KWargs args;
  args["class"] = "complex";
  args["set"] = Mbma::mbma_tagset;
  Morpheme *result = new Morpheme( args, doc );
  string mor;
  cnt = 0;
  desc.clear();
  vector<Morpheme*> stack;
  for ( auto const& it : parts ){
    string deeper_desc;
    int deep_cnt = 0;
    Morpheme *m = it->createMorpheme( doc,
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
  args["set"] = Mbma::clex_tagset;
  args["class"] = toString( tag() );
  PosAnnotation *pos = 0;
#pragma omp critical(foliaupdate)
  {
    pos = result->addPosAnnotation( args );
  }
  Compound::Type ct = compound();
  if ( ct != Compound::Type::NONE ){
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
	= new BracketNest( (*rpos)->tag(), Compound::Type::NONE, debugFlag );
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
      Compound::Type newt = Compound::Type::NN;
      if ( (*prev)->compound() == Compound::Type::NN
	   || (*prev)->compound() == Compound::Type::NN ){
	newt = Compound::Type::NNN;
      }
      BaseBracket *tmp = new BracketNest( CLEX::N, newt, debugFlag );
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

list<BaseBracket*>::iterator BracketNest::glue( list<BaseBracket*>& result,
						const list<BaseBracket*>::iterator& rpos ){
  if ( debugFlag > 5 ){
    cerr << "glue " << endl;
  }
  size_t len = (*rpos)->RightHand.size();
  bool matched = true;
  if ( len == 0 || len > result.size() ){
    if ( debugFlag > 5 ){
      cerr << "test MATCH FAIL (no RHS or RHS > result)" << endl;
    }
    matched = false;
  }
  else {
    size_t j = 0;
    list<BaseBracket*>::iterator it = rpos;
    for (; j < len && it != result.end(); ++j, ++it ){
      if ( debugFlag > 5 ){
	cerr << "test MATCH vergelijk " << *it << " met " << (*rpos)->RightHand[j] << endl;
      }
      if ( (*rpos)->RightHand[j] == CLEX::GLUE ){
	++j;
	continue;
      }
      else if ( (*rpos)->RightHand[j] != (*it)->tag() ){
	if ( debugFlag > 5 ){
	  cerr << "test MATCH FAIL (" << (*rpos)->RightHand[j]
	       << " != " << (*it)->tag() << ")" << endl;
	}
	(*rpos)->set_status(Status::FAILED);
	matched = false;
      }
    }
  }
  if ( matched ){
    list<BaseBracket*>::iterator bit = rpos;
    vector<CLEX::Type> tags;
    if ( debugFlag > 5 ){
      cerr << "OK een match" << endl;
    }
    list<BaseBracket*>::iterator it = bit--;
    BracketNest *tmp
      = new BracketNest( (*rpos)->tag(), Compound::Type::NONE, debugFlag );
    for ( size_t j = 0; j < len-1; ++j ){
      tmp->append( *it );
      tags.push_back( (*it)->tag() );
      if ( debugFlag > 5 ){
	cerr << "erase " << *it << endl;
      }
      it = result.erase(it);
    }
    if ( debugFlag > 5 ){
      cerr << "new node:" << tmp << endl;
    }
    tmp->_compound = construct( tags );
    result.insert( ++bit, tmp );
    return bit;
  }
  else {
    // the glueing failed.
    // we should try to start at the next node
    list<BaseBracket*>::iterator bit = rpos;
    return ++bit;
  }
}


void BracketNest::resolveGlue( ){
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // search for glue rules
    if ( debugFlag > 5 ){
      cerr << "search glue: bekijk: " << *it << endl;
    }
    if ( (*it)->isglue() ){
      it = glue( parts, it );
    }
    else {
      ++it;
    }
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
  CLEX::Type result_cls = CLEX::UNASS;
  auto it = parts.rbegin();
  while ( it != parts.rend() ){
    //    cerr << "bekijk: " << *it << endl;
    if ( (*it)->isNested()
	 || ( (*it)->inflection().empty()
	      && !(*it)->morpheme().isEmpty() ) ){
      result_cls = (*it)->tag();
      //      cerr << "maybe tag = " << result_cls << endl;
      if ( result_cls != CLEX::P ){
	auto it2 = it;
	++it2;
	if ( it2 != parts.rend() ){
	  //	  cerr << "bekijk ook " << *it2 << endl;
	  if ( (*it2)->infixpos() == 0 ){
	    result_cls = (*it2)->tag();
	  }
	}
	// in case of P we hope for better
	// in case of a X_*Y rule we need X
	break;
      }
    }
    ++it;
  }
  //  cerr << "final tag = " << result_cls << endl;
  cls = result_cls;
  return cls;
}
