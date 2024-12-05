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

#ifndef NER_TAGGER_MOD_H
#define NER_TAGGER_MOD_H

#include <unordered_map>
#include <vector>
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "libfolia/folia.h"
#include "frog/tagger_base.h"

using tc_pair = std::pair<icu::UnicodeString,double>;

/// \brief a specialization of Basetagger to tag Named Entities
class NERTagger: public BaseTagger {
 public:
  explicit NERTagger( TiCC::LogStream *, TiCC::LogStream * =0 );
  bool init( const TiCC::Configuration& ) override;
  void Classify( frog_data& ) override;
  void post_process( frog_data& ) override;
  void post_process( frog_data&,
		     const std::vector<tc_pair>& );
  void add_declaration( folia::Document&, folia::processor * ) const override;
  void add_result( const frog_data& fd,
		   const std::vector<folia::Word*>& wv ) const;
  bool read_gazets( const std::string& f, const std::string& p ){
    return read_gazets( f, p, gazet_ners );
  }
  bool read_overrides( const std::string& f, const std::string& p ){
    return read_gazets( f, p, override_ners );
  }
  std::vector<icu::UnicodeString> create_ner_list( const std::vector<icu::UnicodeString>& s ){
    return create_ner_list( s, gazet_ners );
  }
  std::vector<icu::UnicodeString> create_override_list( const std::vector<icu::UnicodeString>& s ){
    return create_ner_list( s, override_ners );
  }
  bool Generate( const std::string& );
  void merge_override( std::vector<tc_pair>&,
		       const std::vector<tc_pair>&,
		       bool,
		       const std::vector<icu::UnicodeString>& ) const;

 private:
  bool read_gazets( const std::string&,
		    const std::string&,
		    std::vector<std::map<icu::UnicodeString,std::set<std::string>>>& );
  bool fill_ners( const std::string&,
		  const std::string&,
		  const std::string&,
		  std::vector<std::map<icu::UnicodeString,std::set<std::string>>>& );
  std::vector<icu::UnicodeString> create_ner_list( const std::vector<icu::UnicodeString>&,
						   const std::vector<std::map<icu::UnicodeString,std::set<std::string>>>& );
  std::vector<std::map<icu::UnicodeString,std::set<std::string>>> gazet_ners;
  std::vector<std::map<icu::UnicodeString,std::set<std::string>>> override_ners;
  void addEntity( frog_data&,
		  size_t,
		  const std::vector<tc_pair>& );
  bool gazets_only;
  int max_ner_size;
};

#endif // NER_TAGGER_MOD_H
