/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2022
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
class ParserBase;
class CGNTagger;
class IOBTagger;
class NERTagger;

/// \brief this class holds the runtime settings for Frog
class FrogOptions {
 public:
  FrogOptions();             ///< the constructor
  bool doTok;                ///< should we run the tokenizer?
  bool doLemma;              ///< should we run the lemmatizer?
  bool doMorph;              ///< should we run the morphological analyzer?
  bool doDeepMorph;          ///< do we want a deep morphological analysis?
  bool doMwu;                ///< should we resolve Multi Word Units?
  bool doIOB;                ///< should we run the IOB Chuker?
  bool doNER;                ///< should we run the Named Entity recognizer?
  bool doParse;              ///< should we run the Dependency Parser?
  bool doTagger;             ///< should we run the Dependency Parser?
  bool doSentencePerLine;    ///< do we want a sentence per line?
  /*!< should we see every input-line as a separate
    sentence?
  */
  bool doQuoteDetection;     ///< enable quote detection (NOT USED)
  /*!< should we enable Quote Detection. This value MUST be false.
    Quote Detection is NOT supported
  */
  bool doRetry;              ///< set retry mode (MISNOMER)
  /*!< when TRUE, we assume we are retrying one ore more inputfiles, and
    we skip those input files for which an outputfile already exists.
  */
  bool noStdOut;             ///< do we want output to stdout?
  bool doXMLin;              ///< do we have FoLiA input?
  bool doXMLout;             ///< do we want FoLiA output?
  bool wantOUT;              ///< do we want ANY output?
  bool doJSONin;             ///< do we have JSON input?
  /*!< This is only supported for the Server mode of Frog,
    it implies JSON output too.
   */
  bool doJSONout;            ///< do we want JSON output?
  bool doServer;             ///< do we want to run as a server?
  /*!< currently only TCP servers are supported
   */
  bool doKanon;              ///< do we want FoLiA to be output in a canonical way?
  /*!< This can be conveniant for testing purposes as it makes sure that nodes
    from several modules are always in the same order in the XML
   */
  bool test_API;             ///< do we want to run some tests?
  /*!< This will run some generic tests and then stop.
    No real frogging is done!
  */
  bool hide_timers;          ///< should we output timing information?
  /*!< normaly Frog outputs timing information for the several modules,
    but it may be usefull to skip that
  */
  bool interactive;         ///< are we running from the command line?
  bool doAlpinoServer;      ///< should we try to connect to an Alpino server?
  /*!< this assumes that an Alpino Server is set up and running and that it's
    location is configured correctly.
   */
  bool doAlpino;            ///< should we directly run Alpino?
  /*!< This assumes that Alpinois installed locally and the Alpino command is
    working.
   */
  int numThreads;           ///< limit for the number of threads
  int debugFlag;            ///< value for the generic debug level
  /*!< This value is used as the debug level for EVERY module.
    It is however possible to set specific levels per module too.
   */
  int JSON_pp;              ///< for JSON output, use this value to format.
  /*!< normally JSON will be outputted as one (very long) line.
    Using a value of JSON_pp >0 it wil be 'pretty-printed' indented with
    that value.
   */
  std::string encoding;     ///< which input-encoding do we expect
  /*!< using the capabilities of the Ucto tokenizer, Frog can handle a lot
of input encodings. The default is UTF8. The output will always be in UTF8.
   */
  std::string uttmark;     ///< the string which separates Utterances
  std::string listenport;  ///< determines the port to run the Frog Server on
  std::string docid;       ///< the FoLiA document ID on output.
  std::string inputclass;  ///< the textclass to use on FoLiA input
  std::string outputclass; ///< the textclass to use on FoLiA output
  std::string default_language; ///< what is our default language
  std::set<std::string> languages; ///< all languages to take into account
  /*< This set of languages will be handled over to the ucto tokenizer
   */
  std::string textredundancy; ///< determines how much text is added in the FoLiA
  /*!< possible values are 'full', 'minimal' and 'none'.

    'none': no text (\<t\>) nodes are added to higher structure nodes like
    \<s\> and \<p\>.

    'minimal': text is added to the structure above \<w\>. Mostly \<s\>
    nodes

    'full': text is added to all structure nodes. This might result in a
    lot of (redundant) text.

   */
  bool correct_words;      ///< should we allow the tokenizer to correct words?
  /*!< When true, the tokenizer might split words changing the number of words
    and the text value of the above structure(s). e.g '1984!' to '1984 !'
   */
  unsigned int maxParserTokens;  ///< limit the number of words to Parse
  /*< The Parser may 'explode' on VERY long sentences. So we limit it to a
maximum of 500 words PER SENTENC. Which is already a lot!
   */
  std::set<std::string> fileNames; ///< the filenames as parsed from the commandline
  std::string testDirName;    ///< the name of the directory with testfiles
  std::string xmlDirName;     ///< the name of the directory to store FoLia results in
  std::string outputDirName;  ///< the name of the output directory for tabbed or JSON
  std::string outputFileName; ///< the name of a single tabbed/JSON outputfile
  std::string XMLoutFileName; ///< the name fo a single FoLiA outputfile
  std::string command;        ///< the original command that invoked Frog

