/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2024
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
  class MorphologyLayer;
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
  Status status() const { return _status; };
  void set_status( const Status s ) { _status = s; };
  virtual icu::UnicodeString morpheme() const { return "";};
  virtual icu::UnicodeString inflection() const { return ""; };
  virtual icu::UnicodeString original() const { return ""; };
  virtual int infixpos() const { return -1; };
  virtual bool isglue() const { return false; };
  virtual icu::UnicodeString put( bool = false ) const;
  virtual BaseBracket *append( BaseBracket * ){ abort(); };
  virtual bool isNested() const { return false; };
  virtual void resolveGlue(){ abort(); };
  virtual void resolveLead(){ abort(); };
  virtual void resolveTail(){ abort(); };
  virtual void resolveMiddle(){ abort(); };
  virtual folia::Morpheme *createMorpheme( folia::Document *,
					   const std::string& ) const = 0;
  virtual folia::Morpheme *createMorpheme( folia::Document *,
					   const std::string&,
					   icu::UnicodeString&,
					   int& ) const = 0;
  virtual Compound::Type compound() const { return Compound::Type::NONE; };
  virtual Compound::Type speculateCompoundType() { return compound(); };
  virtual void display_parts( std::ostream&, int=0 ) const { return; };
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
  ~BracketLeaf() override;
  icu::UnicodeString put( bool = false ) const override;
  icu::UnicodeString morpheme() const override {
    /// return the value of the morpheme
    return _morph;
  };
  icu::UnicodeString inflection() const override {
    /// return the value of the inflexion (if any)
    return _inflect;
  };
  icu::UnicodeString original() const override {
    /// return the original value before processing started
    return _orig;
  };
  int infixpos() const override {
    /// return the position of an infix
    return _ifpos;
  };
  bool isglue() const override {
    /// return tre if this is a glue tag
    return _glue;
  };
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string&) const override;
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string&,
				   icu::UnicodeString&,
				   int& ) const override;
private:
  int _ifpos;
  bool _glue;
  icu::UnicodeString _orig;
  icu::UnicodeString _morph;
  icu::UnicodeString _inflect;
};

/// \brief a specialization of BaseBracket to store intermediate nodes
///
/// provides functions to test and resolve rules
class BracketNest: public BaseBracket {
 public:
  BracketNest( CLEX::Type, Compound::Type, int, TiCC::LogStream& );
  BaseBracket *append( BaseBracket * ) override ;
  ~BracketNest() override;
  bool isNested() const override { return true; };
  icu::UnicodeString put( bool = false ) const override;
  bool testMatch( const std::list<BaseBracket*>& result,
		  const std::list<BaseBracket*>::const_iterator& rpos,
		  std::list<BaseBracket*>::const_iterator& bpos ) const;
  std::list<BaseBracket*>::iterator glue( std::list<BaseBracket*>&,
					  const std::list<BaseBracket*>::iterator& ) const;
  std::list<BaseBracket*>::const_iterator resolveAffix( std::list<BaseBracket*>&,
							const std::list<BaseBracket*>::const_iterator& );
  void resolveGlue() override;
  void resolveLead() override;
  void resolveTail() override;
  void resolveMiddle() override;
  Compound::Type speculateCompoundType() override;
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string& ) const override;
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string&,
				   icu::UnicodeString&,
				   int& ) const override;
  void resolveNouns();
  CLEX::Type getFinalTag();
  Compound::Type compound() const override { return _compound; };
  void display_parts( std::ostream&, int=0 ) const override;
 private:
  std::list<BaseBracket *> _parts;
  Compound::Type _compound;
};

std::string toString( const Compound::Type& );
std::ostream& operator<<( std::ostream&, const Status& );
std::ostream& operator<<( std::ostream&, const Compound::Type& );
std::ostream& operator<<( std::ostream&, const BaseBracket& );
std::ostream& operator<<( std::ostream&, const BaseBracket * );

#endif // MBMA_BRACKETS_H
