/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
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
  folia::KWargs args;
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::CHUNKING, tagset, args );
}

void IOBTagger::Classify( frog_data& swords ){
  vector<string> words;
  vector<string> ptags;
#pragma omp critical (dataupdate)
  {
    for ( const auto& w : swords.units ){
      string word_s = w.word;
      // the word may contain spaces, remove them all!
      word_s.erase(remove_if(word_s.begin(), word_s.end(), ::isspace), word_s.end());
      words.push_back( word_s );
      ptags.push_back( w.tag );
    }
  }

  vector<tag_entry> to_do;
  string prev = "_";
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
  _tag_result = tagLine( to_do );
  if ( debug ){
    DBG << "IOB tagger out: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      DBG << "[" << i << "] : word=" << _tag_result[i].word()
	  << " tag=" << _tag_result[i].assignedTag()
	  << " confidence=" << _tag_result[i].confidence() << endl;
    }
  }
  post_process( swords );
}

void IOBTagger::post_process( frog_data& words ){
  if ( debug ){
    DBG << "IOB postprocess...." << endl;
  }
  string last_tag;
  for ( size_t i=0; i < _tag_result.size(); ++i ){
    string tag = _tag_result[i].assignedTag();
    if ( tag[0] == 'I' ){
      // make sure that we start a new 'sequence' with a B
      if ( last_tag.empty() ){
	tag[0] = 'B';
      }
      else {
	if ( last_tag.substr(2) != tag.substr( 2 ) ){
	  tag[0] = 'B';
	}
      }
      last_tag = tag;
    }
    else if ( tag[0] == 'O' ){
      last_tag.clear();
    }
    else if ( tag[0] == 'B' ){
      last_tag = tag;
    }
    addTag( words.units[i],
	    tag,
	    _tag_result[i].confidence() );
  }
}

void IOBTagger::addTag( frog_record& fd,
			const string& tag,
			double confidence ){
#pragma omp critical (dataupdate)
  {
    fd.iob_tag = tag;
    fd.iob_confidence = confidence;
  }
}

void IOBTagger::add_result( const frog_data& fd,
			    const vector<folia::Word*>& wv ) const {
  folia::Sentence* s = wv[0]->sentence();
  folia::ChunkingLayer *el = 0;
  folia::Chunk *iob = 0;
  double iob_conf = 0.0; //accummulated confidence
  size_t i = 0;
  for ( const auto& word : fd.units ){
    if ( word.iob_tag[0] == 'B' ){
      if ( el == 0 ){
	try {
	  el = s->annotation<folia::ChunkingLayer>(tagset);
	}
	catch(...){
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
