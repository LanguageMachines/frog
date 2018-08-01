/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2018
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

#include <ostream>
#include "ticcutils/PrettyPrint.h"
#include "frog/FrogData.h"

using namespace std;
using TiCC::operator<<;

frog_data::frog_data():
  no_space(false),
  tag_confidence(0.0),
  iob_confidence(0.0),
  ner_confidence(0.0)
{}

const string TAB = "\t";
ostream& operator<<( ostream& os, const frog_data& fd ){
  os << fd.word << TAB;
  if ( !fd.lemmas.empty() ){
    os << fd.lemmas[0];
  }
  os << TAB;
  if ( fd.morphs.empty() ){
    if ( !fd.morphs_nested.empty() ){
      os << fd.morphs_nested[0];
    }
  }
  else {
    os << fd.morphs;
  }
  os << TAB << fd.tag << TAB << fd.tag_confidence;
  os << TAB << fd.iob_tag << TAB << fd.iob_confidence;
  os << TAB << fd.ner_tag << TAB << fd.ner_confidence;
  return os;
}
