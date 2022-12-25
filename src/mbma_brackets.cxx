/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2023
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

#include "frog/mbma_brackets.h"

#include <cassert>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include "ticcutils/Configuration.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "libfolia/folia.h"
#include "frog/clex.h"
#include "frog/mbma_rule.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace icu;
using TiCC::operator<<;

#define LOG *TiCC::Log(myLog)

string toString( const Compound::Type& ct ){
  /// return the string representation for the Compound::Type
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
  /// give the Compound::Type for this string
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
  /// output a CompoundType to a stream
  os << toString( ct );
  return os;
}

string toString( const Status& st ){
  /// return the string representation so a Status value
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
  /// output a Status to a stream
  os << toString( st );
  return os;
}

BracketLeaf::BracketLeaf( const RulePart& p,
			  int debug_flag,
			  TiCC::LogStream& l ):
  BaseBracket(p.ResultClass, p.RightHand, debug_flag, l ),
  glue(false),
  morph(p.morpheme )
{
  /// create a BracketLeaf object from a RulePart
  /*!
    \param p A Rulepart to create from
    \param debug_flag the debug value
    \param l a LogStream for messages
  */
  ifpos = -1;
  if ( !p.inflect.isEmpty() ){
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

BracketLeaf::BracketLeaf( CLEX::Type t,
			  const UnicodeString& morpheme,
			  int debug_flag,
			  TiCC::LogStream& l ):
  BaseBracket( t, vector<CLEX::Type>(), debug_flag, l ),
  morph( morpheme )
{
  /// create a BracketLeaf object from a CLEX::Type and a morpheme
  /*!
    \param t A CLEX::Type
    \param morpheme the (Unicode) morpheme
    \param debug_flag the debug value
    \param l a LogStream for messages
  */
  ifpos = -1;
  orig = toString( t );
  _status = Status::STEM;
}

BracketLeaf *BracketLeaf::clone() const{
  /// make a copy of the BracketLeaf
  return new BracketLeaf( *this );
}

BracketNest::BracketNest( CLEX::Type t,
			  Compound::Type c,
			  int debug_flag,
			  TiCC::LogStream& l ):
  BaseBracket( t, debug_flag, l ),
  _compound( c )
{
  /// create a BracketNest object from a CLEX::Type and a CompoundType
  /*!
    \param t A CLEX::Type
    \param c a Compound::Type
    \param debug_flag the debug value
    \param l a LogStream for messages
  */
  _status = Status::COMPLEX;
}

BracketNest *BracketNest::clone() const {
  /// make a deep copy of the Bracketnest
  BracketNest *result = new BracketNest( *this );
  for ( auto& it : result->parts ){
    it = it->clone();
  }
  return result;
}

BaseBracket *BracketNest::append( BaseBracket *t ){
  /// append a Bracket structure to this Nest
  parts.push_back( t );
  return this;
}

BracketNest::~BracketNest(){
  for ( auto const& it : parts ){
    delete it;
  }
}

string BaseBracket::put( bool full ) const {
  /// create a UTF8 string representation for this object
  string result = "[err?]";
  if ( full ){
    result += toString(cls);
  }
  return result;
}

string BaseBracket::pretty_put() const {
  /// create a descriptive UTF8 string representation for this object
  string result = "[err?]" + CLEX::get_tDescr(cls);
  return result;
}

string BracketLeaf::put( bool full ) const {
  /// create a UTF8 string representation for this object
  string result;
  if ( !morph.isEmpty() ){
    result += "[";
    result += TiCC::UnicodeToUTF8(morph);
    result += "]";
  }
  if ( full ){
    if ( orig.empty() ){
      string s = toString(cls);
      if ( s == "/" ){
	result += s + TiCC::UnicodeToUTF8(inflect);
      }
      else {
	result += s + "/" + TiCC::UnicodeToUTF8(inflect);
      }
    }
    else {
      result += orig;
    }
  }
  return result;
}

string BracketLeaf::pretty_put() const {
  /// create a descriptive UTF8 string representation for this object
  string result;
  if ( !morph.isEmpty() ){
    result += "[";
    result += TiCC::UnicodeToUTF8(morph);
    result += "]";
  }
  if ( glue ){
    size_t pos = orig.find_first_of( "^" );
    UnicodeString tag( orig[pos+1] );
    string utf_tag = TiCC::UnicodeToUTF8(tag);
    result += CLEX::get_tDescr(CLEX::toCLEX(utf_tag));
  }
  if ( status() != Status::PARTICIPLE
       && status() != Status::PARTICLE
       && status() != Status::DERIVATIONAL
       &&  status() != Status::FAILED
       && cls != CLEX::UNASS
       && cls != CLEX::NEUTRAL ){
    string s = CLEX::get_tDescr(cls);
    if ( s != "/" ){
      result += s;
    }
  }
  for ( int i=0; i < inflect.length(); ++i ){
    string id = CLEX::get_iDescr(inflect[i]);
    if ( !id.empty() ){
      result += "/" + id;
    }
  }
  return result;
}

string BracketNest::put( bool full ) const {
  /// create a UTF8 string representation for this object
  string result = "[ ";
  for ( auto const& it : parts ){
    string tmp = it->put( full );
    if ( !tmp.empty() ){
      result += tmp + " ";
    }
  }
  result += "]";
  if ( full ){
    if ( cls != CLEX::UNASS ){
      result += toString(cls);
    }
    if ( _compound != Compound::Type::NONE ){
      result += " " + toString(_compound) + "-compound";
    }
  }
  return result;
}

string BracketNest::pretty_put( ) const {
  /// create a descriptive UTF8 string representation for this object
  string result;
  int cnt = 0;
  for ( auto const& it : parts ){
    string tmp = it->pretty_put();
    if ( tmp[0] == '[' ){
      ++cnt;
    }
    result += tmp;
  }
  if ( cnt > 1 ){
    result = "[" + result + "]";
    if ( cls != CLEX::UNASS && cls != CLEX::NEUTRAL ){
      result += CLEX::get_tDescr(cls);
    }
  }
  return result;
}

ostream& operator<< ( ostream& os, const BaseBracket& c ){
  /// output a BaseBracket to a stream
  os << c.put();
  return os;
}

ostream& operator<< ( ostream& os, const BaseBracket *c ){
  /// output a BaseBracket to a stream
  if ( c ){
    os << c->put();
  }
  else {
    os << "[EMPTY]";
  }
  return os;
}

bool BracketNest::testMatch( list<BaseBracket*>& result,
			     const list<BaseBracket*>::iterator& rpos,
			     list<BaseBracket*>::iterator& bpos ){
  /// test if the rule matches at a certain position
  /*!
    \param result the current result. A new match will be appended
    \param rpos the position in the rules list we are at.
    \param bpos output parameter to return the END postion of the match
    \return true if it matches
  */
  if ( debugFlag > 5 ){
    LOG << "test MATCH: rpos= " << *rpos << endl;
  }
  bpos = result.end();
  size_t len = (*rpos)->RightHand.size();
  if ( len == 0 || len > result.size() ){
    if ( debugFlag > 5 ){
      LOG << "test MATCH FAIL (no RHS or RHS > result)" << endl;
    }
    return false;
  }
  size_t fpos = (*rpos)->infixpos();
  if ( debugFlag > 5 ){
    LOG << "test MATCH, fpos=" << fpos << " en len=" << len << endl;
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
      LOG << "test MATCH vergelijk " << *it << " met " << (*rpos)->RightHand[j] << endl;
    }
    if ( (*rpos)->RightHand[j] == CLEX::XAFFIX ){
      continue;
    }
    else if ( (*rpos)->RightHand[j] == CLEX::AFFIX ){
      continue;
    }
    else if ( (*rpos)->RightHand[j] != (*it)->tag() ){
      if ( debugFlag > 5 ){
	LOG << "test MATCH FAIL (" << (*rpos)->RightHand[j]
	    << " != " << (*it)->tag() << ")" << endl;
      }
      (*rpos)->set_status(Status::FAILED);
      bpos = it;
      return false;
    }
  }
  if ( j < len ){
    if ( debugFlag > 5 ){
      LOG << "test MATCH FAIL (j < len)" << endl;
    }
    bpos = result.end();
    return false;
  }
  if ( debugFlag > 5 ){
    LOG << "test MATCH OK" << endl;
  }
  return true;
}

Compound::Type construct( const vector<CLEX::Type>& tags ){
  /// construct a Compound::Type given a list of CLEX tags
  /*!
    \param tags a list of CLEX::Types
    \return a Compound::Type
  */
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
  /// construct a Compound::Type given two CLEX tags
  /*!
    \param tag1 a CLEX::Type
    \param tag2 a CLEX::Type
    \return a Compound::Type
  */
  vector<CLEX::Type> v;
  v.push_back( tag1 );
  v.push_back( tag2 );
  return construct( v );
}

Compound::Type BracketNest::getCompoundType(){
  /// extract the Compound::Type
  /*!
    This function uses a lot of heuristics to determine get Compound::Type
    given the elements in the BracketNest
  */
  if ( debugFlag > 5 ){
    LOG << "get compoundType: " << this << endl;
    LOG << "#parts: " << parts.size() << endl;
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
      LOG << "tag1 :" << tag1 << " stat1: " << st1 << " cp1: " << cp1 << endl;
      LOG << "tag2 :" << tag2 << " stat2: " << st2 << " cp2: " << cp2 << endl;
    }
    if ( st1 != Status::FAILED
	 && st2 != Status::FAILED
	 && st1 != Status::PARTICLE
	 && st1 != Status::PARTICIPLE ){
      switch ( tag1 ){
      case CLEX::N:
      case CLEX::A:
	if ( st1 == Status::DERIVATIONAL ){
	  compound = cp2;
	}
	else if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
	else if ( st2 == Status::DERIVATIONAL
		  || st2 == Status::INFO
		  || st2 == Status::INFLECTION ){
	  compound = cp1;
	}
	break;
      case  CLEX::B:
	if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
	break;
      case CLEX::P:
	if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
	else if ( tag2 == CLEX::NEUTRAL
		  || tag2 == CLEX::UNASS ){
	  compound = cp1;
	}
	break;
      case CLEX::V:
	if ( st1 == Status::DERIVATIONAL ){
	  compound = cp2;
	}
	else if ( st2 == Status::STEM ){
	  compound = construct( tag1, tag2 );
	}
	break;
      default:
	break;
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
      LOG << "tag1 :" << tag1 << " stat1: " << st1 << " cp1: " << cp1 << endl;
      LOG << "tag2 :" << tag2 << " stat2: " << st2 << " cp2: " << cp2 << endl;
      LOG << "tag3 :" << tag3 << " stat3: " << st3 << " cp3: " << cp3 << endl;
    }
    if ( st1 != Status::FAILED
	 && st2 != Status::FAILED
	 && st3 != Status::FAILED
	 && st1 != Status::PARTICLE
	 && st1 != Status::PARTICIPLE ){
      switch ( tag1 ){
      case CLEX::N:
	if ( ( st2 == Status::STEM || st2 == Status::COMPLEX ) && tag2 == CLEX::N
	     && (st3 == Status::STEM || st3 == Status::COMPLEX ) && tag3 == CLEX::N ){
	  compound = Compound::Type::NNN;
	}
	else if ( st1 != Status::DERIVATIONAL && st2 == Status::STEM &&
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
	  else if ( st2 == Status::INFLECTION &&
		    ( tag3 == CLEX::NEUTRAL || st3 == Status::INFLECTION ) ){
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
	break;
      case CLEX::A:
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
	break;
      case CLEX::P:
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
	break;
      case CLEX::B:
	if ( st1 == Status::STEM ){
	  if ( ( st2 == Status::STEM
		 && ( st3 == Status::INFLECTION || tag3 == CLEX::NEUTRAL ) ) ){
	    compound = construct( tag1, tag2 );
	  }
	  else if ( st2 == Status::COMPLEX ){
	    if ( tag2 == CLEX::N ){
	      compound = Compound::Type::BN;
	    }
	    else {
	      compound = cp2;
	    }
	  }
	}
	break;
      case CLEX::V:
      	if ( st2 == Status::STEM &&
	     ( st3 == Status::INFLECTION || tag3 == CLEX::NEUTRAL ) ){
	  compound = construct( tag1, tag2 );
	}
      	else if ( st3 == Status::STEM &&
		  ( st2 == Status::INFLECTION ) ){
	  compound = construct( tag1, tag3 );
	}
	break;
      default:
	break;
      }
    }
  }
  if ( debugFlag > 5 ){
    LOG << "   ASSIGNED :" << compound << endl;
  }
  _compound = compound;
  return compound;
}

folia::Morpheme *BracketLeaf::createMorpheme( folia::Document *doc ) const {
  /// use the data in the Leaf to create a folia::Morpheme node
  /*!
    \param doc The FoLiA Document context
  */
  string desc;
  int cnt = 0;
  return createMorpheme( doc, desc, cnt );
}

folia::Morpheme *BracketLeaf::createMorpheme( folia::Document *doc,
					      string& desc,
					      int& cnt ) const {
  /// use the data in the Leaf to create a folia::Morpheme node
  /*!
    \param doc The FoLiA Document context
    \param desc a decriptien note to add
    \param cnt a counter for the number of handled morphemes
  */
  folia::Morpheme *result = 0;
  desc.clear();
  size_t pos = orig.find_first_of( "^" );
  bool glue = ( pos != string::npos );
  if ( _status == Status::COMPLEX ){
    abort();
  }
  else if ( _status == Status::STEM
	    || ( _status == Status::DERIVATIONAL && glue ) ){
    string out = TiCC::UnicodeToUTF8(morph);
    if ( out.empty() ){
      throw logic_error( "stem has empty morpheme" );
    }
    folia::KWargs args;
    args["set"] = Mbma::mbma_tagset;
    args["class"] = "stem";
#pragma omp critical (foliaupdate)
    {
      result = new folia::Morpheme( args, doc );
      result->settext( out );
    }
    ++cnt;
    args.clear();
    args["set"] = Mbma::clex_tagset;
    if ( glue ){
      string tag;
      tag += orig[pos+1];
      args["class"] = tag;
      desc = "[" + out + "]" + CLEX::get_tDescr( CLEX::toCLEX(tag) ); // spread the word upwards!

    }
    else {
      args["class"] = toString( tag() );
      desc = "[" + out + "]" + CLEX::get_tDescr( tag() ); // spread the word upwards!
      folia::KWargs fargs;
      fargs["subset"] = "structure";
      if ( tag() == CLEX::SPEC
	   || tag() == CLEX::LET ){
	fargs["class"] = "[" + out + "]";
      }
      else {
	fargs["class"] = desc;
      }
#pragma omp critical (foliaupdate)
      {
	result->add_child<folia::Feature>( fargs );
      }
    }
#pragma omp critical (foliaupdate)
    {
      result->addPosAnnotation( args );
    }
  }
  else if ( _status == Status::PARTICLE ){
    string out = TiCC::UnicodeToUTF8(morph);
    if ( out.empty() ){
      throw logic_error( "particle has empty morpheme" );
    }
    folia::KWargs args;
    args["set"] = Mbma::mbma_tagset;
    args["class"] = "particle";
#pragma omp critical (foliaupdate)
    {
      result = new folia::Morpheme( args, doc );
      result->settext( out );
    }
    ++cnt;
    args.clear();
    args["set"] = Mbma::clex_tagset;
    args["class"] = toString( tag() );
#pragma omp critical (foliaupdate)
    {
      result->addPosAnnotation( args );
    }
    desc = "[" + out + "]"; // spread the word upwards! maybe add 'part' ??
  }
  else if ( _status == Status::INFLECTION ){
    string out = TiCC::UnicodeToUTF8(morph);
    if ( !out.empty() ){
      desc = "[" + out + "]";
    }
    folia::KWargs args;
    args["class"] = "inflection";
    args["set"] = Mbma::mbma_tagset;
#pragma omp critical (foliaupdate)
    {
      result = new folia::Morpheme( args, doc );
      if ( !out.empty() ){
	result->settext( out );
      }
    }
    ++cnt;
    args.clear();
    args["subset"] = "inflection";
    for ( int i=0; i < inflect.length(); ++i ){
      UChar inf = inflect[i];
      if ( inf != '/' ){
	string d = CLEX::get_iDescr(inf);
	if ( !d.empty() ){
	  // happens sometimes when there is fawlty data
	  args["class"] = d;
	  desc += "/" + d;
#pragma omp critical (foliaupdate)
	  {
	    result->add_child<folia::Feature>( args );
	  }
	}
      }
    }
  }
  else if ( _status == Status::DERIVATIONAL
	    || _status == Status::PARTICIPLE
	    || _status == Status::FAILED ){
    string out = TiCC::UnicodeToUTF8(morph);
    if ( out.empty() ){
      throw logic_error( "Derivation with empty morpheme" );
    }
    folia::KWargs args;
    if ( _status == Status::DERIVATIONAL ){
      args["class"] = "affix";
    }
    else if ( _status == Status::PARTICIPLE ){
      args["class"] = "participle";
    }
    else {
      args["class"] = "derivational";
    }
    args["set"] = Mbma::mbma_tagset;
#pragma omp critical (foliaupdate)
    {
      result = new folia::Morpheme( args, doc );
      result->settext( out );
    }
    ++cnt;
    desc = "[" + out + "]"; // pass it up!
    for ( int i=0; i < inflect.length(); ++i ){
      UChar inf = inflect[i];
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
#pragma omp critical (foliaupdate)
    {
      result->add_child<folia::Feature>( args );
    }
  }
  else if ( _status == Status::INFO ){
    folia::KWargs args;
    args["class"] = "inflection";
    args["set"] = Mbma::mbma_tagset;
#pragma omp critical (foliaupdate)
    {
      result = new folia::Morpheme( args, doc );
    }
    args.clear();
    args["subset"] = "inflection";
    for ( int i=0; i < inflect.length(); ++i ){
      UChar inf = inflect[i];
      if ( inf != '/' ){
	string d = CLEX::get_iDescr( inf );
	if ( !d.empty() ){
	  // happens sometimes when there is fawlty data
	  desc += "/" + d;
	  args["class"] = d;
#pragma omp critical (foliaupdate)
	  {
	    result->add_child<folia::Feature>( args );
	  }
	}
      }
    }
  }
  return result;
}

folia::Morpheme *BracketNest::createMorpheme( folia::Document *doc ) const {
  /// use the data in the Leaf to create a folia::Morpheme node
  /*!
    \param doc The FoLiA Document context
  */
  string desc;
  int cnt = 0;
  return createMorpheme( doc, desc, cnt );
}

folia::Morpheme *BracketNest::createMorpheme( folia::Document *doc,
					      string& desc,
					      int& cnt ) const {
  /// use the data in the Leaf to create a folia::Morpheme node
  /*!
    \param doc The FoLiA Document context
    \param desc a decriptive note to add
    \param cnt a counter for the number of handled morphemes
  */
  folia::Morpheme *result = 0;
  folia::KWargs args;
  args["class"] = "complex";
  args["set"] = Mbma::mbma_tagset;
#pragma omp critical (foliaupdate)
  {
    result = new folia::Morpheme( args, doc );
  }
  cnt = 0;
  desc.clear();
  vector<folia::Morpheme*> stack;
  for ( auto const& it : parts ){
    string deeper_desc;
    int deep_cnt = 0;
    folia::Morpheme *m = it->createMorpheme( doc,
					     deeper_desc,
					     deep_cnt );
    if ( it->status() == Status::DERIVATIONAL
	 || it->status() == Status::PARTICIPLE ){
      if ( !it->original().empty() ){
	args.clear();
	args["subset"] = "applied_rule";
	args["class"] = it->original();
#pragma omp critical (foliaupdate)
	{
	  result->add_child<folia::Feature>( args );
	}
      }
    }
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
  if ( desc.empty() ){
    desc = "XYZ";
  }
  args["class"]  = desc;
#pragma omp critical (foliaupdate)
  {
    result->add_child<folia::Feature>( args );
  }
  args.clear();
  args["set"] = Mbma::clex_tagset;
  args["class"] = toString( tag() );
  folia::PosAnnotation *pos = 0;
#pragma omp critical (foliaupdate)
  {
    pos = result->addPosAnnotation( args );
  }
  Compound::Type ct = compound();
  if ( ct != Compound::Type::NONE ){
    args.clear();
    args["subset"] = "compound";
    args["class"]  = toString(ct);
#pragma omp critical (foliaupdate)
    {
      pos->add_child<folia::Feature>( args );
    }
  }
#pragma omp critical (foliaupdate)
  for ( const auto& s : stack ){
    result->append( s );
  }
  return result;
}

list<BaseBracket*>::iterator BracketNest::resolveAffix( list<BaseBracket*>& result,
							const list<BaseBracket*>::iterator& rpos ){
  /// try to resolve an Affix rule
  /*!
    \param result the output, matches might replace part of it by a new Nest
    \param rpos start posotion for this search
    \return an iterator where the next resolving step should start
  */
  if ( debugFlag > 5 ){
    LOG << "resolve affix" << endl;
  }
  list<BaseBracket*>::iterator bit;
  bool matched = testMatch( result, rpos, bit );
  if ( matched ){
    if ( debugFlag > 5 ){
      LOG << "OK een match" << endl;
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
	= new BracketNest( (*rpos)->tag(), Compound::Type::NONE, debugFlag, myLog );
      for ( size_t j = 0; j < len; ++j ){
	tmp->append( *it );
	if ( debugFlag > 5 ){
	  LOG << "erase " << *it << endl;
	}
	it = result.erase(it);
      }
      if ( debugFlag > 5 ){
	LOG << "new node:" << tmp << endl;
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
  /// check for adjacent Nouns and replace by a new Nest
  if ( debugFlag > 5 ){
    LOG << "resolve NOUNS in:" << this << endl;
  }
  list<BaseBracket*>::iterator it = parts.begin();
  list<BaseBracket*>::iterator prev = it++;
  while ( it != parts.end() ){
    if ( (*prev)->tag() == CLEX::N && (*prev)->RightHand.size() == 0
	 && ( (*it)->tag() == CLEX::N && (*it)->status() == Status::STEM )
	 && (*it)->RightHand.size() == 0 ){
      Compound::Type newt = Compound::Type::NN;
      if ( (*prev)->compound() == Compound::Type::NN ){
	newt = Compound::Type::NNN;
      }
      BaseBracket *tmp = new BracketNest( CLEX::N, newt, debugFlag, myLog );
      tmp->append( *prev );
      tmp->append( *it );
      if ( debugFlag > 5 ){
	LOG << "current result:" << parts << endl;
	LOG << "new node:" << tmp << endl;
	LOG << "erase " << *prev << endl;
      }
      prev = parts.erase(prev);
      if ( debugFlag > 5 ){
	LOG << "erase " << *prev << endl;
      }
      prev = parts.erase(prev);
      prev = parts.insert( prev, tmp );
      if ( debugFlag > 5 ){
	LOG << "current result:" << parts << endl;
      }
      it = prev;
      ++it;
    }
    else {
      prev = it++;
    }
  }
  if ( debugFlag > 5 ){
    LOG << "resolve NOUNS result:" << this << endl;
  }
}

list<BaseBracket*>::iterator BracketNest::glue( list<BaseBracket*>& result,
						const list<BaseBracket*>::iterator& rpos ){
  /// apply a glue rule
  if ( debugFlag > 5 ){
    LOG << "glue " << endl;
    LOG << "result IN : " << result << endl;
    LOG << "rpos= " << *rpos << endl;
  }
  size_t len = (*rpos)->RightHand.size();
  bool matched = true;
  vector<CLEX::Type> match_tags;
  if ( len == 0 || len > result.size() ){
    if ( debugFlag > 5 ){
      LOG << "test MATCH FAIL (no RHS or RHS > result)" << endl;
    }
    matched = false;
  }
  else {
    size_t j = 0;
    list<BaseBracket*>::iterator it = rpos;
    for (; j < len && it != result.end(); ++j, ++it ){
      if ( debugFlag > 5 ){
	LOG << "test MATCH vergelijk " << (*it)->tag() << " met " << (*rpos)->RightHand[j] << endl;
      }
      if ( (*rpos)->RightHand[j] == CLEX::GLUE ){
	++j;
	match_tags.push_back( (*rpos)->RightHand[j] );
	continue; // the ^ is always OK
      }
      if ( (*rpos)->RightHand[j] != (*it)->tag() ){
	if ( debugFlag > 5 ){
	  LOG << "test MATCH FAIL (" << (*it)->tag()
	       << " != " << (*rpos)->RightHand[j] << ")" << endl;
	}
	(*rpos)->set_status(Status::FAILED);
	matched = false;
      }
      match_tags.push_back( (*rpos)->RightHand[j] );
    }
  }
  if ( matched ){
    list<BaseBracket*>::iterator bit = rpos;
    if ( debugFlag > 5 ){
      LOG << "OK een match" << endl;
    }
    list<BaseBracket*>::iterator it = bit--;
    BracketNest *tmp
      = new BracketNest( (*rpos)->tag(), Compound::Type::NONE, debugFlag, myLog );
    for ( size_t j = 0; j < len-1; ++j ){
      tmp->append( *it );
      if ( debugFlag > 5 ){
	LOG << "erase " << *it << endl;
      }
      it = result.erase(it);
    }
    if ( debugFlag > 5 ){
      LOG << "new node:" << tmp << endl;
      LOG << "match_tags = " << match_tags << endl;
    }
    tmp->_compound = construct( match_tags );
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
  /// resolve all glue rules
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // search for glue rules
    if ( debugFlag > 5 ){
      LOG << "search glue: bekijk: " << *it << endl;
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
  /// resolve rules starting with *
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // search for rules with a * at the begin
    if ( debugFlag > 5 ){
      LOG << "search leading *: bekijk: " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	LOG << "nested! " << endl;
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
  /// resolve rules ending with *
  list<BaseBracket *>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // search for rules with a * at the end
    if ( debugFlag > 5 ){
      LOG << "search trailing *: bekijk: " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	LOG << "nested! " << endl;
      }
      (*it)->resolveTail();
      ++it;
    }
    else {
      size_t len = (*it)->RightHand.size();
      if ( (*it)->infixpos() > 0
	   && (*it)->infixpos() == signed(len)-1 ){
	if ( debugFlag > 5 ){
	  LOG << "found trailing * " << *it << endl;
	  LOG << "infixpos=" << (*it)->infixpos() << endl;
	  LOG << "len=" << len << endl;
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
  /// resolve rules with a * NOT at begin or end
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    // now search for other rules with a * in the middle
    if ( debugFlag > 5 ){
      LOG << "hoofd infix loop bekijk: " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	LOG << "nested! " << endl;
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
  /// remove all nodes that don't have a morpheme or an inflection
  if ( debugFlag > 5 ){
    LOG << "clear emptyNodes: " << this << endl;
  }
  list<BaseBracket*> out;
  list<BaseBracket*>::iterator it = parts.begin();
  while ( it != parts.end() ){
    if ( debugFlag > 5 ){
      LOG << "loop clear emptyNodes : " << *it << endl;
    }
    if ( (*it)->isNested() ){
      if ( debugFlag > 5 ){
	LOG << "nested! " << endl;
      }
      (*it)->clearEmptyNodes( );
      out.push_back( *it );
    }
    else {
      if ( (*it)->morpheme().isEmpty() &&
	   (*it)->inflection().isEmpty() ){
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
    LOG << "RESULT clear emptyNodes: " << this << endl;
  }
}

CLEX::Type BracketNest::getFinalTag() {
  /// get the result tag for this rule
  // It is the last tag in the list, except for 'P' tags
  //
  // LOG << "get Final Tag from: " << this << endl;
  CLEX::Type result_cls = CLEX::UNASS;
  auto it = parts.rbegin();
  while ( it != parts.rend() ){
    //    LOG << "bekijk: " << *it << endl;
    if ( (*it)->isNested()
	 || ( (*it)->inflection().isEmpty()
	      && !(*it)->morpheme().isEmpty() ) ){
      result_cls = (*it)->tag();
      //      LOG << "maybe tag = " << result_cls << endl;
      if ( result_cls != CLEX::P ){
	// in case of P we hope for better to the left
	auto it2 = it;
	++it2;
	if ( it2 != parts.rend() ){
	  //	  LOG << "bekijk ook " << *it2 << endl;
	  if ( (*it2)->infixpos() == 0 ){
	    result_cls = (*it2)->tag();
	    // in case of a X_*Y rule we need X
	  }
	}
	break;
      }
    }
    ++it;
  }
  //  LOG << "final tag = " << result_cls << endl;
  cls = result_cls;
  return cls;
}
