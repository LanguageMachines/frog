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

#ifndef CLEX_H
#define CLEX_H

#include <iostream>
#include <string>
#include <map>

namespace CLEX {
  /// all possible CELEX tags and action properties
  enum Type {
    UNASS,  ///< unknow value
    A,      ///< Adjective (CELEX tag)
    B,      ///< Adverb (CELEX tag)
    C,      ///< Conjunction (CELEX tag)
    D,      ///< Article (CELEX tag)
    I,      ///< Interjection (CELEX tag)
    N,      ///< Noun (CELEX tag)
    O,      ///< Pronoun (CELEX tag)
    P,      ///< Preposition (CELEX tag)
    Q,      ///< Numeral (CELEX tag)
    V,      ///< Verb (CELEX tag)
    LET,    ///< Letter (not in CELEX)
    PN,     ///< Proper Noun (not in CELEX)
    SPEC,   ///< Special (not in CELEX)
    X,      ///< Unanalysed
    Z,      ///< Expresssion-part
    AFFIX,  ///< affix property
    XAFFIX, ///< x-affix property
    GLUE,   ///< Glue property
    NEUTRAL ///< No action
  };
  bool is_CELEX_base( const Type& );
  Type select_tag( const char ch );
  std::string toString( const Type& );
  icu::UnicodeString toUnicodeString( const Type& );
  Type toCLEX( const icu::UnicodeString& );
  Type toCLEX( const UChar );
  const icu::UnicodeString& get_inflect_descr( char c );
  const icu::UnicodeString& get_tag_descr( CLEX::Type t );
}

std::ostream& operator<<( std::ostream&, const CLEX::Type& );

#endif // CLEX_H
