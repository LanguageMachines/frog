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

#include <iostream>
#include <iomanip>
#include "ticcutils/PrettyPrint.h"
#include "frog/FrogData.h"
#include "frog/mbma_brackets.h"

using namespace std;
using TiCC::operator<<;

frog_record::frog_record():
  no_space(false),
  new_paragraph(false),
  tag_confidence(0.0),
  iob_tag( "O" ),
  iob_confidence(0.0),
  ner_tag( "O" ),
  ner_confidence(0.0),
  compound_string( "0" ),
  parse_index(-1)
{}

frog_record::~frog_record(){
  for( const auto& dm : deep_morphs ){
    delete dm;
  }
}

const string TAB = "\t";

ostream& operator<<( ostream& os, const frog_record& fd ){
  os << fd.word << TAB;
  if ( !fd.lemmas.empty() ){
    os << fd.lemmas[0];
  }
  os << TAB;
  if ( fd.morphs.empty() ){
    if ( !fd.deep_morph_string.empty() ){
      os << fd.deep_morph_string;
    }
    else {
      if ( !fd.deep_morphs.empty() ){
	for ( const auto nm : fd.deep_morphs ){
	  os << "[" << nm << "]";
	  break; // first alternative only!
	  // if ( &nm != &fd.deep_morphs.back() ){
	  //   os << "/";
	  // }
	}
      }
    }
  }
  else {
    if ( !fd.morph_string.empty() ){
      os << fd.morph_string;
    }
    else {
      for ( const auto nm : fd.morphs ){
	for ( auto const& m : nm ){
	  os << m;
	}
	break; // first alternative only!
	// if ( &nm != &fd.morphs.back() ){
	//   os << "/";
	// }
      }
    }
  }
  os << TAB << fd.tag << TAB << fixed << showpoint << std::setprecision(6) << fd.tag_confidence;
  os << TAB << fd.ner_tag; // << TAB << fd.ner_confidence;
  os << TAB << fd.iob_tag; // << TAB << fd.iob_confidence;
  os << TAB << fd.parse_index;
  os << TAB << fd.parse_role;
  return os;
}

frog_record merge( const frog_data& fd, size_t start, size_t finish ){
  // cerr << "merge a FD of size:" << fd.units.size() << " with start=" << start
  //      << " and finish=" << finish << endl;
  frog_record result = fd.units[start];
  //  cerr << "start: " << result << endl;
  result.parts.insert( start );
  for ( size_t i = start+1; i <= finish; ++i ){
    result.parts.insert( i );
    result.word += "_" + fd.units[i].word;
    result.clean_word += "_" + fd.units[i].word;
    if ( !result.lemmas.empty() ){
      result.lemmas[0] += "_" + fd.units[i].lemmas[0];
    }
    if ( result.morphs.empty() ){
      if ( result.deep_morph_string.empty() ){
	// no morphemes
      }
      else {
	result.deep_morph_string += "_" + fd.units[i].deep_morph_string;
      }
    }
    else {
      result.morph_string += "_" + fd.units[i].morph_string;
      // cerr << endl << "STEP " << i << endl;
      // cerr << "result.morphs=" << result.morphs << endl;
      // cerr << "fd.units[" << i << "].morphs=" << fd.units[i].morphs << endl;
      result.morphs[0].back() += "_";
      for ( size_t pos=0; pos < fd.units[i].morphs.size(); ++pos ){
	auto variant = fd.units[i].morphs[pos];
	for ( size_t k=0; k < variant.size(); ++k ){
	  result.morphs[0].back() += variant[k];
	}
      }
    }
    result.tag += "_" + fd.units[i].tag;
    result.compound_string += "_" + fd.units[i].compound_string;
    result.tag_confidence *= fd.units[i].tag_confidence;
    result.ner_tag += "_" + fd.units[i].ner_tag;
    result.iob_tag += "_" + fd.units[i].iob_tag;
    // cerr << "intermediate: " << result << endl;
    // cerr << "result.morphs=" << result.morphs << endl;
  }
  // cerr << "DONE: " << result << endl;
  // cerr << "result.morphs=" << result.morphs << endl;
  return result;
}

string frog_data::sentence( bool tokenized ) const {
  ///
  /// extract the sentence from a frog_data structure by concatenating
  /// the words in the units. Normally separated by spaces.
  /// When @tokenized is true, the 'no_space' value is taken into account.
  string result;
  for ( const auto& it : units ){
    result += it.word;
    if ( !tokenized || !it.no_space ){
      result += " ";
    }
  }
  return result;
}

void frog_data::resolve_mwus(){
  mw_units.clear();
  for ( size_t pos=0; pos < units.size(); ++pos ){
    if ( mwus.find( pos ) == mwus.end() ){
      mw_units.push_back( units[pos] );
      mw_units.back().deep_morphs.clear(); // we don't need them AND avoid
      //                                      pointers to deleted memory
      // A shallow copy function would be better
      mw_units.back().parts.insert( pos );
    }
    else {
      frog_record merged = merge( *this, pos, mwus.find( pos )->second );
      merged.deep_morphs.clear(); // we don't need them AND avoid pointers to
      //                             deleted memory
      // A shallow copy function would be better
      mw_units.push_back( merged );
      pos = mwus[pos];
    }
  }
}

ostream& operator<<( ostream& os, const frog_data& fd ){
  if ( fd.mw_units.empty() ){
    for ( size_t pos=0; pos < fd.units.size(); ++pos ){
      os << pos+1 << TAB << fd.units[pos] << endl;
    }
  }
  else {
    for ( size_t pos=0; pos < fd.mw_units.size(); ++pos ){
      os << pos+1 << TAB << fd.mw_units[pos] << endl;
    }
  }
  return os;
}

void frog_data::append( const frog_record& fr ){
  units.push_back( fr );
}
