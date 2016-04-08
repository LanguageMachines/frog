/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2016
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

#include <set>
#include <string>
#include "config.h"
#include "frog/Frog.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

using namespace std;

string prefix( const string& path, const string& fn ){
  if ( fn.find( "/" ) == string::npos && !path.empty() ){
    // only append prefix when it isn't empty AND
    // NO path is specified in the filename
    return path + "/" + fn;
  }
  return fn;
}

#ifdef HAVE_DIRENT_H
void getFileNames( const string& dirName,
		   const string& ext,
		   set<string>& fileNames ){
  DIR *dir = opendir( dirName.c_str() );
  if ( !dir )
    return;
  else {
    struct stat sb;
    struct dirent *entry = readdir( dir );
    while ( entry ){
      if ( entry->d_name[0] != '.' ) {
	string filename = entry->d_name;
	if ( ext.empty() ||
	     filename.rfind( ext ) != string::npos ) {
	  string fullName = dirName + "/" + filename;
	  if ( stat( fullName.c_str(), &sb ) >= 0 ){
            if ( (sb.st_mode & S_IFMT) == S_IFREG )
	      fileNames.insert( entry->d_name );
	  }
	}
      }
      entry = readdir( dir );
    }
    closedir( dir );
  }
}

#endif

string getTime() {
  time_t Time;
  time(&Time);
  tm curtime;
  localtime_r(&Time,&curtime);
  char buf[256];
  strftime( buf, 100, "%Y-%m-%dT%X", &curtime );
  string res = buf;
  return res;
}
