/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2011
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

#ifndef __MWU_CHUNKER__
#define __MWU_CHUNKER__

#include "libfolia/folia.h"

class mwuAna {
  friend std::ostream& operator<< (std::ostream&, const mwuAna& );
 public:
  mwuAna( folia::FoliaElement * );
  virtual ~mwuAna() {};
  
  void merge( const mwuAna * );

  std::string getWord() const {
    return word;
  }

  bool isSpec(){ return spec; };
  void addEntity( folia::FoliaElement * );
  
 protected:
    mwuAna(){};
    std::string word;
    bool spec;
    std::vector<folia::FoliaElement *> fwords;
};

#define mymap2 std::multimap<std::string, std::vector<std::string> >

class Configuration;

class Mwu {
  friend std::ostream& operator<< (std::ostream&, const Mwu& );
 public:
  ~Mwu();
  void reset();
  bool init( const Configuration& );
  void Classify( folia::FoliaElement* );
  void add( folia::FoliaElement * );
  std::vector<mwuAna*>& getAna(){ return mWords; };
  std::string myCFS;
 private:
  bool readsettings( const std::string&, const std::string&);
  bool read_mwus( const std::string& );
  void Classify();
  int debug;
  std::string mwuFileName;
  std::vector<mwuAna*> mWords;
  mymap2 MWUs;
};

#endif
