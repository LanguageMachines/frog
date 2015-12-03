/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
  Tilburg University

  This file is part of frog.

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
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
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
  Mbma(TiCC::LogStream *);
  ~Mbma();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& doc ) const;
  void Classify( folia::Word * );
  void Classify( const UnicodeString& );
  void filterHeadTag( const std::string& );
  void filterSubTags( const std::vector<std::string>& );
  std::vector<std::vector<std::string> > getResult( bool = true ) const;
  void setDaring( bool b ){ doDaring = b; };
  void clearAnalysis();
  std::string getTagset() const { return mbma_tagset; };
  Rule* matchRule( const std::vector<std::string>&, const UnicodeString& );
  std::vector<Rule*> execute( const UnicodeString& ,
			      const std::vector<std::string>& );
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
  std::map<std::string,std::string> TAGconv;
  std::vector<Rule*> analysis;
  std::string version;
  std::string mbma_tagset;
  std::string cgn_tagset;
  std::string clex_tagset;
  TiCC::LogStream *mbmaLog;
  Transliterator *transliterator;
  Tokenizer::UnicodeFilter *filter;
  bool doDaring;
};

#endif
