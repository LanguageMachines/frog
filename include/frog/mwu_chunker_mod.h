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


class complexAna;

class mwuAna {
  friend std::ostream& operator<< (std::ostream&, const mwuAna& );
 public:
  mwuAna( const std::string&, const std::string&,
	  const std::string&, const std::string&,
	  const std::string&, const std::string& );
  virtual ~mwuAna() {};
  
  virtual complexAna *append( const mwuAna * );
  std::string getTagHead() const {
    return tagHead;
  }
  
  std::string getTagMods() const;
  
  std::string getWord() const {
    return word;
  }
  std::string getLemma() const {
    return lemma;
  }
  std::string getMorphemes() const {
    return morphemes;
  }
  std::string getTag() const {
    return tag;
  }
  
  virtual std::string displayTag( );
  
 protected:
    mwuAna(){};
    std::string word;
    std::string tag;
    std::string tagHead;
    std::string tagMods;
    std::string lemma;
    std::string morphemes;
    std::string CFS;
    std::string OFS;
};

class complexAna: public mwuAna {
 public:
  complexAna( );
  complexAna *append( const mwuAna * );
  std::string displayTag( );
};

#define mymap2 std::multimap<std::string, std::vector<std::string> >

class Configuration;

class Mwu {
  friend std::ostream& operator<< (std::ostream&, const Mwu& );
 public:
  ~Mwu();
  void reset();
  void init( const Configuration& );
  void Classify( );
  void add( const std::string&, const std::string&,
	    const std::string&, const std::string& );
  std::vector<mwuAna*>& getAna(){ return mWords; };
  std::string myCFS;
 private:
  bool readsettings( const std::string&, const std::string&);
  bool read_mwus( const std::string& );
  int debug;
  std::string mwuFileName;
  std::vector<mwuAna*> mWords;
  mymap2 MWUs;
};

#endif
