/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
  Tilburg University

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for Dutch

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

#ifndef PARSER_H
#define PARSER_H

struct parseData;

class Parser {
 public:
  Parser(TiCC::LogStream* logstream):pairs(0),dir(0),rels(0),isInit(false),keepIntermediate(false) {
    parseLog = new TiCC::LogStream(logstream, "parser-");
  };
  ~Parser();
  bool init( const TiCC::Configuration& );
  void addDeclaration( folia::Document& doc ) const;
  void Parse( const std::vector<folia::Word *>&,
	      const std::string&, const std::string&, TimerBlock& );
  void prepareParse( const std::vector<folia::Word *>&,
		     const std::string&, parseData& );
  void createParserFile( const parseData& );
  std::string getTagset() const { return dep_tagset; };
 private:
  void createPairInstances( const parseData&,
			    std::vector<std::string>& );
  void createDirRelInstances( const parseData&,
			      std::vector<std::string>&,
			      std::vector<std::string>& );
  Timbl::TimblAPI *pairs;
  Timbl::TimblAPI *dir;
  Timbl::TimblAPI *rels;
  std::string maxDepSpanS;
  size_t maxDepSpan;
  bool isInit;
  std::string fileName;
  TiCC::LogStream *parseLog;
  bool keepIntermediate;
  bool oldparser;
  std::string version;
  std::string dep_tagset;
  std::string POS_tagset;

};


#endif
