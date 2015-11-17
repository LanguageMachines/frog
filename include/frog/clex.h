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


#ifndef CLEX_H
#define CLEX_H

namespace CLEX {
  enum Type { UNASS, N, A, Q, V, D, O, B, P, C, I, X, Z, PN, AFFIX, XAFFIX,
	      NEUTRAL };
  bool isBasicClass( const Type& );
  Type select_tag( const char ch );
  std::string toString( const Type& );
  Type toCLEX( const std::string& );
  Type toCLEX( const char );
  extern std::map<CLEX::Type,std::string> tagNames;
  extern std::map<char,std::string> iNames;
  inline std::string get_iDescr( char c ) { return iNames[c]; }
  inline std::string get_tDescr( CLEX::Type t ) {  return tagNames[t]; }
}

std::ostream& operator<<( std::ostream&, const CLEX::Type& );

#endif // CLEX_H
