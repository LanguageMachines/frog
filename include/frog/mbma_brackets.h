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

#ifndef MBMA_BRACKETS_H
#define MBMA_BRACKETS_H

enum class Status { INFO, STEM, COMPLEX, INFLECTION, DERIVATIONAL };

enum class CompoundType { NONE, NN, PN, PV };

namespace folia {
  class Document;
  class Morpheme;
}

class RulePart;

class BaseBracket {
public:
 BaseBracket( CLEX::Type t, const std::vector<CLEX::Type>& R, int flag ):
  RightHand(R),
    cls(t),
    debugFlag(flag)

   {};
 BaseBracket( CLEX::Type t, int flag ):
  cls(t), debugFlag(flag)
  {};
  virtual ~BaseBracket() {};

  Status status() const { return _status; };
  virtual UnicodeString morpheme() const { return "";};
  virtual UnicodeString deepmorphemes() const { return "";};
  virtual std::string inflection() const { return ""; };
  virtual std::string original() const { return ""; };
  virtual int infixpos() const { return -1; };
  virtual UnicodeString put( bool = false ) const;
  virtual BaseBracket *append( BaseBracket * ){ abort(); };
  virtual bool isNested() { return false; };
  virtual void resolveLead(){ abort(); };
  virtual void resolveTail(){ abort(); };
  virtual void resolveMiddle(){ abort(); };
  virtual folia::Morpheme *createMorpheme( folia::Document *,
					   const std::string&,
					   const std::string& ) const = 0;
  virtual folia::Morpheme *createMorpheme( folia::Document *,
					   const std::string&,
					   const std::string&,
					   std::string&, int& ) const = 0;
  virtual CompoundType compound() const { return CompoundType::NONE; };
  virtual void setCompoundType() { return; };
  CLEX::Type tag() const { return cls; };
  void setTag( CLEX::Type t ) { cls = t; };
  std::vector<CLEX::Type> RightHand;
 protected:
  CLEX::Type cls;
  Status _status;
  int debugFlag;
};

class BracketLeaf: public BaseBracket {
public:
  BracketLeaf( const RulePart&, int );
  BracketLeaf( CLEX::Type, const UnicodeString&, int );
  UnicodeString put( bool = false ) const;
  UnicodeString morpheme() const { return morph; };
  UnicodeString deepmorphemes() const { return morph; };
  std::string inflection() const { return inflect; };
  std::string original() const { return orig; };
  int infixpos() const { return ifpos; };
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string&,
				   const std::string& ) const;
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string&,
				   const std::string&,
				   std::string&, int& ) const;
private:
  int ifpos;
  UnicodeString morph;
  std::string orig;
  std::string inflect;
};

class BracketNest: public BaseBracket {
 public:
  BracketNest( CLEX::Type, CompoundType, int );
  BaseBracket *append( BaseBracket * );
  ~BracketNest();
  bool isNested() { return true; };
  UnicodeString put( bool = false ) const;
  bool testMatch( std::list<BaseBracket*>& result,
		  const std::list<BaseBracket*>::iterator& rpos,
		  std::list<BaseBracket*>::iterator& bpos );
  std::list<BaseBracket*>::iterator resolveAffix( std::list<BaseBracket*>&,
						  const std::list<BaseBracket*>::iterator& );
  void resolveNouns();
  void resolveLead();
  void resolveTail();
  void resolveMiddle();
  void setCompoundType();
  UnicodeString deepmorphemes() const;
  CLEX::Type getFinalTag();
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string&,
				   const std::string& ) const;
  folia::Morpheme *createMorpheme( folia::Document *,
				   const std::string&,
				   const std::string&,
				   std::string&, int& ) const;
  std::list<BaseBracket *> parts;
  CompoundType compound() const { return _compound; };
 private:
  CompoundType _compound;
};

std::string toString( const CompoundType& );
std::ostream& operator<<( std::ostream&, const CompoundType );
std::ostream& operator<<( std::ostream&, const BaseBracket& );
std::ostream& operator<<( std::ostream&, const BaseBracket * );

#endif // MBMA_BRACKETS_H