/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2017
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of frog

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
      https://github.com/LanguageMachines/timblserver/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/


#ifndef FROGAPI_H
#define FROGAPI_H

#include "timbl/TimblAPI.h"

#include "frog/Frog.h" //internals
#include "ticcutils/Configuration.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/FdStream.h"
#include "ticcutils/ServerBase.h"

#include <vector>
#include <string>
#include <iostream>

class UctoTokenizer;
class Mbma;
class Mblem;
class Mwu;
class Parser;
class CGNTagger;
class IOBTagger;
class EIOBTagger;
class NERTagger;
class ENERTagger;

class FrogOptions {
 public:
  bool doTok;
  bool doLemma;
  bool doMorph;
  bool doDeepMorph;
  bool doMwu;
  bool doIOB;
  bool doNER;
  bool doParse;
  bool doSentencePerLine;
  bool doQuoteDetection;
  bool doDirTest;
  bool doRetry;
  bool noStdOut;
  bool doXMLin;
  bool doXMLout;
  bool doServer;
  bool doKanon;

  int debugFlag;
  bool interactive;
  int numThreads;

  std::string encoding;
  std::string uttmark;
  std::string listenport;
  std::string docid;
  std::string inputclass;
  std::string outputclass;
  std::string language;
  std::string textredundancy;

  unsigned int maxParserTokens;

  FrogOptions();
 private:
  FrogOptions(const FrogOptions & );
};


class FrogAPI {
 public:
  FrogAPI( FrogOptions&,
	   const TiCC::Configuration&,
	   TiCC::LogStream * );
  ~FrogAPI();
  static std::string defaultConfigDir( const std::string& ="" );
  static std::string defaultConfigFile( const std::string& ="" );
  void FrogFile( const std::string&, std::ostream&, const std::string& );
  void FrogDoc( folia::Document&, bool=false );
  void FrogServer( Sockets::ServerSocket &conn );
  void FrogInteractive();
  std::string Frogtostring( const std::string& );
  std::string Frogtostringfromfile( const std::string& );

 private:
  void test_version( const std::string&, double );
  // functions
  bool TestSentence( folia::Sentence*, TimerBlock&);
  void FrogStdin( bool prompt );
  std::vector<folia::Word*> lookup( folia::Word *,
				    const std::vector<folia::Entity*>& ) const;
  folia::Dependency *lookupDep( const folia::Word *,
				const std::vector<folia::Dependency*>& ) const;
  std::string lookupNEREntity( const std::vector<folia::Word *>&,
			       const std::vector<folia::Entity*>& ) const;
  std::string lookupIOBChunk( const std::vector<folia::Word *>&,
			      const std::vector<folia::Chunk*>& ) const;
  void displayMWU( std::ostream&, size_t, const std::vector<folia::Word*>& ) const;
  std::ostream& showResults( std::ostream&, folia::Document& ) const;

  // data
  const TiCC::Configuration& configuration;
  FrogOptions& options;
  TiCC::LogStream *theErrLog;
  TimerBlock timers;
  // pointers to all the modules
  Mbma *myMbma;
  Mblem *myMblem;
  Mwu *myMwu;
  Parser *myParser;
  CGNTagger *myCGNTagger;
#ifdef ENR_IOB
  EIOBTagger *myIOBTagger;
#else
  IOBTagger *myIOBTagger;
#endif
  ENERTagger *myNERTagger;
  UctoTokenizer *tokenizer;
};

std::vector<std::string> get_full_morph_analysis( folia::Word *, bool = false );
std::vector<std::string> get_full_morph_analysis( folia::Word *,
						  const std::string&,
						  bool = false );
std::vector<std::string> get_compound_analysis( folia::Word * );

#endif
