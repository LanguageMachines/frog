/*
  $Id: mbma_mod.cxx 18070 2015-02-18 15:05:05Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/Frog/trunk/src/mbma_mod.cxx $

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

#ifndef MBMA_RULE_H
#define MBMA_RULE_H

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

std::ostream& operator<<( std::ostream& os, const RulePart&  );
std::ostream& operator<<( std::ostream& os, const RulePart * );

class BracketNest;

class Rule {
public:
  Rule( const std::vector<std::string>&, const UnicodeString&, int flag );
  std::vector<std::string> extract_morphemes() const;
  std::string getCleanInflect() const;
  void reduceZeroNodes();
  BracketNest *resolveBrackets( bool, CLEX::Type& );
  std::vector<RulePart> rules;
  int debugFlag;
};

std::ostream& operator<<( std::ostream& os, const Rule& );
std::ostream& operator<<( std::ostream& os, const Rule * );

#endif // MBMA_RULE_H
