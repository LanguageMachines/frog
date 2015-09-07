/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
  Tilburg University

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for Dutch

  This file is part of frog

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

#include "mbt/MbtAPI.h"
#include "ucto/unicode.h"
#include "frog/Frog.h"
#include "frog/cgn_tagger_mod.h"

using namespace std;
using namespace folia;

void CGNTagger::fillSubSetTable(){
  // should become a config file!
  cgnSubSets.insert( make_pair("soort", "ntype" ));
  cgnSubSets.insert( make_pair("eigen", "ntype" ));

  cgnSubSets.insert( make_pair("ev", "getal" ));
  cgnSubSets.insert( make_pair("mv", "getal" ));
  cgnSubSets.insert( make_pair("getal", "getal" ));

  cgnSubSets.insert( make_pair("zijd", "genus" ));
  cgnSubSets.insert( make_pair("onz", "genus" ));
  cgnSubSets.insert( make_pair("masc", "genus" ));
  cgnSubSets.insert( make_pair("fem", "genus" ));
  cgnSubSets.insert( make_pair("genus", "genus" ) );

  cgnSubSets.insert( make_pair("stan", "naamval" ));
  cgnSubSets.insert( make_pair("gen", "naamval" ));
  cgnSubSets.insert( make_pair("dat", "naamval" ));
  cgnSubSets.insert( make_pair("nomin", "naamval" ));
  cgnSubSets.insert( make_pair("obl", "naamval" ));
  cgnSubSets.insert( make_pair("bijz", "naamval" ));

  cgnSubSets.insert( make_pair("afgebr", "spectype" ));
  cgnSubSets.insert( make_pair("afk", "spectype" ));
  cgnSubSets.insert( make_pair("deeleigen", "spectype" ));
  cgnSubSets.insert( make_pair("symb", "spectype" ));
  cgnSubSets.insert( make_pair("vreemd", "spectype" ));
  cgnSubSets.insert( make_pair("enof", "spectype" ));
  cgnSubSets.insert( make_pair("meta", "spectype" ));
  cgnSubSets.insert( make_pair("achter", "spectype" ));
  cgnSubSets.insert( make_pair("comment", "spectype" ));
  cgnSubSets.insert( make_pair("onverst", "spectype" ));

  cgnSubSets.insert( make_pair("neven", "conjtype" ));
  cgnSubSets.insert( make_pair("onder", "conjtype" ));

  cgnSubSets.insert( make_pair("init", "vztype" ));
  cgnSubSets.insert( make_pair("versm", "vztype" ));
  cgnSubSets.insert( make_pair("fin", "vztype" ));

  cgnSubSets.insert( make_pair("agr", "npagr" ));
  cgnSubSets.insert( make_pair("evon", "npagr" ));
  cgnSubSets.insert( make_pair("rest", "npagr" ));
  cgnSubSets.insert( make_pair("evz", "npagr" ));
  cgnSubSets.insert( make_pair( "agr3", "npagr" ));
  cgnSubSets.insert( make_pair( "evmo", "npagr" ));
  cgnSubSets.insert( make_pair( "rest3", "npagr" ));
  cgnSubSets.insert( make_pair("evf", "npagr" ));

  cgnSubSets.insert( make_pair("bep", "lwtype" ));
  cgnSubSets.insert( make_pair("onbep", "lwtype" ));

  cgnSubSets.insert( make_pair("pers", "vwtype" ));
  cgnSubSets.insert( make_pair("pr", "vwtype" ));
  cgnSubSets.insert( make_pair("refl", "vwtype" ));
  cgnSubSets.insert( make_pair("recip", "vwtype" ));
  cgnSubSets.insert( make_pair("bez", "vwtype" ));
  cgnSubSets.insert( make_pair("vb", "vwtype" ));
  cgnSubSets.insert( make_pair("vrag", "vwtype" ));
  cgnSubSets.insert( make_pair("betr", "vwtype" ));
  cgnSubSets.insert( make_pair("excl", "vwtype" ));
  cgnSubSets.insert( make_pair("aanw", "vwtype" ));
  cgnSubSets.insert( make_pair("onbep", "vwtype" ));

  cgnSubSets.insert( make_pair("adv-pron", "pdtype" ));
  cgnSubSets.insert( make_pair("pron", "pdtype" ));
  cgnSubSets.insert( make_pair("det", "pdtype" ));
  cgnSubSets.insert( make_pair("grad", "pdtype" ));

  cgnSubSets.insert( make_pair("vol", "status" ));
  cgnSubSets.insert( make_pair("red", "status" ));
  cgnSubSets.insert( make_pair("nadr", "status" ));

  cgnSubSets.insert( make_pair("1", "persoon" ));
  cgnSubSets.insert( make_pair("2", "persoon" ));
  cgnSubSets.insert( make_pair("2v", "persoon" ));
  cgnSubSets.insert( make_pair("2b", "persoon" ));
  cgnSubSets.insert( make_pair("3", "persoon" ));
  cgnSubSets.insert( make_pair("3p", "persoon" ));
  cgnSubSets.insert( make_pair("3m", "persoon" ));
  cgnSubSets.insert( make_pair("3v", "persoon" ));
  cgnSubSets.insert( make_pair("3o", "persoon" ));
  cgnSubSets.insert( make_pair("persoon", "persoon" ));

  cgnSubSets.insert( make_pair("prenom", "positie" ));
  cgnSubSets.insert( make_pair("postnom", "positie" ));
  cgnSubSets.insert( make_pair("nom", "positie" ));
  cgnSubSets.insert( make_pair("vrij", "positie" ));

  cgnSubSets.insert( make_pair("zonder", "buiging" ));
  cgnSubSets.insert( make_pair("met-e", "buiging" ));
  cgnSubSets.insert( make_pair("met-s", "buiging" ));

  cgnSubSets.insert( make_pair("zonder-v", "getal-n" ));
  cgnSubSets.insert( make_pair("mv-n", "getal-n" ));
  cgnSubSets.insert( make_pair("zonder-n", "getal-n" ));

  cgnSubSets.insert( make_pair("basis", "graad" ));
  cgnSubSets.insert( make_pair("comp", "graad" ));
  cgnSubSets.insert( make_pair("sup", "graad" ));
  cgnSubSets.insert( make_pair("dim", "graad" ));

  cgnSubSets.insert( make_pair("pv", "wvorm" ));
  cgnSubSets.insert( make_pair("inf", "wvorm" ));
  cgnSubSets.insert( make_pair("vd", "wvorm" ));
  cgnSubSets.insert( make_pair("od", "wvorm" ));

  cgnSubSets.insert( make_pair("tgw", "pvtijd" ));
  cgnSubSets.insert( make_pair("verl", "pvtijd" ));
  cgnSubSets.insert( make_pair("conj", "pvtijd" ));

  cgnSubSets.insert( make_pair("ev", "pvagr" ));
  cgnSubSets.insert( make_pair("mv", "pvagr" ));
  cgnSubSets.insert( make_pair( "met-t", "pvagr" ));

  cgnSubSets.insert( make_pair("hoofd", "numtype" ));
  cgnSubSets.insert( make_pair("rang", "numtype" ));

  cgnSubSets.insert( make_pair("dial", "dial" ));

  cgnConstraints.insert( make_pair( "getal", "N" ) );
  cgnConstraints.insert( make_pair( "getal", "VNW" ) );
  cgnConstraints.insert( make_pair( "pvagr", "WW" ) );
}


