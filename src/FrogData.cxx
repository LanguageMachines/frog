/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2018
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

#include <ostream>
#include <iomanip>
#include "ticcutils/PrettyPrint.h"
#include "frog/FrogData.h"

using namespace std;
using TiCC::operator<<;

frog_record::frog_record():
  no_space(false),
  tag_confidence(0.0),
  iob_tag( "O" ),
  iob_confidence(0.0),
  ner_tag( "O" ),
  ner_confidence(0.0)
{}

const string TAB = "\t";

ostream& operator<<( ostream& os, const frog_record& fd ){
  os << fd.word << TAB;
  if ( !fd.lemmas.empty() ){
    os << fd.lemmas[0];
  }
  os << TAB;
  if ( fd.morphs.empty() ){
    if ( !fd.morphs_nested.empty() ){
      os << fd.morphs_nested[0];
    }
  }
  else {
    os << fd.morphs;
  }
  os << TAB << fd.tag << TAB << fixed << showpoint << std::setprecision(6) << fd.tag_confidence;
  os << TAB << fd.ner_tag; // << TAB << fd.ner_confidence;
  os << TAB << fd.iob_tag; // << TAB << fd.iob_confidence;
  os << TAB << fd.parse_index;
  os << TAB << fd.parse_role;
  return os;
}

frog_record merge( const frog_data& fd, size_t start, size_t finish ){
  frog_record result = fd.units[start];
  for ( size_t i = start+1; i <= finish; ++i ){
    result.word += "_" + fd.units[i].word;
    result.lemmas[0] += "_" + fd.units[i].lemmas[0];
    if ( result.morphs.empty() ){
      result.morphs_nested[0] += "_" + fd.units[i].morphs_nested[0];
    }
    else {
      result.morphs[0] += "]_[" + fd.units[i].morphs[0];
    }
    result.tag += "_" + fd.units[i].tag;
    result.tag_confidence *= fd.units[i].tag_confidence;
    result.ner_tag += "_" + fd.units[i].ner_tag;
    result.iob_tag += "_" + fd.units[i].iob_tag;
  }
  return result;
}

void frog_data::resolve_mwus(){
  mw_units.clear();
  for ( size_t pos=0; pos < units.size(); ++pos ){
    if ( mwus.find( pos ) == mwus.end() ){
      mw_units.push_back( units[pos] );
    }
    else {
      frog_record merged = merge( *this, pos, mwus.find( pos )->second );
      mw_units.push_back( merged );
      pos = mwus.find( pos )->second;
    }
  }
}

ostream& operator<<( ostream& os, frog_data& fd ){
  for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
    os << pos+1 << TAB << fd.mw_units[pos] << endl;
  }
  return os;
}
