/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2022
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
#include "ticcutils/StringOps.h"
#include "ticcutils/Unicode.h"
#include "frog/FrogData.h"
#include "frog/mbma_brackets.h"

using namespace std;
using namespace icu;
using namespace nlohmann;
using TiCC::operator<<;

/// default constructor
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

/// default destructor
frog_record::~frog_record(){
  for( const auto& dm : deep_morphs ){
    delete dm;
  }
}

json frog_record::to_json() const {
  /// format a frog_record fd into a json structure
  /*!
    \return an JSON structure
  */
  json result;
  result["word"] = TiCC::UnicodeToUTF8(word);
  if ( !token_class.isEmpty() ){
    json tok;
    tok["token"] = TiCC::UnicodeToUTF8(token_class);
    if ( no_space ){
      tok["space"] = false;
    }
    if ( new_paragraph ){
      tok["new_paragraph"] = true;
    }
    result["ucto"] = tok;
  }
  if ( !lemmas.empty() ){
    result["lemma"] = TiCC::UnicodeToUTF8(lemmas[0]);
  }
  if ( !deep_morph_string.empty() ){
    result["deep_morph"] = deep_morph_string;
    if ( compound_string.find("0") == string::npos ){
      result["compound"] = compound_string;
    }
  }
  else if ( !morph_string.empty() ){
    result["morph"] = morph_string;
  }
  if ( !tag.isEmpty() ){
    json tg;
    tg["tag"] = TiCC::UnicodeToUTF8(tag);
    tg["confidence"] = tag_confidence;
    result["pos"] = tg;
  }
  if ( !ner_tag.isEmpty() && ner_confidence > 0.0 ){
    json tg;
    tg["tag"] = TiCC::UnicodeToUTF8(ner_tag);
    tg["confidence"] = ner_confidence;
    result["ner"] = tg;
  }
  if ( !iob_tag.isEmpty() ){
    json tg;
    tg["tag"] = TiCC::UnicodeToUTF8(iob_tag);
    tg["confidence"] = iob_confidence;
    result["chunking"] = tg;
  }
  if ( !parse_role.empty() ){
    json parse;
    parse["parse_index"] = parse_index;
    parse["parse_role"] = parse_role;
    result["parse"] = parse;
  }
  return result;
}


const string TAB = "\t";

ostream& operator<<( ostream& os, const frog_record& fr ){
  /// output a frog_record structure to a stream
  /*!
    \param os output stream
    \param fr the record to output
    \return the stream
  */
  os << fr.word << TAB;
  if ( !fr.lemmas.empty() ){
    os << fr.lemmas[0];
  }
  os << TAB;
  if ( fr.morphs.empty() ){
    if ( !fr.deep_morph_string.empty() ){
      os << fr.deep_morph_string;
    }
    else {
      if ( !fr.deep_morphs.empty() ){
	for ( const auto nm : fr.deep_morphs ){
	  os << "[" << nm << "]";
	  break; // first alternative only!
	  // if ( &nm != &fr.deep_morphs.back() ){
	  //   os << "/";
	  // }
	}
      }
    }
  }
  else {
    if ( !fr.morph_string.empty() ){
      os << fr.morph_string;
    }
    else {
      for ( const auto nm : fr.morphs ){
	for ( auto const& m : nm ){
	  os << m;
	}
	break; // first alternative only!
      }
    }
  }
  os << TAB << fr.tag << TAB << fixed << showpoint << std::setprecision(6) << fr.tag_confidence;
  os << TAB << fr.ner_tag; // << TAB << fr.ner_confidence;
  os << TAB << fr.iob_tag; // << TAB << fr.iob_confidence;
  os << TAB << fr.parse_index;
  os << TAB << fr.parse_role;
  return os;
}

frog_record merge( const frog_data& fd, size_t start, size_t finish ){
  /// merge a range of records of an frog_data structure into the first one
  /*!
    \param fd the frog_data structure
    \param start index of the first record in the structure to merge
    \param finish index of the last record in the structure to merge
    \return the new merged record

    all information from the records \e start +1 to \em finish is merged into
    the record at position \e start. Strings are concatenated using an
    underscore ('_') which is the way Frog has always displayed MWU's

    \note merging is only done for the first (default) lemma and morpheme
   */
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
  /// extract the sentence from a frog_data structure by concatenating
  /// the words in the units. Normally separated by spaces.
  /*!
    \param tokenized When true, the 'no_space' value is taken into account.
    \return a UTF8 string of the orginal words, separated by 1 space
    except when the no_space value is set AND \e tokenized is true
   */
  UnicodeString result;
  for ( const auto& it : units ){
    result += it.word;
    if ( !tokenized || !it.no_space ){
      result += " ";
    }
  }
  return TiCC::UnicodeToUTF8(result);
}

void frog_data::resolve_mwus(){
  /// resolve MWU's by merging them into the first record of the MWU
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
  /// output a frog_data structure to a stream
  /*!
    \param os output stream
    \param fd the record to output
    \return the stream
  */
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
  /// add a frog_record to the frog_data structure
  /*!
    \param fr the record to add.
  */
  units.push_back( fr );
}

string frog_data::get_language() const {
  /// return the language of the frog_data structure
  /*!
    \return a string
    loop through all records and return the first non-default language value
    returns "default" when nothing was found
   */
  string result = "default";
  for ( const auto& r : units ){
    if ( !r.language.empty() ){
      return r.language;
    }
  }
  return result;
}
