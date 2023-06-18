/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2023
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

#ifndef TAGGER_BASE_H
#define TAGGER_BASE_H

#include <vector>
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/SocketBasics.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/json.hpp"
#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "frog/FrogData.h"

/// \brief helper class to store a word + enrichment
class tag_entry {
public:
  icu::UnicodeString word;
  icu::UnicodeString enrichment;
};

/// \brief a base Class for interfacing to MBT taggers
class BaseTagger {
 public:
  explicit BaseTagger( TiCC::LogStream *,
		       TiCC::LogStream *,
		       const std::string& );
  virtual ~BaseTagger();
  virtual bool init( const TiCC::Configuration& );
  virtual void post_process( frog_data& ) = 0;
  virtual void Classify( frog_data& );
  virtual void add_declaration( folia::Document&, folia::processor * ) const = 0;
  void add_provenance( folia::Document&, folia::processor * ) const;
  std::string getTagset() const { return tagset; };
  icu::UnicodeString set_eos_mark( const icu::UnicodeString& );
  bool fill_map( const std::string& );
  std::vector<Tagger::TagResult> tagLine( const icu::UnicodeString& );
  std::vector<Tagger::TagResult> tag_entries( const std::vector<tag_entry>& );
  std::string version() const { return _version; };
 private:
  std::vector<tag_entry> extract_sentence( const frog_data& );
  nlohmann::json read_from_client( Sockets::ClientSocket& ) const;
  void write_to_client( nlohmann::json&,
			Sockets::ClientSocket& ) const;
 protected:
  void extract_words_tags(  const std::vector<folia::Word *>&,
			    const std::string&,
			    std::vector<icu::UnicodeString>&,
			    std::vector<icu::UnicodeString>& );
  std::vector<Tagger::TagResult> call_server( const std::vector<tag_entry>& ) const;
  int debug;
  std::string _label;
  std::string tagset;
  std::string _version;
  std::string textclass;
  TiCC::LogStream *err_log;
  TiCC::LogStream *dbg_log;
  std::string base;
  std::string _host;
  std::string _port;
  MbtAPI *tagger;
  TiCC::UniFilter *filter;
  std::vector<std::string> _words;
  std::vector<Tagger::TagResult> _tag_result;
  std::map<icu::UnicodeString,icu::UnicodeString> token_tag_map;
  BaseTagger( const BaseTagger& ) = delete; // inhibit copies
  BaseTagger& operator=( const BaseTagger& ) = delete; // inhibit copies
};

icu::UnicodeString filter_spaces( const icu::UnicodeString& );

#endif // TAGGER_BASE_H
