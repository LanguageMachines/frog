/*
  Copyright (c) 2006 - 2019
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
  std::vector<std::string> tokenize( const std::string&  );
  frog_data tokenize_line( const std::string& );
  frog_data tokenize_stream( std::istream& );
  frog_data tokenize_stream_next();
  std::string tokenizeStream( std::istream& );
  std::vector<folia::Word*> add_words( folia::Sentence *,
				       const std::string&,
				       const std::string&,
				       const frog_data& ) const;
 private:
  std::list<Tokenizer::Token> queue;  // for the reentrant tokenize_stream()
  std::istream *cur_is;
  Tokenizer::TokenizerClass *tokenizer;
  TiCC::LogStream *errLog;
  TiCC::LogStream *dbgLog;
  int debug;
  std::string textredundancy;
};

#endif
