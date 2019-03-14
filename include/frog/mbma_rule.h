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

#ifndef MBMA_RULE_H
#define MBMA_RULE_H

#include <map>
#include <vector>
#include <string>
#include "unicode/unistr.h"
#include "ticcutils/LogStream.h"
#include "frog/clex.h"

namespace Compound {
  enum Type : int;
}

class RulePart {
public:
  RulePart( const std::string&, const UChar, bool );
  bool isBasic() const;
  void get_edits( const std::string& );
  CLEX::Type ResultClass;
  std::vector<CLEX::Type> RightHand;
  icu::UnicodeString ins;
  icu::UnicodeString del;
  icu::UnicodeString hide;
  icu::UnicodeString uchar;
  icu::UnicodeString morpheme;
  std::string inflect;
  bool is_affix;
  bool is_glue;
  bool is_participle;
};

std::ostream& operator<<( std::ostream& os, const RulePart&  );
std::ostream& operator<<( std::ostream& os, const RulePart * );

class BracketNest;

class Rule {
public:
  Rule( const std::vector<std::string>&,
	const icu::UnicodeString&,
	TiCC::LogStream&,
	TiCC::LogStream&,
	int );
  ~Rule();
  std::vector<std::string> extract_morphemes() const;
  std::string morpheme_string( bool = false ) const;
  std::string pretty_string() const;
  icu::UnicodeString getKey( bool );
  bool performEdits();
  void getCleanInflect();
  void reduceZeroNodes();
  void resolveBrackets( bool );
  void resolve_inflections();
  std::vector<RulePart> rules;
  int debugFlag;
  CLEX::Type tag;
  icu::UnicodeString sortkey;
  icu::UnicodeString orig_word;
  std::string description;
  std::string inflection;
  Compound::Type compound;
  BracketNest *brackets;
  TiCC::LogStream& myLog;
  TiCC::LogStream& dbgLog;
  double confidence;
};

std::ostream& operator<<( std::ostream& os, const Rule& );
std::ostream& operator<<( std::ostream& os, const Rule * );

#endif // MBMA_RULE_H
