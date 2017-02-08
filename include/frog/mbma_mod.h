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

#ifndef MBMA_MOD_H
#define MBMA_MOD_H

#include <unicode/translit.h>
#include "ticcutils/LogStream.h"
#include "frog/clex.h"
#include "frog/mbma_rule.h"
#include "frog/mbma_brackets.h"

class MBMAana;
namespace Timbl{
  class TimblAPI;
}

namespace Tokenizer {
  class UnicodeFilter;
}

namespace TiCC {
  class Configuration;
}

namespace folia {
  class Word;
  class Document;
  class MorphologyLayer;
  class Morpheme;
}

class Mbma {
 public:
 explicit Mbma( TiCC::LogStream * );
  ~Mbma();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& doc ) const;
  void Classify( folia::Word * );
  void Classify( const UnicodeString& );
  void filterHeadTag( const std::string& );
  void filterSubTags( const std::vector<std::string>& );
  void assign_compounds();
  std::vector<std::string> getResult() const;
  std::vector<std::pair<std::string,std::string>> getResults( ) const;
  void setDeepMorph( bool b ){ doDeepMorph = b; };
  void clearAnalysis();
  Rule* matchRule( const std::vector<std::string>&, const UnicodeString& );
  std::vector<Rule*> execute( const UnicodeString& ,
			      const std::vector<std::string>& );
  static std::map<std::string,std::string> TAGconv;
  static std::string mbma_tagset;
  static std::string cgn_tagset;
  static std::string clex_tagset;
 private:
  void cleanUp();
  bool readsettings( const std::string&, const std::string& );
  void fillMaps();
  void init_cgn( const std::string&, const std::string& );
  Transliterator * init_trans();
  UnicodeString filterDiacritics( const UnicodeString& ) const;
  void getFoLiAResult( folia::Word *, const UnicodeString& ) const;
  std::vector<std::string> make_instances( const UnicodeString& word );
  CLEX::Type getFinalTag( const std::list<BaseBracket*>& );
  int debugFlag;
  void addMorph( folia::MorphologyLayer *,
		 const std::vector<std::string>& ) const;
  void addMorph( folia::Word *, const std::vector<std::string>& ) const;
  void addBracketMorph( folia::Word *,
			const std::string&,
			const std::string& ) const;
  void addBracketMorph( folia::Word *,
			const std::string&,
			const BracketNest * ) const;
  std::string MTreeFilename;
  Timbl::TimblAPI *MTree;
  std::vector<Rule*> analysis;
  std::string version;
  std::string textclass;
  TiCC::LogStream *mbmaLog;
  Transliterator *transliterator;
  Tokenizer::UnicodeFilter *filter;
  bool doDeepMorph;
};

#endif
