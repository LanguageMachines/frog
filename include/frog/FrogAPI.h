/* ex: set tabstop=8 expandtab: */
/*
  $Id: Frog.h 17614 2014-09-07 12:49:43Z mvgompel $
  $URL: https://ilk.uvt.nl/svn/sources/Frog/trunk/include/frog/Frog.h $

  Copyright (c) 2006 - 2014
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

  void Test( const std::string&, std::ostream&, const std::string& );
  void Test( folia::Document&, std::ostream&,
	     bool=false, const std::string& ="");
  bool TestSentence( folia::Sentence*, TimerBlock&);
  void TestServer( Sockets::ServerSocket &conn );
  void TestInteractive();
  std::string Testtostring( folia::Document*); //hack for cython, returns results as string

 private:
  // functions
  std::vector<folia::Word*> lookup( folia::Word *,
				    const std::vector<folia::Entity*>& );
  folia::Dependency *lookupDep( const folia::Word *,
				const std::vector<folia::Dependency*>& );
  std::string lookupNEREntity( const std::vector<folia::Word *>&,
			       const std::vector<folia::Entity*>& );
  std::string lookupIOBChunk( const std::vector<folia::Word *>&,
			      const std::vector<folia::Chunk*>& );
  void displayMWU( std::ostream&, size_t, const std::vector<folia::Word*>& );
  std::ostream & showResults( std::ostream&, const folia::Sentence*, bool );

  // data
  const TiCC::Configuration& configuration;
  const FrogOptions& options;
  TiCC::LogStream *theErrLog;

  // pointers to all the modules
  Mbma *myMbma;
  Mblem *myMblem;
  Mwu *myMwu;
  Parser *myParser;
  CGNTagger *myCGNTagger;
  IOBTagger *myIOBTagger;
  NERTagger *myNERTagger;
  UctoTokenizer *tokenizer;
};

#endif
