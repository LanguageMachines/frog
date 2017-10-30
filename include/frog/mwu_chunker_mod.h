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

#ifndef MWU_CHUNKER_H
#define MWU_CHUNKER_H

#include <ostream>
#include <string>
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "libfolia/folia.h"

class mwuAna {
  friend std::ostream& operator<< (std::ostream&, const mwuAna& );
 public:
  mwuAna( folia::Word *, const std::string&, const std::string& );
  virtual ~mwuAna() {};

  void merge( const mwuAna * );

  std::string getWord() const {
    return word;
  }

  bool isSpec(){ return spec; };
  folia::EntitiesLayer *addEntity( const std::string&,
				   const std::string&,
				   folia::Sentence *,
				   folia::EntitiesLayer * );

 protected:
    mwuAna(){};
    std::string word;
    bool spec;
    std::vector<folia::Word *> fwords;
};

#define mymap2 std::multimap<std::string, std::vector<std::string> >

class Mwu {
  friend std::ostream& operator<< (std::ostream&, const Mwu& );
 public:
  explicit Mwu(TiCC::LogStream*);
  ~Mwu();
  void reset();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& ) const;
  void Classify( const std::vector<folia::Word *>& );
  void add( folia::Word * );
  std::string getTagset() const { return mwu_tagset; };
  std::vector<mwuAna*>& getAna(){ return mWords; };
 private:
  bool readsettings( const std::string&, const std::string&);
  bool read_mwus( const std::string& );
  void Classify();
  int debug;
  std::string mwuFileName;
  std::vector<mwuAna*> mWords;
  mymap2 MWUs;
  TiCC::LogStream *mwuLog;
  std::string version;
  std::string textclass;
  std::string mwu_tagset;
  std::string glue_tag;
  Tokenizer::UnicodeFilter *filter;
};

#endif
