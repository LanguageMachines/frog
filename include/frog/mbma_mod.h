/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2014
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

#ifndef MBMA_MOD_H
#define MBMA_MOD_H

#include <unicode/translit.h>

class MBMAana;

namespace CLEX {
  enum Type { UNASS, N, A, Q, V, D, O, B, P, Y, I, X, Z, PN, AFFIX, XAFFIX,
	      NEUTRAL };
}

class RulePart {
public:
  RulePart( const std::string&, const UChar );
  bool isBasic() const;
  void get_ins_del( const std::string& );
  CLEX::Type ResultClass;
  std::vector<CLEX::Type> RightHand;
  UnicodeString ins;
  UnicodeString del;
  UnicodeString uchar;
  UnicodeString morpheme;
  std::string inflect;
  int fixpos;
  int xfixpos;
  bool participle;
};


class Rule {
public:
  Rule( const std::vector<std::string>&, const UnicodeString& );
  std::vector<RulePart> rules;
};


class Mbma {
 public:
  Mbma();
  ~Mbma();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& doc ) const;
  void Classify( folia::Word * );
  void Classify( const UnicodeString& );
  void filterTag( const std::string&, const std::vector<std::string>& );
  std::vector<std::vector<std::string> > getResult() const;
 private:
  void cleanUp();
  bool readsettings( const std::string&, const std::string& );
  void fillMaps();
  void init_cgn( const std::string& );
  Transliterator * init_trans();
  UnicodeString filterDiacritics( const UnicodeString& ) const;
  void getFoLiAResult( folia::Word *, const UnicodeString& ) const;
  std::vector<std::string> make_instances( const UnicodeString& word );
  void Step1( Rule& );
  void resolve_inflections( Rule& );
  MBMAana addInflect( Rule& rule,
		      const std::string&,
		      const std::vector<std::string>& );
  MBMAana inflectAndAffix( Rule& );
  void execute( const UnicodeString& word,
		const std::vector<std::string>& classes );
  int debugFlag;
  void addMorph( folia::MorphologyLayer *,
		 const std::vector<std::string>& ) const;
  void addMorph( folia::Word *, const std::vector<std::string>& ) const;
  void addAltMorph( folia::Word *, const std::vector<std::string>& ) const;
  std::string MTreeFilename;
  Timbl::TimblAPI *MTree;
  std::map<char,std::string> iNames;
  std::map<std::string,std::string> tagNames;
  std::map<std::string,std::string> TAGconv;
  std::vector<MBMAana> analysis;
  std::string version;
  std::string tagset;
  std::string cgn_tagset;
  TiCC::LogStream *mbmaLog;
  Transliterator *transliterator;
  Tokenizer::UnicodeFilter *filter;
};

class MBMAana {
  friend std::ostream& operator<< ( std::ostream& , const MBMAana& );
  friend std::ostream& operator<< ( std::ostream& , const MBMAana* );
  public:
  MBMAana() {
    tag = "";
    infl = "";
    description = "";
  };
 MBMAana( const std::string& _tag,
	  const std::string& _infl,
	  const std::vector<std::string>& _mo,
	  const std::string& _des ):
  tag(_tag),infl(_infl),morphemes(_mo), description(_des) {};

  ~MBMAana() {};

  std::string getTag() const {
    return tag;
  };

  std::string getInflection() const {
    return infl;
  };

  const std::vector<std::string>& getMorph() const {
    return morphemes;
  };

 private:
  std::string tag;
  std::string infl;
  std::vector<std::string> morphemes;
  std::string description;
};

#endif
