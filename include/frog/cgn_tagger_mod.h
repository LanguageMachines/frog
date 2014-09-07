/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2014
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

#ifndef CGN_TAGGER_MOD_H
#define CGN_TAGGER_MOD_H

#include "mbt/MbtAPI.h"

class CGNTagger {
 public:
  CGNTagger(TiCC::LogStream*);
  ~CGNTagger();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& ) const;
  void addTag( folia::Word *, const std::string&, double );
  void Classify( const std::vector<folia::Word *>& );
  std::vector<TagResult> tagLine( const std::string& );
  std::string getTagset() const { return tagset; };
 private:
  MbtAPI *tagger;  
  LogStream *cgnLog;
  int debug;
  std::string tagset;
  std::string version;
  Tokenizer::UnicodeFilter *filter;
};

#endif // CGN_TAGGER_MOD_H
