/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2020
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
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "timbl/TimblAPI.h"
#include "frog/FrogData.h"
#include "frog/ckyparser.h" // only for struct parsrel....

struct parseData;
class TimerBlock;
class timbl_result;

class ParserBase {
 public:
  explicit ParserBase( TiCC::LogStream* errlog, TiCC::LogStream* dbglog ):
  isInit( false ),
    filter( 0 )
      {
	errLog = new TiCC::LogStream(errlog, "parser-");
	dbgLog = new TiCC::LogStream(dbglog, "parser-dbg-");
      };
  virtual ~ParserBase();
  virtual bool init( const TiCC::Configuration& ) = 0;
  virtual void add_provenance( folia::Document& doc,
			       folia::processor * ) const =0;
  virtual void Parse( frog_data&, TimerBlock& ) = 0;
  virtual void add_result( const frog_data&,
			   const std::vector<folia::Word*>& ) const;
  std::vector<std::string> createParserInstances( const parseData& );
  std::string getTagset() const { return dep_tagset; };
 protected:
  bool isInit;
  TiCC::LogStream *errLog;
  TiCC::LogStream *dbgLog;
  std::string _version;
  std::string dep_tagset;
  std::string POS_tagset;
  std::string MWU_tagset;
  std::string textclass;
  TiCC::UniFilter *filter;
  std::string _host;
  std::string _port;
  ParserBase( const ParserBase& ){}; // inhibit copies
};

class Parser: public ParserBase {
 public:
  explicit Parser( TiCC::LogStream* errlog, TiCC::LogStream* dbglog ):
  ParserBase( errlog, dbglog ),
    maxDepSpan( 0 ),
    pairs(0),
    dir(0),
    rels(0) {};
  ~Parser();
  bool init( const TiCC::Configuration& );
  void add_provenance( folia::Document& doc, folia::processor * ) const;
  parseData prepareParse( frog_data& );
  void Parse( frog_data&, TimerBlock& );
 private:
  std::vector<std::string> createPairInstances( const parseData& );
  std::vector<std::string> createDirInstances( const parseData& );
  std::vector<std::string> createRelInstances( const parseData& );
  void timbl_server( const std::string&,
		     const std::vector<std::string>&,
		     std::vector<timbl_result>& );
  std::string maxDepSpanS;
  size_t maxDepSpan;
  Timbl::TimblAPI *pairs;
  Timbl::TimblAPI *dir;
  Timbl::TimblAPI *rels;
  std::string _pairs_base;
  std::string _dirs_base;
  std::string _rels_base;
};

void appendParseResult( frog_data&, const std::vector<parsrel>& );
#endif
