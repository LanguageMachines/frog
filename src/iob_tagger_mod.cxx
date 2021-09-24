/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2021
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

#include <algorithm>
#include "frog/iob_tagger_mod.h"

#include "ticcutils/PrettyPrint.h"
#include "frog/Frog-util.h"

using namespace std;
using namespace Tagger;

#define LOG *TiCC::Log(err_log)
#define DBG *TiCC::Log(dbg_log)

static string POS_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";

bool IOBTagger::init( const TiCC::Configuration& config ){
  /// initalize a IOB chunker from 'config'
  /*!
    \param config the TiCC::Configuration
    \return true on succes, false otherwise

    first BaseTagger::init() is called to set generic values,
    then the IOB specific values 'set' is added
  */
  if ( !BaseTagger::init( config ) ){
    return false;
  }
  string val = config.lookUp( "set", "tagger" );
  if ( !val.empty() ){
    POS_tagset = val;
  }
  return true;
}

void IOBTagger::add_declaration( folia::Document& doc,
				 folia::processor *proc ) const {
  /// add CHUNKING annotation as an AnnotationType to the document
  /*!
    \param doc the Document the add to
    \param proc the processor to add
  */
  folia::KWargs args;
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::CHUNKING, tagset, args );
}

void IOBTagger::Classify( frog_data& swords ){
  /// Tag one sentence, given in frog_data format
  /*!
    \param swords the frog_data structure to analyze

    When tagging succeeds, 'swords' will be extended with the tag results
  */
  vector<UnicodeString> words;
  vector<UnicodeString> ptags;
#pragma omp critical (dataupdate)
  {
    for ( const auto& w : swords.units ){
      UnicodeString word = TiCC::UnicodeFromUTF8( w.word);
      word = filter_spaces( word );
      words.push_back( word );
      ptags.push_back( TiCC::UnicodeFromUTF8(w.tag) );
    }
  }

  vector<tag_entry> to_do;
  UnicodeString prev = "_";
  for ( size_t i=0; i < swords.size(); ++i ){
    tag_entry ta;
    ta.word = words[i];
    ta.enrichment = prev;
    prev = ptags[i];
    ta.enrichment += "\t" + ptags[i];
    if ( i < swords.size() - 1 ){
      ta.enrichment += "\t" + ptags[i+1];
    }
    else {
      ta.enrichment += "\t_";
    }
    to_do.push_back( ta );
  }
  _tag_result = tag_entries( to_do );
  if ( debug ){
    DBG << "IOB tagger out: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      DBG << "[" << i << "] : word=" << _tag_result[i].word()
	  << " tag=" << _tag_result[i].assigned_tag()
	  << " confidence=" << _tag_result[i].confidence() << endl;
    }
  }
  post_process( swords );
}

void IOBTagger::post_process( frog_data& sentence ){
  /// finish the Chunking processs by updating 'sentence'
  /*!
    \param sentence a frog_data structure to update with Chunking info
    from the _tag_result array.
  */
  if ( debug ){
    DBG << "IOB postprocess...." << endl;
  }
  string last_tag;
  for ( size_t i=0; i < _tag_result.size(); ++i ){
    UnicodeString tag = _tag_result[i].assigned_tag();
    if ( tag[0] == 'I' ){
      // make sure that we start a new 'sequence' with a B
      if ( last_tag.empty() ){
	tag.replace(0,1,'B');
      }
      else {
	string hack = TiCC::UnicodeToUTF8( tag );
	if ( last_tag.substr(2) != hack.substr( 2 ) ){
	  tag.replace(0,1,'B');
	}
      }
      last_tag = TiCC::UnicodeToUTF8(tag);
    }
    else if ( tag[0] == 'O' ){
      last_tag.clear();
    }
    else if ( tag[0] == 'B' ){
      last_tag = TiCC::UnicodeToUTF8(tag);
    }
    addTag( sentence.units[i],
	    tag,
	    _tag_result[i].confidence() );
  }
}

void IOBTagger::addTag( frog_record& fd,
			const UnicodeString& tag,
			double confidence ){
  /// add a tag/confidence pair to a frog_record
  /*!
    \param fd The record to add to
    \param tag the Chunk tag to add
    \param confidence the confidence value for the tag
  */
#pragma omp critical (dataupdate)
  {
    fd.iob_tag = TiCC::UnicodeToUTF8(tag);
    fd.iob_confidence = confidence;
  }
}

void IOBTagger::add_result( const frog_data& fd,
			    const vector<folia::Word*>& wv ) const {
  /// add the Chunking tags in 'fd' to the FoLiA list of Word
  /*!
    \param fd The tagged results
    \param wv The folia:Word vector
  */
  folia::Sentence* s = wv[0]->sentence();
  folia::ChunkingLayer *el = s->annotation<folia::ChunkingLayer>(tagset);
  folia::Chunk *iob = 0;
  double iob_conf = 0.0; //accummulated confidence
  size_t i = 0;
  for ( const auto& word : fd.units ){
    if ( word.iob_tag[0] == 'B' ){
      if ( el == 0 ){
	// create a layer, we need it
	folia::KWargs args;
	args["set"] = getTagset();
	if ( !s->id().empty() ){
	  args["generate_id"] = s->id();
	}
#pragma omp critical (foliaupdate)
	{
	  el = new folia::ChunkingLayer( args, s->doc() );
	  s->append(el);
	}
      }
      // a new entity starts here
      if ( iob != 0 ){
	// add this iob to the layer
	iob->confidence( iob_conf );
#pragma omp critical (foliaupdate)
	{
	  el->append( iob );
	}
      }
      // now make new entity
      folia::KWargs args;
      args["set"] = getTagset();
      if ( !el->id().empty() ){
	args["generate_id"] = el->id();
      }
      args["class"] = word.iob_tag.substr(2);
      args["confidence"] = TiCC::toString(word.iob_confidence);
      if ( textclass != "current" ){
	args["textclass"] = textclass;
      }
#pragma omp critical (foliaupdate)
      {
	iob = new folia::Chunk( args, s->doc() );
	iob_conf = word.iob_confidence;
	iob->append( wv[i] );
      }
    }
    else if ( word.iob_tag[0] == 'I' ){
      // continue in an entity
      iob_conf *= word.iob_confidence;
      if ( iob ){
#pragma omp critical (foliaupdate)
	{
	  iob->append( wv[i] );
	}
      }
      else {
	throw logic_error( "unexpected empty IOB" );
      }
    }
    else if ( word.iob_tag[0] == '0' ){
      if ( iob != 0 ){
	iob->confidence( iob_conf );
	iob_conf = 0.0;
#pragma omp critical (foliaupdate)
	{
	  el->append( iob );
	}
	iob = 0;
      }
    }
    ++i;
  }
  if ( iob != 0 ){
    // some leftovers
    iob->confidence( iob_conf );
#pragma omp critical (foliaupdate)
    {
      el->append( iob );
    }
  }
}
