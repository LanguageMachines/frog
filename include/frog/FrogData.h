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

#ifndef FROGDATA_H
#define FROGDATA_H

#include <string>
#include <vector>
#include <map>
#include <set>

class frog_record {
 public:
  frog_record();
  frog_record( size_t );
  std::string word;
  std::string token_class;
  std::string language;
  bool no_space;
  std::string tag;
  double tag_confidence;
  std::string iob_tag;
  double iob_confidence;
  std::string ner_tag;
  double ner_confidence;
  std::vector<std::string> lemmas;
  std::vector<std::vector<std::string>> morphs;
  std::vector<std::string> morphs_nested;
  int parse_index;
  std::string parse_role;
  std::set<size_t> parts;
};

class frog_data {
 public:
  size_t size() const { return units.size(); };
  bool empty() const { return units.size() == 0; };
  void resolve_mwus();
  std::vector<frog_record> units;
  std::vector<frog_record> mw_units;
  std::map<size_t,size_t> mwus; // maps a start pos to end pos
};

std::ostream& operator<<( std::ostream&, const frog_record& );
std::ostream& operator<<( std::ostream&, frog_data& );

#endif
