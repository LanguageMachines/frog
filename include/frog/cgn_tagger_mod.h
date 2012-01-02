/*
  $Id: $
  $URL: $

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

#ifndef TAGGER_MOD_H
#define TAGGER_MOD_H

class Configuration;

class MBTagger {
 public:
  MBTagger();
  ~MBTagger();
  bool init( const Configuration& );
  std::string Classify( folia::FoliaElement * );
 private:
  MbtAPI *tagger;  
};

#endif // TAGGER_MOD_H
