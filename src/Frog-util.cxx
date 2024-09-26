/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2024
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

#include "frog/Frog-util.h"

#include <cstring>
#include <set>
#include <string>
#include <ostream>
#include <fstream>
#include <filesystem>
#include "ticcutils/SocketBasics.h"
#include "ticcutils/FileUtils.h"
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

string prefix( const string& path, const string& fn ){
  /// add a full path to the filename \e fn
  /*!
    \param path the path to add
    \param fn the filename to be prefixed
    \return a new path
    If \e fn already contains (relative) path information, it is left unchanged.
   */
  if ( fn.find( "/" ) == string::npos && !path.empty() ){
    // only append prefix when it isn't empty AND
    // NO path is specified in the filename
    return path + "/" + fn;
  }
  return fn;
}

set<string> getFileNames( const string& dir_name,
			  const string& ext ){
  /// extract a (sorted) list of file-names matching an extension pattern
  /*!
    \param dir_name the search directory
    \param ext the file extension we search
    \return a sorted list of file-names
  */
  set<string> result;
  filesystem::path path( dir_name );
  for( auto const& entry : filesystem::directory_iterator(path) ){
    string val = entry.path().filename();
    if ( ext.empty() ||
	 val.rfind( ext ) != string::npos ) {
      result.insert(val);
    }
  }
  return result;
}

string check_server( const string& host,
		     const string& port,
		     const string& name ){
  /// check if a certain host:port is available for us
  /*!
    \param host the host we check
    \param port the port we want to access
    \param name extra information (used in diagnostics only)
    \return "" on succes or an error message on failure
  */
  string outline;
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    outline = "cannot open connection, " + host + ":" + port;
    if ( !name.empty() ){
      outline += " for " + name + " module";
    }
    outline += "\nmessage: (" + client.getMessage() + ")" ;
  }
  return outline;
}
