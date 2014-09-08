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

extern TiCC::LogStream *theErrLog;

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

  std::string encoding;
  std::string uttmark;
  std::string listenport;
  std::string docid;
  std::string textclass;

  std::string tmpDirName;

  unsigned int maxParserTokens;

  FrogOptions();
  FrogOptions(const FrogOptions & );
};


class FrogAPI {
 protected:
  std::vector<folia::Word*> lookup( folia::Word *word, const std::vector<folia::Entity*>& entities );
  folia::Dependency * lookupDep( const folia::Word *word, const std::vector<folia::Dependency*>&dependencies );
  std::string lookupNEREntity( const std::vector<folia::Word *>& mwu, const std::vector<folia::Entity*>& entities);
  std::string lookupIOBChunk( const std::vector<folia::Word *>& mwu, const std::vector<folia::Chunk*>& chunks );
  void displayMWU( std::ostream& os, size_t index, const std::vector<folia::Word*> mwu);
  std::ostream & showResults( std::ostream& os, const folia::Sentence* sentence, bool showParse);
 public:
  Mbma * myMbma;
  Mblem * myMblem;
  Mwu * myMwu;
  Parser * myParser;
  CGNTagger * myCGNTagger;
  IOBTagger * myIOBTagger;
  NERTagger * myNERTagger;
  UctoTokenizer * tokenizer;

  TiCC::LogStream * theErrLog;
  TiCC::Configuration * configuration;
  FrogOptions * options;

        FrogAPI(FrogOptions * options, TiCC::Configuration * configuration, TiCC::LogStream *);
        ~FrogAPI();
        void Test( const std::string& infilename, std::ostream &os, const std::string& xmlOutF);
        void Test( folia::Document& doc, std::ostream& outStream,  bool hidetimers = false, const std::string& xmlOutFile = "");
        std::string Test( folia::Document& doc, bool hidetimers = true); //returns results as string
        bool TestSentence( folia::Sentence* sent, TimerBlock& timers);
    
};

#endif
