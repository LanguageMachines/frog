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

#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "timbl/Types.h"
#include "timbl/Common.h"
#include "frog/mwu_chunker_mod.h"
#include "frog/Frog.h"
#include "frog/FrogData.h"

using namespace::std;
using namespace::Timbl;

FrogData::FrogData( Mwu &mwu ): mwu_data(mwu.getAna()){
  parseNum.resize( mwu_data.size() );
  parseTag.resize( mwu_data.size() );
  OFS = configuration.lookUp( "outputSeparator" );
  if ( OFS.empty() )
    OFS = "\t";
}

FrogData::~FrogData(){
  for ( size_t j = 0; j < mwu_data.size(); ++j )
    delete mwu_data[j];
}

void FrogData::appendParseResult( std::istream& is ){
  string line;
  int cnt=0;
  while ( getline( is, line ) ){
    vector<string> parts;
    int num = split_at( line, parts, " " );
    if ( num > 7 ){
      if ( stringTo<int>( parts[0] ) != cnt+1 ){
	*Log(theErrLog) << "confused! " << endl;
	*Log(theErrLog) << "got line '" << line << "'" << endl;
	*Log(theErrLog) << "expected something like '" << cnt+1 << " " << 
	  mwu_data[cnt] << endl;
      }
      parseNum[cnt] = parts[6];
      parseTag[cnt] = parts[7];
    }
    ++cnt;
  }
}

ostream &showResults( ostream& os, const FrogData& pd ){
  for( size_t i = 0; i < pd.mwu_data.size(); ++i ){
    os << i+1 << pd.OFS << *pd.mwu_data[i] << pd.OFS 
       << pd.parseNum[i] << pd.OFS << pd.parseTag[i] << endl;
  }
  if ( pd.mwu_data.size() )
      os << endl;
  return os;
}

