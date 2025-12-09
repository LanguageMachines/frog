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

/// \brief a virtual base class to add parser functionality. Needs specializions
/// for e.g running a CKY parser or Alpino
class ParserBase {
 public:
  explicit ParserBase( TiCC::LogStream* errlog, TiCC::LogStream* dbglog ):
    isInit( false ),
    filter( 0 )
  {
    errLog = new TiCC::LogStream(errlog);
    errLog->add_message("parser-");
    dbgLog = new TiCC::LogStream(dbglog);
    dbgLog->add_message("parser-dbg-");
  };
  virtual ~ParserBase();
  virtual bool init( const TiCC::Configuration& ) = 0;
  virtual void add_provenance( folia::Document& doc,
			       folia::processor * ) const =0;
  virtual void Parse( frog_data&, TimerBlock& ) = 0;
  virtual void add_result( const frog_data&,
			   const std::vector<folia::Word*>& ) const;
  std::vector<std::string> createParserInstances( const parseData& );
  const std::string& getTagset() const { return dep_tagset; };
  ParserBase( const ParserBase& ) = delete;
  ParserBase& operator=( const ParserBase& ) = delete;
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
};

/// \brief a specialization of ParserBase to run a CKY parser
class Parser: public ParserBase {
public:
  explicit Parser( TiCC::LogStream* errlog, TiCC::LogStream* dbglog ):
    ParserBase( errlog, dbglog ),
    maxDepSpan( 0 ),
    pairs(0),
    dir(0),
    rels(0) {};
  ~Parser() override;
  bool init( const TiCC::Configuration& ) override;
  void add_provenance( folia::Document& doc,
		       folia::processor * ) const override;
  parseData prepareParse( frog_data& );
  void Parse( frog_data&, TimerBlock& ) override;
 private:
  std::vector<icu::UnicodeString> createPairInstances( const parseData& );
  std::vector<icu::UnicodeString> createDirInstances( const parseData& );
  std::vector<icu::UnicodeString> createRelInstances( const parseData& );
  std::vector<timbl_result> timbl_server( const std::string&,
					  const std::vector<icu::UnicodeString>& );
  Parser( const Parser& ) = delete; // inhibit copies
  Parser operator=( const Parser& ) = delete; // inhibit copies
  std::string maxDepSpanS;
  size_t maxDepSpan;
  Timbl::TimblAPI *pairs;
  Timbl::TimblAPI *dir;
  Timbl::TimblAPI *rels;
  std::string _pairs_base;
  std::string _dirs_base;
  std::string _rels_base;
};

void appendParseResult( frog_data& fd,
			const std::vector<parsrel>& res );
#endif
