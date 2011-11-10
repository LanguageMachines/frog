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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include "config.h"
#include "timbl/TimblAPI.h"
#include "frog/Frog.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

using namespace std;

string prefix( const string& path, const string& fn ){
  if ( fn.find( "/" ) == string::npos ){
    // only append prefix when NO path is specified
    return path + fn;
  }
  return fn;
}

#ifdef HAVE_DIRENT_H
bool existsDir( const string& dirName ){
  bool result = false;
  DIR *dir = opendir( dirName.c_str() );
  if ( dir ){
    result = (closedir( dir ) == 0);
  }
  return result;
}

void getFileNames( const string& dirName, set<string>& fileNames ){
  DIR *dir = opendir( dirName.c_str() );
  if ( !dir )
    return;
  else {
    struct stat sb;
    struct dirent *entry = readdir( dir );
    while ( entry ){
      if (entry->d_name[0] != '.') {
        string fullName = dirName + "/" + entry->d_name;
        if ( stat( fullName.c_str(), &sb ) >= 0 ){
            if ( (sb.st_mode & S_IFMT) == S_IFREG )
                fileNames.insert( entry->d_name );
        }
       }
       entry = readdir( dir );
    }
  }
}

#endif

string& trim(string &str)  //3rd party function. TODO: test if utf-8 safe? (MvG)
{
    int i,j,start,end;

    //ltrim
    for (i=0; (str[i]!=0 && str[i]<=32); )
        i++;
    start=i;

    //rtrim
    for(i=0,j=0; str[i]!=0; i++)
        j = ((str[i]<=32)? j+1 : 0);
    end=i-j;
    str = str.substr(start,end-start);
    return str;
}

string escape( const string& in ){
  string out;
  for ( size_t i=0; i < in.length(); ++i ){
    if ( in[i] == '\'' )
      out += "\\";
    out += in[i];
  }
  return out;
}