 private:
  FrogOptions( const FrogOptions & );
};

/// \brief This is the API class which can be used to set up Frog and run it
/// on files, strings, TCP sockets or a terminal.
class FrogAPI {
 public:
  FrogAPI( TiCC::CL_Options&,
	   TiCC::LogStream *,
	   TiCC::LogStream * );
  ~FrogAPI();
  void run_api( const TiCC::Configuration& );
  void run_on_files();
  bool run_a_server();
  void run_interactive();
  void run_api_tests( const std::string& );
  std::string Frogtostring( const std::string& );
  std::string Frogtostringfromfile( const std::string& );
  static std::string defaultConfigFile( const std::string& ="" );

 private:
  static std::string defaultConfigDir( const std::string& ="" );
  bool collect_options( TiCC::CL_Options&,
			TiCC::Configuration&,
			TiCC::LogStream* );
  folia::Document *FrogFile( const std::string& );
  void FrogServer( Sockets::ClientSocket &conn );

  frog_data frog_sentence( std::vector<Tokenizer::Token>&,
			   const size_t );
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
  void test_version( const TiCC::Configuration&, const std::string&, double );
  // functions
  void FrogStdin( bool prompt );
  void output_tabbed( std::ostream&,
		      const frog_record& ) const;
  void output_JSON( std::ostream& os,
		    const frog_data& fd,
		    int = 0 ) const;
  void show_results( std::ostream&,
		     const frog_data& ) const;
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
  void handle_word_vector( std::ostream&,
			   const std::vector<folia::Word*>&,
			   const size_t );
  // data
 public:
  FrogOptions options;                     ///< all runtime options
  std::ostream *outS;
 private:
  TiCC::LogStream *theErrLog;               ///< the stream to send errors to
  TiCC::LogStream *theDbgLog;               ///< the stream to send debug info
  TimerBlock timers;                        ///< all runtime timers
  Mbma *myMbma;             ///< pointer to the MBMA module
  Mblem *myMblem;           ///< pointer to the MBLEM module
  Mwu *myMwu;               ///< pointer to the MWU module
  ParserBase *myParser;     ///< pointer to the CKY parser module
  CGNTagger *myCGNTagger;   ///< pointer to the CGN tagger
  IOBTagger *myIOBTagger;   ///< pointer to the IOB chunker
  NERTagger *myNERTagger;   ///< pointer to the NER
  UctoTokenizer *tokenizer; ///< pointer to the Ucot tokenizer
};

std::vector<std::string> get_full_morph_analysis( folia::Word *word,
						  bool make_flat=false );
std::vector<std::string> get_compound_analysis( folia::Word *word );

const std::string& get_mbma_tagset( const std::string& );

#endif
