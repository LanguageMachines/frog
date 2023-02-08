/*
  Copyright (c) 2006 - 2023
  CLST  - Radboud University
  ILK   - Tilburg University

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
      https://github.com/LanguageMachines/timblserver/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/

#ifndef UCTO_TOKENIZER_MOD_H
#define UCTO_TOKENIZER_MOD_H

#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "frog/FrogData.h"
#include "ticcutils/Configuration.h"

/// \brief an Interface to the Ucto tokenizer
class UctoTokenizer {
 public:
  explicit UctoTokenizer( TiCC::LogStream *, TiCC::LogStream * =0 );
  ~UctoTokenizer();
  bool init( const TiCC::Configuration& );
  void setUttMarker( const std::string& );
  void setPassThru( bool );
  void set_TC_debug( bool );
  bool getPassThru() const;
  void setSentencePerLineInput( bool );
  void setInputEncoding( const std::string& );
  void setQuoteDetection( bool );
  void setInputXml( bool );
  void setFiltering( bool );
  void setInputClass( const std::string& );
  void setOutputClass( const std::string& );
  void setDocID( const std::string& );
  void setTextRedundancy( const std::string& );
  void setWordCorrection( bool );
  bool setUndLang( bool );
  bool setLangDetection( bool );
  bool getUndLang() const;
  std::string get_data_version() const;
  std::string default_language() const;
  bool get_setting_info( const std::string&, std::string&, std::string& ) const;
  std::vector<icu::UnicodeString> tokenize( const icu::UnicodeString&  );
  std::vector<Tokenizer::Token> tokenize_line( const std::string&, const std::string& = "" );
  std::vector<Tokenizer::Token> tokenize_line_next();
  std::vector<Tokenizer::Token> tokenize_stream( std::istream& );
  std::vector<Tokenizer::Token> tokenize_stream_next();
  icu::UnicodeString tokenizeStream( std::istream& );
  std::vector<folia::Word*> add_words( folia::Sentence *,
				       const frog_data& ) const;
  void add_provenance( folia::Document& , folia::processor * ) const;
  std::vector<Tokenizer::Token> correct_words( folia::FoliaElement *,
					       const std::vector<folia::Word*>& );
 private:
  std::istream *cur_is;
  Tokenizer::TokenizerClass *tokenizer;
  TiCC::LogStream *errLog;
  TiCC::LogStream *dbgLog;
  int debug;
  std::string textredundancy;
};

#endif
