/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
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

#include <vector>
#include <string>
#include <iostream>

#include "timbl/TimblAPI.h"

#include "ticcutils/Configuration.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/FdStream.h"
#include "ticcutils/ServerBase.h"

#include "libfolia/folia.h"
#include "ucto/tokenize.h"

#include "frog/Frog-util.h"
#include "frog/FrogData.h"

class UctoTokenizer;
class Mbma;
class Mblem;
class Mwu;
class Parser;
class CGNTagger;
class IOBTagger;
class NERTagger;

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
  bool doTagger;
  bool doSentencePerLine;
  bool doQuoteDetection;
  bool doDirTest;
  bool doRetry;
  bool noStdOut;
  bool doXMLin;
  bool doXMLout;
  bool doServer;
  bool doKanon;
  bool test_API;
  bool hide_timers;
  int debugFlag;
  bool interactive;
  int numThreads;

  std::string encoding;
  std::string uttmark;
  std::string listenport;
  std::string docid;
  std::string inputclass;
  std::string outputclass;
  std::string default_language;
  std::set<std::string> languages;
  std::string textredundancy;
  unsigned int maxParserTokens;
  std::string command;

  FrogOptions();
 private:
  FrogOptions(const FrogOptions & );
};


class FrogAPI {
 public:
  FrogAPI( FrogOptions&,
	   const TiCC::Configuration&,
	   TiCC::LogStream *,
	   TiCC::LogStream * );
  ~FrogAPI();
  static std::string defaultConfigDir( const std::string& ="" );
  static std::string defaultConfigFile( const std::string& ="" );
  folia::Document *FrogFile( const std::string&, std::ostream& );
  void FrogServer( Sockets::ServerSocket &conn );
  void FrogInteractive();
  frog_data frog_sentence( std::vector<Tokenizer::Token>&,
			   const size_t );
  std::string Frogtostring( const std::string& );
  std::string Frogtostringfromfile( const std::string& );
  void run_api_tests( const std::string&, std::ostream& );
 private:
  folia::Document *run_folia_engine( const std::string&,
				     std::ostream& );
  folia::Document *run_text_engine( const std::string&,
				    std::ostream& );
  folia::FoliaElement* start_document( const std::string&,
				  folia::Document *& ) const;
  folia::FoliaElement *append_to_folia( folia::FoliaElement *,
					const frog_data&,
					unsigned int& ) const;
  void add_ner_result( folia::Sentence *,
		       const frog_data&,
		       const std::vector<folia::Word*>& ) const;
  void add_iob_result( folia::Sentence *,
		       const frog_data&,
		       const std::vector<folia::Word*>& ) const;
  void add_mwu_result( folia::Sentence *,
		       const frog_data&,
		       const std::vector<folia::Word*>& ) const;
  void add_parse_result( folia::Sentence *,
			 const frog_data&,
			 const std::vector<folia::Word*>& ) const;
  folia::processor *add_provenance( folia::Document& ) const;
  void test_version( const std::string&, double );
  // functions
  void FrogStdin( bool prompt );
  void output_tabbed( std::ostream&, const frog_record& ) const;
  void show_results( std::ostream&, const frog_data& ) const;
  void handle_one_paragraph( std::ostream&,
			     folia::Paragraph*,
			     int& );
  void handle_one_text_parent( std::ostream&,
			       folia::FoliaElement *e,
			       int&  );
  void handle_one_sentence( std::ostream&,
			    folia::Sentence *,
			    const size_t );
  void append_to_sentence( folia::Sentence *, const frog_data& ) const;
  void append_to_words( const std::vector<folia::Word*>&,
			const frog_data& ) const;

  // data
  const TiCC::Configuration& configuration;
  FrogOptions& options;
  TiCC::LogStream *theErrLog;
  TiCC::LogStream *theDbgLog;
  TimerBlock timers;
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

// the functions below here are ONLY used by TSCAN.
// the should be moved there probably
// =======================================================================

std::vector<std::string> get_full_morph_analysis( folia::Word *, bool = false );
std::vector<std::string> get_compound_analysis( folia::Word * );
//std::vector<std::string> get_full_morph_analysis( folia::Word *,
//						  const std::string&,
//						  bool = false );

#endif
