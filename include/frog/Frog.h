/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2011
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


#ifndef __DEMO_OPTIONS__
#define __DEMO_OPTIONS__

#define MAX_NAMELEN 2048 

#include <set>
#include "timbl/LogStream.h"
#include "libfolia/foliautils.h"
#include "frog/Configuration.h"

std::string prefix( const std::string&, const std::string& );
bool existsDir( const std::string& );
std::string& trim(std::string &str);

void getFileNames( const std::string&, std::set<std::string>& );

extern LogStream *theErrLog;

extern int tpDebug;
extern bool keepIntermediateFiles;
extern bool doServer;
extern Configuration configuration;

class TimerBlock{
public:
  Common::Timer parseTimer;
  Common::Timer tokTimer;
  Common::Timer mblemTimer;
  Common::Timer mbmaTimer;
  Common::Timer mwuTimer;
  Common::Timer tagTimer;
  Common::Timer prepareTimer;
  Common::Timer pairsTimer;
  Common::Timer relsTimer;
  Common::Timer dirTimer;
  Common::Timer csiTimer;
};

#endif
