/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
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

enum Status { INFO, PARTICLE, PARTICIPLE, STEM, COMPLEX, INFLECTION,
	      DERIVATIONAL, FAILED };

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
  virtual std::string inflection() const { return ""; };
  virtual std::string original() const { return ""; };
  virtual int infixpos() const { return -1; };
  virtual bool isglue() const { return false; };
  virtual icu::UnicodeString put( bool = true ) const;
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

class BracketLeaf: public BaseBracket {
public:
  BracketLeaf( const RulePart&, int, TiCC::LogStream& );
  BracketLeaf( CLEX::Type, const icu::UnicodeString&, int, TiCC::LogStream& );
  icu::UnicodeString put( bool = true ) const;
  icu::UnicodeString morpheme() const { return morph; };
  std::string inflection() const { return inflect; };
  std::string original() const { return orig; };
  int infixpos() const { return ifpos; };
  bool isglue() const { return glue; };
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

class BracketNest: public BaseBracket {
 public:
  BracketNest( CLEX::Type, Compound::Type, int, TiCC::LogStream& );
  BaseBracket *append( BaseBracket * );
  ~BracketNest();
  bool isNested() { return true; };
  void clearEmptyNodes();
  icu::UnicodeString put( bool = true ) const;
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
