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

#ifndef POS_TAGGER_MOD_H
#define POS_TAGGER_MOD_H

#include "mbt/MbtAPI.h"

class POSTagger {
 public:
  explicit POSTagger( TiCC::LogStream * );
  virtual ~POSTagger();
  virtual bool init( const TiCC::Configuration& );
  virtual void Classify( const std::vector<folia::Word *>& );
  void addDeclaration( folia::Document& ) const;
  void addTag( folia::Word *, const std::string&, double, bool );
  std::vector<Tagger::TagResult> tagLine( const std::string& );
  std::string getTagset() const { return tagset; };
  bool fill_map( const std::string&, std::map<std::string,std::string>& );
  std::string set_eos_mark( const std::string& );
 protected:
  int debug;
  std::string tagset;
  TiCC::LogStream *tag_log;
 private:
  MbtAPI *tagger;
  std::string version;
  std::string textclass;
  Tokenizer::UnicodeFilter *filter;
  std::map<std::string,std::string> token_tag_map;
  std::set<std::string> valid_tags;
  POSTagger( const POSTagger& ){} // inhibit copies
};

#endif // POS_TAGGER_MOD_H
