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
                                    
#ifndef MBMA_MOD_H
#define MBMA_MOD_H

#include <unicode/translit.h>

class MBMAana;

struct waStruct {
  UnicodeString word;
  std::string act;
  void clear(){
    word = "";
    act = "";
  }
};

class Configuration;

class Mbma {
 public:
  Mbma();
  ~Mbma();
  bool init( const Configuration& );
  void addDeclaration( folia::Document& doc ) const;
  bool Classify( folia::Word * );
  std::vector<std::vector<std::string> > analyze( const std::string& );
 private:
  void cleanUp();
  bool readsettings( const std::string&, const std::string& );
  void fillMaps();
  void init_cgn( const std::string& );
  void init_filter( );
  UnicodeString filterDiacritics( const UnicodeString& ) const;
  std::vector<std::string> make_instances( const UnicodeString& word );
  std::string calculate_ins_del( const std::string& in_class, 
				 std::string& deletestring,
				 std::string& insertstring,
				 bool& );
  std::vector<waStruct> Step1( unsigned int step, 
			       const UnicodeString& word, 
			       const std::vector<std::vector<std::string> >& classParts,
			       const std::string& basictags );
  void resolve_inflections( std::vector<waStruct>& , const std::string& );
  MBMAana addInflect( const std::vector<waStruct>& ana,
		      const std::string&, 
		      const std::vector<std::string>&  );
  MBMAana inflectAndAffix( const std::vector<waStruct>& ana );
  void execute( const UnicodeString& word, 
		const std::vector<std::string>& classes );
  void postprocess( folia::Word *, folia::PosAnnotation * );
  int debugFlag;
  void addMorph( folia::Word *, const std::vector<std::string>& );
  std::string MTreeFilename;
  Timbl::TimblAPI *MTree;
  std::map<char,std::string> iNames;  
  std::map<std::string,std::string> tagNames;  
  std::map<std::string,std::string> TAGconv;
  std::vector<MBMAana> analysis;
  std::string version;
  std::string tagset;
  LogStream *mbmaLog;
  Transliterator *transliterator;
};

class MBMAana {
  friend std::ostream& operator<< ( std::ostream& , const MBMAana& );
  public:
  MBMAana() {
    tag = "";
    infl = "";
    description = "";    
  };
 MBMAana( const std::string& _tag,
	  const std::string& _infl,
	  const std::vector<std::string>& _mo, 
	  const std::string& _des ): 
  tag(_tag),infl(_infl),morphemes(_mo), description(_des) {};
  
  ~MBMAana() {};
  
  std::string getTag() const {
    return tag;
  };
  
  std::string getInflection() const {
    return infl;
  };
  
  const std::vector<std::string>& getMorph() const {
    return morphemes;
  };
  
 private:
  std::string tag;
  std::string infl;
  std::vector<std::string> morphemes;
  std::string description; 
};

#endif
