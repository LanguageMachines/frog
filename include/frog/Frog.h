/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2012
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


#ifndef FROG_H
#define FROG_H

#include <set>
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "libfolia/foliautils.h"

std::string prefix( const std::string&, const std::string& );
bool existsDir( const std::string& );
std::string& trim(std::string &str);
std::string getTime();

void getFileNames( const std::string&, const std::string&, 
		   std::set<std::string>& );

extern TiCC::LogStream *theErrLog;

extern int tpDebug;
extern TiCC::Configuration configuration;

class TimerBlock{
public:
  Common::Timer parseTimer;
  Common::Timer tokTimer;
  Common::Timer mblemTimer;
  Common::Timer mbmaTimer;
  Common::Timer mwuTimer;
  Common::Timer tagTimer;
  Common::Timer iobTimer;
  Common::Timer nerTimer;
  Common::Timer prepareTimer;
  Common::Timer pairsTimer;
  Common::Timer relsTimer;
  Common::Timer dirTimer;
  Common::Timer csiTimer;
  Common::Timer frogTimer;
};

#endif
