/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2020
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
#include "ticcutils/json.hpp"

class BaseBracket;
namespace Tokenizer {
  class Token;
}

class frog_record {
 public:
  frog_record();
  ~frog_record();
  nlohmann::json to_json() const;
  std::string word;
  std::string clean_word;
  std::string token_class;
  std::string language;
  bool no_space;
  bool new_paragraph;
  std::string tag;
  double tag_confidence;
  std::string iob_tag;
  double iob_confidence;
  std::string ner_tag;
  double ner_confidence;
  std::vector<std::string> lemmas;
  std::vector<std::vector<std::string>> morphs;
  std::vector<const BaseBracket*> deep_morphs;
  std::string compound_string;   // string representation of first compound
  std::string morph_string;      // string representation of first morph
  std::string deep_morph_string; // string representation of first deep_morph
  int parse_index;
  std::string parse_role;
  std::set<size_t> parts;
};

class frog_data {
  friend frog_data extract_fd( std::vector<Tokenizer::Token>& );
 public:
  size_t size() const { return units.size(); };
  bool empty() const { return units.size() == 0; };
  void resolve_mwus();
  void append( const frog_record& );
  std::string get_language() const;
  std::string sentence( bool = false ) const;
  std::vector<frog_record> units;
  std::vector<frog_record> mw_units;
  std::map<size_t,size_t> mwus; // maps a start pos to end pos
 private:
  std::string language; // the language of all units
};

std::ostream& operator<<( std::ostream&, const frog_record& );
std::ostream& operator<<( std::ostream&, const frog_data& );

#endif
