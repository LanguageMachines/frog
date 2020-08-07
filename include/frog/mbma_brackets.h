/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2020
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

#ifndef MBMA_BRACKETS_H
#define MBMA_BRACKETS_H

#include <vector>
#include <list>
#include "unicode/unistr.h"
#include "ticcutils/LogStream.h"
#include "frog/clex.h"

/// The state of the MBMA structure
enum Status {
  INFO,          ///< The structure contains additional Information
  PARTICLE,      ///< The structure describes a Particle
  PARTICIPLE,    ///< The structure describes a Participle
  STEM,          ///< The structure describes the Stem
  COMPLEX,       ///< The structure describes a Complex Rule
  INFLECTION,    ///< The structure describes an Inflection Rule
  DERIVATIONAL,  ///< The structure describes a Drivational rule
  FAILED         ///< The structure describes a Failed state
};

/// a range off all 'possible' compound types. Not all of these ae actively
/// assigned
namespace Compound {
  enum Type : int {
    // NB and PB compounds don't exist
    NONE,
      NN, NA, NB, NP, NV,
      AN, AA, AB, AP, AV,
      BN, BA, BB, BP, BV,
      PN, PA, PB, PP, PV,
      VN, VA, VB, VP, VV,
      NNN, NVN };
}

namespace folia {
  class Document;
  class Morpheme;
}

class RulePart;

/// \brief a base class for storing bracketted MBMA rules
class BaseBracket {
 public:
 BaseBracket( CLEX::Type t, const std::vector<CLEX::Type>& R, int flag,
	      TiCC::LogStream& l ):
  RightHand(R),
    cls(t),
    _status( FAILED ),
    debugFlag(flag),
    myLog(l)
    {};
 BaseBracket( CLEX::Type t, int flag, TiCC::LogStream& l ):
  cls(t),
    _status( FAILED ),
    debugFlag(flag),
    myLog(l)
    {};
  virtual ~BaseBracket() {};
  virtual BaseBracket *clone() const = 0;
  Status status() const { return _status; };
  void set_status( const Status s ) { _status = s; };
  virtual icu::UnicodeString morpheme() const { return "";};
  virtual std::string inflection() const { return ""; };
  virtual std::string original() const { return ""; };
  virtual int infixpos() const { return -1; };
  virtual bool isglue() const { return false; };
  virtual icu::UnicodeString put( bool = true ) const;
  virtual icu::UnicodeString pretty_put() const;
  virtual BaseBracket *append( BaseBracket * ){ abort(); };
  virtual bool isNested() { return false; };
  virtual void resolveGlue(){ abort(); };
  virtual void resolveLead(){ abort(); };
  virtual void resolveTail(){ abort(); };
  virtual void resolveMiddle(){ abort(); };
  virtual void clearEmptyNodes() { abort(); };
  virtual folia::Morpheme *createMorpheme( folia::Document *  ) const = 0;
  virtual folia::Morpheme *createMorpheme( folia::Document *,
					   std::string&, int& ) const = 0;
  virtual Compound::Type compound() const { return Compound::Type::NONE; };
  virtual Compound::Type getCompoundType() { return compound(); };
  CLEX::Type tag() const { return cls; };
  void setTag( CLEX::Type t ) { cls = t; };
  std::vector<CLEX::Type> RightHand;
 protected:
  CLEX::Type cls;
  Status _status;
  int debugFlag;
  TiCC::LogStream& myLog;
};

/// \brief a specialization of BaseBracket to store endnodes (morphemes and
/// inflection information
class BracketLeaf: public BaseBracket {
public:
  BracketLeaf( const RulePart&, int, TiCC::LogStream& );
  BracketLeaf( CLEX::Type, const icu::UnicodeString&, int, TiCC::LogStream& );
  BracketLeaf *clone() const;
  icu::UnicodeString put( bool = true ) const;
  icu::UnicodeString pretty_put() const;
  icu::UnicodeString morpheme() const {
    /// return the value of the morpheme
    return morph;
  };
  std::string inflection() const {
    /// return the value of the inflexion (if any)
    return inflect;
  };
  std::string original() const {
    /// return the original value befor processing
    return orig;
  };
  int infixpos() const {
    /// return the position of an infix
    return ifpos;
  };
  bool isglue() const {
    /// return tre if this is a glue tag
    return glue;
  };
  folia::Morpheme *createMorpheme( folia::Document * ) const;
  folia::Morpheme *createMorpheme( folia::Document *,
				   std::string&, int& ) const;
private:
  int ifpos;
  bool glue;
  icu::UnicodeString morph;
  std::string orig;
  std::string inflect;
};

/// \brief a specialization of BaseBracket to store intermediate nodes
///
/// provides functions to test and resolve rules
class BracketNest: public BaseBracket {
 public:
  BracketNest( CLEX::Type, Compound::Type, int, TiCC::LogStream& );
  BaseBracket *append( BaseBracket * );
  BracketNest *clone() const;
  ~BracketNest();
  bool isNested() { return true; };
  void clearEmptyNodes();
  icu::UnicodeString put( bool = true ) const;
  icu::UnicodeString pretty_put() const;
  bool testMatch( std::list<BaseBracket*>& result,
		  const std::list<BaseBracket*>::iterator& rpos,
		  std::list<BaseBracket*>::iterator& bpos );
  std::list<BaseBracket*>::iterator glue( std::list<BaseBracket*>&,
					  const std::list<BaseBracket*>::iterator& );
  std::list<BaseBracket*>::iterator resolveAffix( std::list<BaseBracket*>&,
						  const std::list<BaseBracket*>::iterator& );
  void resolveGlue();
  void resolveNouns();
  void resolveLead();
  void resolveTail();
  void resolveMiddle();
  Compound::Type getCompoundType();
  CLEX::Type getFinalTag();
  folia::Morpheme *createMorpheme( folia::Document * ) const;
  folia::Morpheme *createMorpheme( folia::Document *,
				   std::string&, int& ) const;
  std::list<BaseBracket *> parts;
  Compound::Type compound() const { return _compound; };
 private:
  Compound::Type _compound;
};

std::string toString( const Compound::Type& );
std::ostream& operator<<( std::ostream&, const Status& );
std::ostream& operator<<( std::ostream&, const Compound::Type& );
std::ostream& operator<<( std::ostream&, const BaseBracket& );
std::ostream& operator<<( std::ostream&, const BaseBracket * );

#endif // MBMA_BRACKETS_H
