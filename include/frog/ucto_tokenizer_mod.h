/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2012
  Tilburg University
  
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
                                    
#ifndef UCTO_TOKENIZER_MOD_H
#define UCTO_TOKENIZER_MOD_H

#include "libfolia/folia.h"
#include "libfolia/document.h"
#include "ucto/tokenize.h"

class Configuration;

class UctoTokenizer {
 public:
  UctoTokenizer();
  ~UctoTokenizer() { delete tokenizer; delete uctoLog; };
  bool init( const Configuration&, const std::string&, bool );
  void setUttMarker( const std::string& );
  void setSentencePerLineInput( bool );
  void setInputEncoding( const std::string& );
  void setQuoteDetection( bool );
  void setInputXml( bool );
  folia::Document tokenize( std::istream& );
  bool tokenize( folia::Document& );
  std::vector<std::string> tokenize( const std::string&  );
 private:
  Tokenizer::TokenizerClass *tokenizer;
  TiCC::LogStream *uctoLog;
};

#endif 
