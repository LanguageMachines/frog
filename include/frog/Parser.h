/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2018
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

#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <set>
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "timbl/TimblAPI.h"

struct parseData;
class TimerBlock;

class Parser {
 public:
  explicit Parser( TiCC::LogStream* logstream ):
  pairs(0),
    dir(0),
    rels(0),
    maxDepSpan( 0 ),
    isInit( false ),
    filter( 0 )
      {
	parseLog = new TiCC::LogStream(logstream, "parser-");
      };
  ~Parser();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& doc ) const;
  void Parse( const std::vector<folia::Word *>&, TimerBlock& );
  parseData prepareParse( const std::vector<folia::Word *>& );

  std::vector<std::string> createParserInstances( const parseData& );
  std::string getTagset() const { return dep_tagset; };
 private:
  std::vector<std::string> createPairInstances( const parseData& );
  std::vector<std::string> createDirInstances( const parseData& );
  std::vector<std::string> createRelInstances( const parseData& );

  Timbl::TimblAPI *pairs;
  Timbl::TimblAPI *dir;
  Timbl::TimblAPI *rels;
  std::string maxDepSpanS;
  size_t maxDepSpan;
  bool isInit;
  TiCC::LogStream *parseLog;
  std::string version;
  std::string dep_tagset;
  std::string POS_tagset;
  std::string MWU_tagset;
  std::string textclass;
  Tokenizer::UnicodeFilter *filter;
  Parser( const Parser& ){}; // inhibit copies
};


#endif
