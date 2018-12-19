/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
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
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"
#include "frog/FrogData.h"

class mwuAna {
  friend std::ostream& operator<< (std::ostream&, const mwuAna& );
 public:
  mwuAna( const std::string&, const std::string&, const std::string&, size_t );
  virtual ~mwuAna() {};

  void merge( const mwuAna * );

  std::string getWord() const {
    return word;
  }

  bool isSpec(){ return spec; };

  size_t mwu_start;
  size_t mwu_end;

 protected:
    mwuAna(){};
    std::string word;
    bool spec;
};

#define mymap2 std::multimap<std::string, std::vector<std::string> >

class Mwu {
  friend std::ostream& operator<< (std::ostream&, const Mwu& );
 public:
  explicit Mwu( TiCC::LogStream*, TiCC::LogStream* );
  ~Mwu();
  void reset();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& ) const;
  void addDeclaration( folia::Processor& ) const;
  void Classify( frog_data& );
  void add( frog_record&, size_t );
  void add_result( const frog_data&,
		   const std::vector<folia::Word*>& ) const;
  std::string getTagset() const { return mwu_tagset; };
  std::vector<mwuAna*>& getAna(){ return mWords; };
  std::string version() const { return _version; };
 private:
  bool readsettings( const std::string&, const std::string&);
  bool read_mwus( const std::string& );
  void Classify();
  int debug;
  std::string mwuFileName;
  std::vector<mwuAna*> mWords;
  mymap2 MWUs;
  TiCC::LogStream *errLog;
  TiCC::LogStream *dbgLog;
  std::string _version;
  std::string textclass;
  std::string mwu_tagset;
  std::string glue_tag;
  TiCC::UniFilter *filter;
};

#endif
