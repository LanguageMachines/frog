/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2021
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

#ifndef MBMA_MOD_H
#define MBMA_MOD_H

#include <map>
#include <vector>
#include <list>
#include "unicode/unistr.h"
#include <unicode/translit.h>
#include "ticcutils/LogStream.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/Configuration.h"
#include "libfolia/folia.h"
#include "frog/FrogData.h"
#include "frog/clex.h"
#include "frog/mbma_rule.h"
#include "frog/mbma_brackets.h"

class MBMAana;
namespace Timbl{
  class TimblAPI;
}

/// \brief provide all functionality to run a Timbl for Morphological Analyzis
class Mbma {
 public:
  explicit Mbma( TiCC::LogStream *, TiCC::LogStream * =0 );
  ~Mbma();
  bool init( const TiCC::Configuration& );
  void add_provenance( folia::Document&, folia::processor * ) const;
  void Classify( frog_record& );
  void Classify( const icu::UnicodeString& );
  void filterHeadTag( const icu::UnicodeString& );
  void filterSubTags( const std::vector<icu::UnicodeString>& );
  void assign_compounds();
  std::vector<icu::UnicodeString> getResult() const;
  std::vector<std::pair<icu::UnicodeString,std::string>> getResults( ) const;
  std::vector<std::pair<std::string,std::string>> getPrettyResults( ) const;
  void setDeepMorph( bool b ){ doDeepMorph = b; };
  void clearAnalysis();
  Rule* matchRule( const std::vector<icu::UnicodeString>&,
		   const icu::UnicodeString& );
  std::vector<Rule*> execute( const icu::UnicodeString& ,
			      const std::vector<icu::UnicodeString>& );
  std::string version() const { return _version; };
  void add_morphemes( const std::vector<folia::Word*>&,
		      const frog_data& fd ) const;
  static std::map<icu::UnicodeString,icu::UnicodeString> TAGconv;
  static std::string mbma_tagset;
  static std::string pos_tagset;
  static std::string clex_tagset;
 private:
  void cleanUp();
  bool readsettings( const std::string&, const std::string& );
  void fillMaps();
  void init_cgn( const std::string&, const std::string& );
  void getResult( frog_record&,
		  const icu::UnicodeString&,
		  const icu::UnicodeString& ) const;
  std::vector<std::string> make_instances( const icu::UnicodeString& word );
  void call_server( const std::vector<std::string>&,
		    std::vector<icu::UnicodeString>& );
  CLEX::Type getFinalTag( const std::list<BaseBracket*>& );
  int debugFlag;
  void store_morphemes( frog_record&,
			const std::vector<icu::UnicodeString>& ) const;
  void store_brackets( frog_record&,
		       const icu::UnicodeString&,
		       const icu::UnicodeString&,
		       bool=false ) const;
  void addBracketMorph( folia::Word *,
			const std::string&,
			const BaseBracket * ) const;
  void store_brackets( frog_record&,
		       const icu::UnicodeString&,
		       const BracketNest * ) const;
  std::string MTreeFilename;
  Timbl::TimblAPI *MTree;
  std::vector<Rule*> analysis;
  std::string _version;
  std::string textclass;
  TiCC::LogStream *errLog;
  TiCC::LogStream *dbgLog;
  TiCC::UniFilter *filter;
  std::string _host;
  std::string _port;
  std::string _base;
  bool filter_diac;
  bool doDeepMorph;
};

#endif
