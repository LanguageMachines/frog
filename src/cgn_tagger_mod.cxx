/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2020
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

#include "frog/cgn_tagger_mod.h"
#include "frog/FrogData.h"
#include "frog/Frog-util.h"
#include "ticcutils/PrettyPrint.h"

using namespace std;
using namespace Tagger;
using TiCC::operator<<;

#define LOG *TiCC::Log(err_log)
#define DBG *TiCC::Log(dbg_log)

/// default value for the name of the CGN subsets file
static string subsets_file = "subsets.cgn";
/// default value for the name of the CGN constraints file
static string constraints_file = "constraints.cgn";

bool CGNTagger::fillSubSetTable( const string& sub_file,
				 const string& const_file ){
  /// read als the CGN subsets and constraints
  /*!
    \param sub_file The name of the subsets file
    \param const_file The name of the constraints file
    \return true on succes, false otherwise

    /// The subsets file is in the format:
    /// subset = val1,...,valn
    /// like :
    ///   ntype = soort, eigen
    ///   getal = ev, mv, getal
    ///
    /// The constraints file is in the format:
    /// value=POS1,POS2
    /// like:
    ///   getal = VNW, N
    ///   pvagr = WW
  */
  ifstream sis( sub_file );
  if ( !sis ){
    LOG << "unable to open subsets file: " << sub_file << endl;
    return false;
  }
  LOG << "reading subsets from " << sub_file << endl;
  string line;
  while ( getline( sis, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    vector<string> att_val = TiCC::split_at( line, "=", 2 );
    if ( att_val.size() != 2 ){
      LOG << "invalid line in:'" << sub_file << "' : " << line << endl;
      return false;
    }
    string at = TiCC::trim(att_val[0]);
    vector<string> vals = TiCC::split_at( att_val[1], "," );
    for ( const auto& val : vals ){
      cgnSubSets.insert( make_pair( TiCC::trim(val), at ) );
    }
  }
  if ( !const_file.empty() ){
    ifstream cis( const_file );
    if ( !cis ){
      LOG << "unable to open constraints file: " << const_file << endl;
      return false;
    }
    LOG << "reading constraints from " << const_file << endl;
    while ( getline( cis, line ) ){
      if ( line.empty() || line[0] == '#' ){
	continue;
      }
      vector<string> att_val = TiCC::split_at( line, "=", 2 );
      if ( att_val.size() != 2 ){
	LOG << "invalid line in:'" << sub_file << "' : " << line << endl;
	return false;
      }
      string at = TiCC::trim(att_val[0]);
      vector<string> vals = TiCC::split_at( att_val[1], "," );
      for ( const auto& val : vals ){
	cgnConstraints.insert( make_pair( at, TiCC::trim(val) ) );
      }
    }
  }
  return true;
}

bool CGNTagger::init( const TiCC::Configuration& config ){
  /// initalize a CGN tagger from 'config'
  /*!
    \param config the TiCC::Configuration
    \return true on succes, false otherwise

    first BaseTagger::init() is called to set generic values,
    then the CGN specific values for subset and constraints file-names are
    added and those files are read, except when these have the value 'ignore'
  */
  if (  debug > 1 ){
    DBG << "INIT CGN Tagger." << endl;
  }
  if ( !BaseTagger::init( config ) ){
    return false;
  }
  string val = config.lookUp( "subsets_file", "tagger" );
  if ( !val.empty() ){
    subsets_file = val;
  }
  if ( subsets_file != "ignore" ){
    subsets_file = prefix( config.configDir(), subsets_file );
  }
  val = config.lookUp( "constraints_file", "tagger" );
  if ( !val.empty() ){
    constraints_file = val;
  }
  if ( constraints_file != "ignore" && subsets_file == "ignore" ){
    LOG << "ERROR: when using a constraints file, you NEED a subsets file"
	<< endl;
    return false;
  }
  if ( constraints_file == "ignore" ){
    constraints_file.clear();
  }
  else {
    constraints_file = prefix( config.configDir(), constraints_file );
  }
  if ( subsets_file != "ignore" ){
    if ( !fillSubSetTable( subsets_file, constraints_file ) ){
      return false;
    }
  }
  if ( debug > 1 ){
    DBG << "DONE Init CGN Tagger." << endl;
  }
  return true;
}

void CGNTagger::add_declaration( folia::Document& doc,
				 folia::processor *proc ) const {
  /// add POS annotation as an AnnotationType to the document
  /*!
    \param doc the Document the add to
    \param proc the processor to add
  */
  folia::KWargs args;
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::POS, tagset, args );
}

