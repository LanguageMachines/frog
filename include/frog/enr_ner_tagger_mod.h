/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2017
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

#ifndef ENER_TAGGER_MOD_H
#define ENER_TAGGER_MOD_H

#include "frog/tagger_base.h"

class ENERTagger: public BaseTagger {
 public:
  explicit ENERTagger( TiCC::LogStream * );
  bool init( const TiCC::Configuration& );
  void Classify( const std::vector<folia::Word *>& );
  void post_process( const std::vector<folia::Word*>& );
  void addDeclaration( folia::Document& ) const;
  void addNERTags( const std::vector<folia::Word*>&,
		   const std::vector<std::string>&,
		   const std::vector<double>& );
  bool read_gazets( const std::string&, const std::string& );
  std::vector<std::string> create_ner_list( const std::vector<std::string>& );
  bool Generate( const std::string& );
 private:
  bool fill_ners( const std::string&, const std::string&, const std::string& );
  std::vector<std::map<std::string,std::set<std::string>>> known_ners;
  int max_ner_size;
};

#endif // NER_TAGGER_MOD_H
