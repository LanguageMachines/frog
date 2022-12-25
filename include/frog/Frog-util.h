/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2023
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of frog:

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for
  several languages

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
      https://github.com/LanguageMachines/frog/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/

#ifndef FROG_H
#define FROG_H

#include <set>
#include <ostream>
#include <fstream>
#include <string>

#include "ticcutils/Timer.h"
#include "ticcutils/Unicode.h"

std::string prefix( const std::string& path,
		    const std::string& fn );

std::set<std::string> getFileNames( const std::string& dirName,
				    const std::string& ext );

std::string check_server( const std::string& host,
			  const std::string& port,
			  const std::string& name= "" );

/// \brief a collection of Ticc:Timers that registrate timings per module
class TimerBlock{
public:
  TiCC::Timer parseTimer;
  TiCC::Timer tokTimer;
  TiCC::Timer mblemTimer;
  TiCC::Timer mbmaTimer;
  TiCC::Timer mwuTimer;
  TiCC::Timer tagTimer;
  TiCC::Timer iobTimer;
  TiCC::Timer nerTimer;
  TiCC::Timer prepareTimer;
  TiCC::Timer pairsTimer;
  TiCC::Timer relsTimer;
  TiCC::Timer dirTimer;
  TiCC::Timer csiTimer;
  TiCC::Timer frogTimer;
  void reset(){
    parseTimer.reset();
    tokTimer.reset();
    mblemTimer.reset();
    mbmaTimer.reset();
    mwuTimer.reset();
    tagTimer.reset();
    iobTimer.reset();
    nerTimer.reset();
    prepareTimer.reset();
    pairsTimer.reset();
    relsTimer.reset();
    dirTimer.reset();
    csiTimer.reset();
    frogTimer.reset();
  }
};

#endif
