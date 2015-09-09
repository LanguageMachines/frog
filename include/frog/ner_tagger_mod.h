/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
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

#ifndef NER_TAGGER_MOD_H
#define NER_TAGGER_MOD_H

class NERTagger {
 public:
  NERTagger(LogStream *);
  ~NERTagger();
  bool init( const TiCC::Configuration& );
  void Classify( const std::vector<folia::Word *>& );
  void addDeclaration( folia::Document& ) const;
  void addNERTags( const std::vector<folia::Word*>&,
		   const std::vector<std::string>&,
		   const std::vector<double>& );
  std::string getTagset() const { return tagset; };
  std::vector<TagResult> tagLine( const std::string& );
  bool fill_known_ners( const std::string& );
  void handle_known_ners( const std::vector<std::string>&, 
			  std::vector<std::string>& );
  void merge( const std::vector<std::string>&,
	      std::vector<std::string>& tags, 
	      std::vector<double>& );

 private:
  MbtAPI *tagger;
  LogStream *nerLog;
  int debug;
  std::string version;
  std::string tagset;
  std::vector<std::map<std::string,std::string>> known_ners;
};

#endif // NER_TAGGER_MOD_H
