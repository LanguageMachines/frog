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

#include <string>
#include <map>
#include "frog/clex.h"

using namespace std;

map<CLEX::Type,string> tagNames;
map<char,string> iNames;

namespace CLEX {

  Type toCLEX( const string& s ){
    if ( s == "N" )
      return N;
    else if ( s == "A" )
      return A;
    else if ( s == "Q" )
      return Q;
    else if ( s == "V" )
      return V;
    else if ( s == "D" )
      return D;
    else if ( s == "O" )
      return O;
    else if ( s == "B" )
      return B;
    else if ( s == "P" )
      return P;
    else if ( s == "Y" )
      return Y;
    else if ( s == "I" )
      return I;
    else if ( s == "X" )
      return X;
    else if ( s == "Z" )
      return Z;
    else if ( s == "PN" )
      return PN;
    else if ( s == "*" )
      return AFFIX;
    else if ( s == "x" )
      return XAFFIX;
    else if ( s == "0" )
      return NEUTRAL;
    else
      return UNASS;
  }

  Type toCLEX( const char c ){
    string s;
    s += c;
    return toCLEX(s);
  }

  string toLongString( const Type& t ){
    switch ( t ){
    case N:
      return "noun";
    case A:
      return "adjective";
    case Q:
      return "quantifier/numeral";
    case V:
      return "verb";
    case D:
      return "article";
    case O:
      return "pronoun";
    case B:
      return "adverb";
    case P:
      return "preposition";
    case Y:
      return "conjunction";
    case I:
      return "interjection";
    case X:
      return "unanalysed";
    case Z:
      return "expression part";
    case PN:
      return "proper noun";
    case AFFIX:
      return "affix";
    case XAFFIX:
      return "x affix";
    case NEUTRAL:
      return "0";
    default:
      return "UNASS";
    }
  }

  string toString( const Type& t ){
    switch ( t ){
    case N:
      return "N";
    case A:
      return "A";
    case Q:
      return "Q";
    case V:
      return "V";
    case D:
      return "D";
    case O:
      return "O";
    case B:
      return "B";
    case P:
      return "P";
    case Y:
      return "Y";
    case I:
      return "I";
    case X:
      return "X";
    case Z:
      return "Z";
    case PN:
      return "PN";
    case AFFIX:
      return "*";
    case XAFFIX:
      return "x";
    case NEUTRAL:
      return "0";
    default:
      return "/";
    }
  }

  bool isBasicClass( const Type& t ){
    const string basictags = "NAQVDOBPYIXZ";
    switch ( t ){
    case N:
    case A:
    case Q:
    case V:
    case D:
    case O:
    case B:
    case P:
    case Y:
    case I:
    case X:
    case Z:
      return true;
    default:
      return false;
    }
  }

}

ostream& operator<<( ostream& os, const CLEX::Type t ){
  os << toString( t );
  return os;
}