bool CGNTagger::init( const Configuration& config ){
  if ( debug )
    *Log(tag_log) << "INIT CGN Tagger." << endl;
  if ( POSTagger::init( config ) ){
    fillSubSetTable();
    if ( debug )
      *Log(tag_log) << "DONE CGN Tagger." << endl;
    return true;
  }
  return false;
}

string CGNTagger::getSubSet( const string& val, const string& head ){
  auto it = cgnSubSets.find( val );
  if ( it == cgnSubSets.end() )
    throw folia::ValueError( "unknown cgn subset for class: '" + val + "'" );
  string result;
  while ( it != cgnSubSets.upper_bound(val) ){
    result = it->second;
    auto cit = cgnConstraints.find( result );
    if ( cit == cgnConstraints.end() ){
      // no constraints on this value
      return result;
    }
    else {
      while ( cit != cgnConstraints.upper_bound( result ) ){
	if ( cit->second == head ) {
	  // allowed
	  return result;
	}
	++cit;
      }
    }
    ++it;
  }
  throw folia::ValueError( "unable to find cgn subset for class: '" + val +
			   "' whithin the constraints for '" + head + "'" );
}

void CGNTagger::post_process( const vector<Word *>& words ){
  for ( auto const word : words ){
    PosAnnotation *postag = word->annotation<PosAnnotation>( );
    string cls = postag->cls();
    vector<string> parts;
    TiCC::split_at_first_of( cls, parts, "()" );
    string head = parts[0];
    KWargs args;
    args["cls"]    = head;
    args["set"]    = tagset;
    folia::Feature *feat = new folia::HeadFeature( args );
    postag->append( feat );
    if ( head == "SPEC" ){
      postag->confidence(1.0);
    }
    if ( parts.size() > 1 ){
      vector<string> tagParts;
      size_t numParts = TiCC::split_at( parts[1], tagParts, "," );
      for ( size_t i=0; i < numParts; ++i ){
	KWargs args;
	args["set"]    = tagset;
	args["subset"] = getSubSet( tagParts[i], head );
	args["cls"]    = tagParts[i];
#pragma omp critical(foliaupdate)
	{
	  folia::Feature *feat = new folia::Feature( args );
	  postag->append( feat );
	}
      }
    }
  }
}

void CGNTagger::Classify( const vector<Word*>& swords ){
  POSTagger::Classify( swords );
  if ( debug ){
    *Log(tag_log) << "POS Classify done:" << endl;
  }
  post_process( swords );
}
