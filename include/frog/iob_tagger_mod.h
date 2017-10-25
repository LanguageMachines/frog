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

#ifndef IOB_TAGGER_MOD_H
#define IOB_TAGGER_MOD_H

#include "frog/tagger_base.h"

class IOBTagger: public BaseTagger {
 public:
  explicit IOBTagger( TiCC::LogStream *l ): BaseTagger( l, "IOB" ){};
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& ) const;
  void Classify( const std::vector<folia::Word*>& );
  void post_process( const std::vector<folia::Word*>& );
 private:
  void addChunk( folia::ChunkingLayer *,
		 const std::vector<folia::Word*>&,
		 const std::vector<double>&,
		 const std::string&,
		 const std::string& );
  void addIOBTags( const std::vector<folia::Word*>&,
		   const std::vector<std::string>&,
		   const std::vector<double>& );
};

#endif // IOB_TAGGER_MOD_H
