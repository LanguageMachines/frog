/* ex: set tabstop=8 expandtab: */
/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
  Tilburg University

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
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/


#ifndef FROGAPI_H
#define FROGAPI_H

#include "timbl/TimblAPI.h"

#include "frog/Frog.h" //internals
#include "ticcutils/Configuration.h"
#include "ticcutils/LogStream.h"
#include "timblserver/FdStream.h"
#include "timblserver/ServerBase.h"

#include "frog/ucto_tokenizer_mod.h"
#include "frog/mbma_mod.h"
#include "frog/mblem_mod.h"
#include "frog/mwu_chunker_mod.h"
#include "frog/cgn_tagger_mod.h"
#include "frog/iob_tagger_mod.h"
#include "frog/ner_tagger_mod.h"
#include "frog/Parser.h"

#include <vector>
#include <string>
#include <iostream>

class FrogOptions {
 public:
  bool doTok;
  bool doLemma;
  bool doMorph;
  bool doDaringMorph;
  bool doMwu;
  bool doIOB;
  bool doNER;
  bool doParse;
  bool doSentencePerLine;
  bool doQuoteDetection;
  bool doDirTest;
  bool doServer;

  bool doXMLin;
  bool doXMLout;
  bool doKanon;

  int debugFlag;
  bool interactive;
  int numThreads;

  std::string encoding;
  std::string uttmark;
  std::string listenport;
  std::string docid;
  std::string textclass;

  std::string tmpDirName;

  unsigned int maxParserTokens;

  FrogOptions();
 private:
  FrogOptions(const FrogOptions & );
};


class FrogAPI {
 public:
  FrogAPI( const FrogOptions&,
	   const TiCC::Configuration&,
	   TiCC::LogStream * );
  ~FrogAPI();
  static std::string defaultConfigDir();
  static std::string defaultConfigFile();
  void FrogFile( const std::string&, std::ostream&, const std::string& );
  void FrogDoc( folia::Document&, bool=false );
  void FrogServer( Sockets::ServerSocket &conn );
  void FrogInteractive();
  std::string Frogtostring( const std::string& );
  std::string Frogtostringfromfile( const std::string& );

 private:
  // functions
  bool TestSentence( folia::Sentence*, TimerBlock&);
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
  const FrogOptions& options;
  TiCC::LogStream *theErrLog;

  // pointers to all the modules
  Mbma *myMbma;
  Mblem *myMblem;
  Mwu *myMwu;
  Parser *myParser;
  POSTagger *myCGNTagger;
  IOBTagger *myIOBTagger;
  NERTagger *myNERTagger;
  UctoTokenizer *tokenizer;
};

#endif
