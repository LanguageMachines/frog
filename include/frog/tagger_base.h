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

#ifndef TAGGER_BASE_H
#define TAGGER_BASE_H

#include "mbt/MbtAPI.h"

class BaseTagger {
 public:
  explicit BaseTagger( TiCC::LogStream *, const std::string& );
  virtual ~BaseTagger();
  virtual bool init( const TiCC::Configuration& );
  virtual void post_process( const std::vector<folia::Word*>& ) = 0;
  void Classify( const std::vector<folia::Word*>& );
  void addDeclaration( folia::Document& ) const;
  std::string getTagset() const { return tagset; };
  std::string set_eos_mark( const std::string& );
  bool fill_map( const std::string&, std::map<std::string,std::string>& );
  std::vector<Tagger::TagResult> tagLine( const std::string& );
 private:
  std::string extract_sentence( const std::vector<folia::Word*>&,
				std::vector<std::string>& );
 protected:
  int debug;
  std::string _label;
  std::string tagset;
  std::string version;
  std::string textclass;
  TiCC::LogStream *tag_log;
  MbtAPI *tagger;
  Tokenizer::UnicodeFilter *filter;
  std::vector<std::string> _words;
  std::vector<Tagger::TagResult> _tag_result;
  std::map<std::string,std::string> token_tag_map;
  BaseTagger( const BaseTagger& ){} // inhibit copies
};

#endif // TAGGER_BASE_H
