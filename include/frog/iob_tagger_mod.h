/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2026
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

/// \brief a specialization of Basetagger to tag IOB tags
class IOBTagger: public BaseTagger {
 public:
  explicit IOBTagger( TiCC::LogStream *l, TiCC::LogStream *d =0 ):
  BaseTagger( l, d, "IOB" ){};
  bool init( const TiCC::Configuration& ) override;
  void add_declaration( folia::Document&, folia::processor * ) const override;
  void Classify( frog_data& ) override;
  void post_process( frog_data& ) override;
  void add_result( const frog_data& fd,
		   const std::vector<folia::Word*>& wv ) const;
 private:
  void addTag( frog_record&,
	       const icu::UnicodeString&,
	       double );
};

#endif // IOB_TAGGER_MOD_H