string CGNTagger::getSubSet( const string& val,
			     const string& head,
			     const string& fullclass ) const {
  /// get a specific subset value. (FoLiA output only)
  /*!
    \param val the val to look up
    \param head the head of the CGN POS-tag
    \param fullclass the original full CGN tag, used for error messages only
    \return a string with the found value or throws when there is a problem
    \note for a well-trained CGN tagger, all values should belong to a subset
    AND there may never be a constraints conflict

    A fullclass may be N(soort,ev,basis,zijd,stan), so the head is N.

    For every value in 'soort,ev,basis,zijd,stan' we lookup the
    subset in the cgn_subsets, and when the constraints on the head
    are satisfied we return the subset.

    For instance: 'soort' is found in the subsets to belong to the subset
    'ntype' and there are no 'head' constrainst on ntype, so the lookup
    for 'soort' yields 'ntype'

    And would the fullclass have been VNW(betr,pron,stan,vol,persoon,getal)
    then the subset for 'getal' is 'getal' AND the constraints for 'getal'
    are 'VNW, N', so these are satisfied and the result is 'getal'
*/
  auto it = cgnSubSets.find( val );
  if ( it == cgnSubSets.end() ){
    throw folia::ValueError( "unknown cgn subset for class: '" + val + "', full class is: '" + fullclass + "'" );
  }
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
			   "' within the constraints for '" + head + "', full class is: '" + fullclass + "'" );
}

void CGNTagger::post_process( frog_data& words ){
  /// add the found tagging results to the frog_data structure
  /*!
    \param words The frog_data structure to extend
  */
  for ( size_t i=0; i < _tag_result.size(); ++i ){
    addTag( words.units[i],
	    _tag_result[i].assigned_tag(),
	    _tag_result[i].confidence() );
  }
}

void CGNTagger::addTag( frog_record& fd,
			const string& inputTag,
			double confidence ){
  /// add a tag/confidence pair to a frog_record
  /*!
    \param fd The record to add to
    \param inputTag the POS tag to add
    \param confidence the confidence value for the inputTag

    \note EXCEPTIONS:
    The confidence will be ignored for SPEC POS tags (1.0) is used then.

    Also a token_tag_map may be used to POST correct the found tags for specific
    Ucto token_classes. e.g. an EMOTICON will migt be translated to a SPEC(SYMB)
    or a PUNCTUATION to a LET()
  */
#pragma omp critical (dataupdate)
  {
    fd.tag = inputTag;
    if ( inputTag.find( "SPEC(" ) == 0 ){
      fd.tag_confidence = 1;
    }
    else {
      fd.tag_confidence = confidence;
    }
  }
  string ucto_class = fd.token_class;
  if ( debug > 1 ){
    DBG << "lookup ucto class= " << ucto_class << endl;
  }
  auto const tt = token_tag_map.find( ucto_class );
  if ( tt != token_tag_map.end() ){
    if ( debug > 1 ){
      DBG << "found translation ucto class= " << ucto_class
	  << " to POS-Tag=" << tt->second << endl;
    }
#pragma omp critical (dataupdate)
    {
      fd.tag = tt->second;
      fd.tag_confidence = 1.0;
    }
  }
}

void CGNTagger::add_tags( const vector<folia::Word*>& wv,
			  const frog_data& fd ) const {
  /// add the tagger results to the folia:Word list
  /*!
    \param wv The folia::Word vector to add to
    \param fd the frog_data structure with the tagger results

  */
  assert( wv.size() == fd.size() );
  size_t pos = 0;
  for ( const auto& word : fd.units ){
    folia::KWargs args;
    args["set"]   = getTagset();
    args["class"] = word.tag;
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
    args["confidence"]= TiCC::toString(word.tag_confidence);
    folia::FoliaElement *postag;
#pragma omp critical (foliaupdate)
    {
      postag = wv[pos]->addPosAnnotation( args );
    }
    vector<string> hv = TiCC::split_at_first_of( word.tag, "()" );
    string head = hv[0];
    args["class"] = head;
#pragma omp critical (foliaupdate)
    {
      folia::Feature *feat = new folia::HeadFeature( args );
      postag->append( feat );
      if ( head == "SPEC" ){
	postag->confidence(1.0);
      }
    }
    vector<string> feats;
    if ( hv.size() > 1 ){
      feats = TiCC::split_at( hv[1], "," );
    }
    for ( const auto& f : feats ){
      folia::KWargs args;
      args["set"] =  getTagset();
      args["subset"] = getSubSet( f, head, word.tag );
      args["class"]  = f;
#pragma omp critical (foliaupdate)
      {
	folia::Feature *feat = new folia::Feature( args, wv[pos]->doc() );
	postag->append( feat );
      }
    }
    ++pos;
  }
}
