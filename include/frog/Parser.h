/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2011
  Tilburg University

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for Dutch
 
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

#ifndef PARSER_H
#define PARSER_H

class Configuration;

class PyObjectRef {
 private:
  PyObject* ref;
  
 public:
 PyObjectRef() : ref(0) {}
  
 PyObjectRef(PyObject* obj) : ref(obj) {}
  
  void assign(PyObject* obj) {
    // assume ref == 0, otherwhise call Py_DECREF
    if ( ref ){
      Py_XDECREF(ref);
    }
    ref = obj;
  }
  
  operator PyObject* () {
    return ref;
  }
  
  ~PyObjectRef() {
    Py_XDECREF(ref);
  }
};

class PythonInterface {
 private:
  PyObjectRef module;
  PyObjectRef mainFunction;
  
 public:
  PythonInterface();
  ~PythonInterface();
  
  void parse( const std::string& depFile,
	      const std::string& modFile,
	      const std::string& dirFile,
	      const std::string& maxDist,
	      const std::string& inputFile,
	      const std::string& outputFile);
};  

struct parseData;

class Parser {
 public:
 Parser():pairs(0),dir(0),rels(0),PI(0),isInit(false){};
  ~Parser();
  bool init( const Configuration& );
  void Parse( FrogData *, folia::AbstractElement *, const std::string&, TimerBlock& );
  void prepareParse( folia::AbstractElement *, parseData& );
  void createParserFile( const parseData& );
 private:
  void createRelDir( const parseData& );
  void createPairs( const parseData& );
  Timbl::TimblAPI *pairs;
  Timbl::TimblAPI *dir;
  Timbl::TimblAPI *rels;
  std::string maxDepSpanS;
  size_t maxDepSpan;
  PythonInterface *PI;
  std::vector<mwuAna*> *mwu_data;
  bool isInit;
  std::string fileName;
  LogStream *parseLog;
};


#endif
