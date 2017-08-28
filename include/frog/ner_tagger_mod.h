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

#ifndef NER_TAGGER_MOD_H
#define NER_TAGGER_MOD_H

#include "frog/pos_tagger_mod.h"

class NERTagger: public POSTagger {
 public:
  explicit NERTagger( TiCC::LogStream *l ): POSTagger( l,  "NER" ){};
  bool init( const TiCC::Configuration&, const std::string& );
  void Classify( const std::vector<folia::Word *>& );
  void addDeclaration( folia::Document& ) const;
  void addNERTags( const std::vector<folia::Word*>&,
		   const std::vector<std::string>&,
		   const std::vector<double>& );
  std::string getTagset() const { return tagset; };
  std::vector<Tagger::TagResult> tagLine( const std::string& );
  bool fill_known_ners( const std::string&, const std::string& );
  void handle_known_ners( const std::vector<std::string>&,
			  std::vector<std::string>& );
  void merge( const std::vector<std::string>&,
	      std::vector<std::string>& tags,
	      std::vector<double>& );
  std::string set_eos_mark( const std::string& );
 private:
  std::vector<std::map<std::string,std::string>> known_ners;
  int max_ner_size;
};

#endif // NER_TAGGER_MOD_H
