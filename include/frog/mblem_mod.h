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

#ifndef MBLEM_MOD_H
#define MBLEM_MOD_H

#include "libfolia/folia.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ucto/tokenize.h"
#include "timbl/TimblAPI.h"

class mblemData {
 public:
 mblemData( const std::string& l, const std::string& t ):
  lemma( l ),
    tag( t ) { };
  std::string getLemma() const { return lemma; };
  std::string getTag() const { return tag; };
 private:
  std::string lemma;
  std::string tag;
};

class Mblem {
 public:
  explicit Mblem( TiCC::LogStream * );
  ~Mblem();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& doc ) const;
  void Classify( folia::Word * );
  void Classify( const UnicodeString& );
  std::vector<std::pair<std::string,std::string> > getResult() const;
  void filterTag( const std::string&  );
  void makeUnique();
  std::string getTagset() const { return tagset; };
  bool fill_ts_map( const std::string& );
  bool fill_eq_set( const std::string& );
 private:
  void read_transtable( const std::string& );
  void create_MBlem_defaults();
  bool readsettings( const std::string& dir, const std::string& fname );
  void addLemma( folia::Word *, const std::string&) ;
  std::string make_instance( const UnicodeString& in );
  void getFoLiAResult( folia::Word *, const UnicodeString& );
  Timbl::TimblAPI *myLex;
  std::string punctuation;
  size_t history;
  int debug;
  bool keep_case;
  std::map<std::string,std::string> classMap;
  std::map<std::string, std::map<std::string, int>> token_strip_map;
  std::set<std::string> one_one_tags;
  std::vector<mblemData> mblemResult;
  std::string version;
  std::string tagset;
  std::string POS_tagset;
  std::string textclass;
  TiCC::LogStream *mblemLog;
  Tokenizer::UnicodeFilter *filter;
};

#endif
