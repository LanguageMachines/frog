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

#ifndef FROGDATA_H
#define FROGDATA_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include "ticcutils/Unicode.h"
#include "ticcutils/json.hpp"

class BaseBracket;
namespace Tokenizer {
  class Token;
}

/// a simple datastructure to hold all frogged information of one word
class frog_record {
 public:
  frog_record();
  ~frog_record();
  nlohmann::json to_json() const;
  icu::UnicodeString word;          ///< the word in Unicode
  icu::UnicodeString clean_word;    ///< lowercased word (MBMA only) in Unicode
  icu::UnicodeString token_class;   ///< the assigned token class of the word
  std::string language;      ///< the detetected language of the word
  bool no_space;             ///< was there a space after the word?
  bool new_paragraph;        ///< did the tokenizer detect a paragraph here?
  icu::UnicodeString tag;           ///< the assigned POS tag in Unicode
  double tag_confidence;            ///< the confidence of the POS tag
  icu::UnicodeString next_tag;      ///< the assigned next POS tag in Unicode
  icu::UnicodeString iob_tag;       ///< the assigned IOB tag
  double iob_confidence;     ///< the confidence of the IOB tag
  icu::UnicodeString ner_tag;       ///< the assigned NER tag
  double ner_confidence;     ///< the confidence of the NER tag
  std::vector<icu::UnicodeString> lemmas;  ///< a list of possible lemma's
  icu::UnicodeString morph_string;      ///< UnicodeString representation of first morph analysis
  std::vector<const BaseBracket*> morph_structure;  ///< pointers to the deep morphemes
  std::string compound_string;   ///< string representation of first compound
  int parse_index;           ///< label of the dependency
  std::string parse_role;    ///< role of the dependency
  std::set<size_t> parts;    ///< set of indices a MWU is made of (MWU only)
};

/// a datastructure to hold all frogged information of one Sentence
class frog_data {
  friend frog_data extract_fd( std::vector<Tokenizer::Token>& );
 public:
  size_t size() const { return units.size(); };
  bool empty() const { return units.size() == 0; };
  void resolve_mwus();
  void append( const frog_record& );
  std::string get_language() const;
  std::string sentence( bool = false ) const;
  std::vector<frog_record> units;    ///< the records that make up the sentence
  std::vector<frog_record> mw_units; ///< the MWU records that make up the sentence
  std::map<size_t,size_t> mwus;      ///> maps that stores MWU start and end pos
};

std::ostream& operator<<( std::ostream& os, const frog_record& fr);
std::ostream& operator<<( std::ostream& os, const frog_data& fd);

#endif
