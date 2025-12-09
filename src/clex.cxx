/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2026
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

#include "ticcutils/Unicode.h"

#include "frog/clex.h"
#include <string>
#include <map>


using namespace std;
using namespace icu;

namespace CLEX {

  /// a harcoded table with a mapping for CLEX::Type to a description
  const map<CLEX::Type,UnicodeString> tagNames = {
    {CLEX::A, "adjective"},
    {CLEX::B, "adverb"},
    {CLEX::C, "conjunction"},
    {CLEX::D, "article"},
    {CLEX::I, "interjection"},
    {CLEX::N, "noun" },
    {CLEX::O, "pronoun"},
    {CLEX::P, "preposition"},
    {CLEX::Q, "quantifier-numeral"},
    {CLEX::V, "verb"},
    {CLEX::LET, "letter"},
    {CLEX::PN, "proper-noun"},
    {CLEX::SPEC, "special"},
    {CLEX::X, "unanalysed"},
    {CLEX::Z, "expression-part"},
    {CLEX::AFFIX, "affix"},
    {CLEX::XAFFIX,"x-affix"},
    {CLEX::NEUTRAL, "neutral"},
    {CLEX::UNASS, "unassigned"}
  };

  /// a hardcodes table with a mapping from inflection codes to a readable
  /// string
  const map<char,UnicodeString> iNames = {
    // the inflection names
    {'X', ""},
    {'s', "separated"},
    {'e', "singular"},
    {'m', "plural"},
    {'d', "diminutive"},
    {'G', "genitive"},
    {'D', "dative"},
    {'P', "positive"},
    {'C', "comparative"},
    {'S', "superlative"},
    {'E', "suffix-e"},
    {'i', "infinitive"},
    {'p', "participle"},
    {'t', "present-tense"},
    {'v', "past-tense"},
    {'1', "1st-person-verb"},
    {'2', "2nd-person-verb"},
    {'3', "3rd-person-verb"},
    {'I', "inversed"},
    {'g', "imperative"},
    {'a', "subjunctive"},
  };

  Type toCLEX( const UnicodeString& s ){
    /// convert a UnicodeString to a CLEX::Type
    /*!
      \param s a UnicodeString
      \return the CLEX::Type, may be UNASS when no translation is found
     */
    if ( s == "N" ) return N;
    else if ( s == "A" )    return A;
    else if ( s == "Q" )    return Q;
    else if ( s == "V" )    return V;
    else if ( s == "D" )    return D;
    else if ( s == "O" )    return O;
    else if ( s == "B" )    return B;
    else if ( s == "P" )    return P;
    else if ( s == "C" )    return C;
    else if ( s == "I" )    return I;
    else if ( s == "X" )    return X;
    else if ( s == "Z" )    return Z;
    else if ( s == "PN" )   return PN;
    else if ( s == "*" )    return AFFIX;
    else if ( s == "x" )    return XAFFIX;
    else if ( s == "^" )    return GLUE;
    else if ( s == "0" )    return NEUTRAL;
    else if ( s == "SPEC" ) return SPEC;
    else if ( s == "LET" )  return LET;
    return UNASS;
  }

  Type toCLEX( const UChar c ){
    /// convert a character  to a CLEX::Type
    /*!
      \param c a character
      \return the CLEX::Type, may be UNASS when no translation is found
     */
    UnicodeString s;
    s += c;
    return toCLEX(s);
  }

  string toString( const Type& t ){
    /// convert a CLEX::Type to a string
    /*!
      \param t a CLEX::Type value
      \return a string representing the type, in a format that can be converted
      back to the type using toCLEX()
    */
    switch ( t ){
    case N:       return "N";
    case A:       return "A";
    case Q:       return "Q";
    case V:       return "V";
    case D:       return "D";
    case O:       return "O";
    case B:       return "B";
    case P:       return "P";
    case C:       return "C";
    case I:       return "I";
    case X:       return "X";
    case Z:       return "Z";
    case PN:      return "PN";
    case AFFIX:   return "*";
    case XAFFIX:  return "x";
    case GLUE:    return "^";
    case NEUTRAL: return "0";
    case SPEC:    return "SPEC";
    case LET:     return "LET";
    default:
      return "/";
    }
  }

  UnicodeString toUnicodeString( const Type& t ){
    return TiCC::UnicodeFromUTF8( toString( t ) );
  }

  bool is_CELEX_base( const Type& t ){
    /// check if the type is a CELEX class
    /*!
      \param t a CLEX::Type
      \return true if so, otherwise false
     */
    switch ( t ){
    case A:
    case B:
    case C:
    case D:
    case I:
    case N:
    case O:
    case P:
    case Q:
    case V:
    case X:
    case Z:
      return true;
    default:
      return false;
    }
  }

  Type select_tag( const char ch ){
    /// select the CELEX base associated with a character code
    /*!
      \param ch a character code
      \return a CLEX::Type. may be UNASS when no translation is found
     */
    Type result = CLEX::UNASS;
    switch( ch ){
    case 'm':
    case 'e':
    case 'd':
    case 'G':
    case 'D':
      result = N;
      break;
    case 'P':
    case 'C':
    case 'S':
    case 'E':
      result = A;
      break;
    case 'i':
    case 'p':
    case 't':
    case 'v':
    case 'g':
    case 'a':
      result = V;
      break;
    default:
      break;
    }
    return result;
  }

  const UnicodeString& get_inflect_descr( char c ) {
    /// get the description related to the inflection symbol 'c'
    /*!
      \param c a character value for the inflexion
      \return a description, may be empty
    */
    static const UnicodeString empty = "";
    const auto& it = iNames.find(c);
    if ( it != iNames.end() ){
      return it->second;
    }
    else {
      return empty;
    }
  }

  const UnicodeString& get_tag_descr( CLEX::Type t ) {
    static const UnicodeString empty;
    /// get the description related to the CLEX::Type symbol 't'
    /*!
      \param t a CLEX::Type
      \return a description, may be empty
    */
    const auto& it = tagNames.find(t);
    if ( it != tagNames.end() ){
      return it->second;
    }
    else {
      return empty;
    }
  }

}

ostream& operator<<( ostream& os, const CLEX::Type& t ){
  os << toString( t );
  return os;
}
