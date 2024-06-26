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

#ifndef MWU_CHUNKER_H
#define MWU_CHUNKER_H

#include <ostream>
#include <string>
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"
#include "frog/FrogData.h"

/// \brief a helper class for Mwu. Stores needed information.
class mwuAna {
  friend std::ostream& operator<< (std::ostream&, const mwuAna& );
public:
  mwuAna( const icu::UnicodeString&, bool, size_t );
  virtual ~mwuAna() {};

  void merge( const mwuAna * );

  icu::UnicodeString getWord() const {
    return word;
  }

  bool isSpec(){ return spec; };

  size_t mwu_start;
  size_t mwu_end;

protected:
  mwuAna(){};
  icu::UnicodeString word;
  bool spec;
};

#define mymap2 std::multimap<icu::UnicodeString, std::vector<icu::UnicodeString> >

/// \brief provide all functionality to detect MWU's
class Mwu {
  friend std::ostream& operator<< (std::ostream&, const Mwu& );
public:
  explicit Mwu( TiCC::LogStream*, TiCC::LogStream* );
  ~Mwu();
  void reset();
  bool init( const TiCC::Configuration& );
  void add_provenance( folia::Document&, folia::processor * ) const;
  void Classify( frog_data& );
  void add( const frog_record& );
  void add_result( const frog_data&,
		   const std::vector<folia::Word*>& ) const;
  /// return the value for \e mwu_tagset. (set via Configuration)
  const std::string& getTagset() const { return mwu_tagset; };
  const std::string& version() const { return _version; };
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
  icu::UnicodeString glue_tag;
  TiCC::UniFilter *filter;
  Mwu( const Mwu& ) = delete; // no copies
  Mwu operator=( const Mwu& ) = delete; // no copies
};

#endif
