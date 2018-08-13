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

#include "frog/iob_tagger_mod.h"

#include "ticcutils/PrettyPrint.h"
#include "frog/Frog-util.h"

using namespace std;
using namespace Tagger;

#define LOG *TiCC::Log(tag_log)

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

void IOBTagger::addChunk( folia::ChunkingLayer *chunks,
			  const vector<folia::Word*>& words,
			  const vector<double>& confs,
			  const string& IOB,
			  const string& textclass ){
  double conf = 1;
  for ( auto const& val : confs )
    conf *= val;
  folia::KWargs args;
  args["class"] = IOB;
  args["set"] = tagset;
  args["confidence"] = TiCC::toString(conf);
  string c_id = chunks->id();
  if ( !c_id.empty() ){
    args["generate_id"] = c_id;
  }
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }
  folia::Chunk *chunk = 0;
#pragma omp critical (foliaupdate)
  {
    try {
      chunk = new folia::Chunk( args, chunks->doc() );
      chunks->append( chunk );
    }
    catch ( exception& e ){
      LOG << "addChunk failed: " << e.what() << endl;
      throw;
    }
  }
  for ( const auto& word : words ){
    if ( word->isinstance(folia::PlaceHolder_t) ){
      continue;
    }
#pragma omp critical (foliaupdate)
    {
      chunk->append( word );
    }
  }
}

void IOBTagger::addIOBTags( const vector<folia::Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() ){
    return;
  }
  folia::ChunkingLayer *el = 0;
#pragma omp critical (foliaupdate)
  {
    folia::Sentence *sent = words[0]->sentence();
    try {
      el = sent->annotation<folia::ChunkingLayer>(tagset);
    }
    catch(...){
      folia::KWargs args;
      string s_id = sent->id();
      if ( !s_id.empty() ){
	args["generate_id"] = s_id;
      }
      args["set"] = tagset;
      el = new folia::ChunkingLayer( args, sent->doc() );
      sent->append( el );
    }
  }
  vector<folia::Word*> stack;
  vector<double> dstack;
  string curIOB;
  int i = 0;
  for ( const auto& tag : tags ){
    vector<string> iob;
    if ( debug){
      LOG << "word=" << words[i]->text() << " IOB TAG = " << tag << endl;
    }
    if ( tag == "O" ){
      if ( !stack.empty() ){
	if ( debug) {
	  LOG << "O spit out " << curIOB << endl;
	  using TiCC::operator<<;
	  LOG << "spit out " << stack << endl;
	}
	addChunk( el, stack, dstack, curIOB, textclass );
	dstack.clear();
	stack.clear();
      }
      ++i;
      continue;
    }
    else {
      int num_words = TiCC::split_at( tag, iob, "-" );
      if ( num_words != 2 ){
	LOG << "expected <IOB>-tag, got: " << tag << endl;
	throw;
      }
    }
    if ( iob[0] == "B" ||
	 ( iob[0] == "I" && stack.empty() ) ||
	 ( iob[0] == "I" && iob[1] != curIOB ) ){
      // an I without preceding B is handled as a B
      // an I with a different TAG is also handled as a B
      if ( !stack.empty() ){
	if ( debug ){
	  LOG << "B spit out " << curIOB << endl;
	  using TiCC::operator<<;
	  LOG << "spit out " << stack << endl;
	}
	addChunk( el, stack, dstack, curIOB, textclass );
	dstack.clear();
	stack.clear();
      }
      curIOB = iob[1];
    }
    dstack.push_back( confs[i] );
    stack.push_back( words[i] );
    ++i;
  }
  if ( !stack.empty() ){
    if ( debug ){
      LOG << "END spit out " << curIOB << endl;
      using TiCC::operator<<;
      LOG << "spit out " << stack << endl;
    }
    addChunk( el, stack, dstack, curIOB, textclass );
  }
}

void IOBTagger::addDeclaration( folia::Document& doc ) const {
  doc.declare( folia::AnnotationType::CHUNKING,
	       tagset,
	       "annotator='frog-chunker-" + _version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void IOBTagger::Classify( const vector<folia::Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    vector<string> ptags;
    extract_words_tags( swords, POS_tagset, words, ptags );
    string text_block;
    string prev = "_";
    for ( size_t i=0; i < swords.size(); ++i ){
      string word = words[i];
      string pos = ptags[i];
      text_block += word + "\t" + prev + "\t" + pos + "\t";
      prev = pos;
      if ( i < swords.size() - 1 ){
	text_block += ptags[i+1];
      }
      else {
	text_block += "_";
      }
      text_block += "\t??\n";
    }
    if ( debug ){
      LOG << "TAGGING TEXT_BLOCK\n" << text_block << endl;
    }
    _tag_result = tagger->TagLine( text_block );
    if ( debug ){
      LOG << "IOB tagger out: " << endl;
      for ( size_t i=0; i < _tag_result.size(); ++i ){
	LOG << "[" << i << "] : word=" << _tag_result[i].word()
	    << " tag=" << _tag_result[i].assignedTag()
	    << " confidence=" << _tag_result[i].confidence() << endl;
      }
    }
  }
  post_process( swords );
}

void IOBTagger::post_process( const std::vector<folia::Word*>& swords ){
  if ( debug ){
    LOG << "IOB postprocess...." << endl;
  }
  vector<double> conf;
  vector<string> tags;
  for ( const auto& tag : _tag_result ){
    tags.push_back( tag.assignedTag() );
    conf.push_back( tag.confidence() );
  }
  addIOBTags( swords, tags, conf );
}

void IOBTagger::Classify( frog_data& swords ){
  vector<string> words;
  vector<string> ptags;
#pragma omp critical (dataupdate)
  {
    for ( const auto& w : swords.units ){
      words.push_back( w.word );
      ptags.push_back( w.tag );
    }
  }
  string text_block;
  string prev = "_";
  for ( size_t i=0; i < swords.size(); ++i ){
    string word = words[i];
    string pos = ptags[i];
    text_block += word + "\t" + prev + "\t" + pos + "\t";
    prev = pos;
    if ( i < swords.size() - 1 ){
      text_block += ptags[i+1];
    }
    else {
      text_block += "_";
    }
    text_block += "\t??\n";
  }
  if ( debug ){
    LOG << "TAGGING TEXT_BLOCK\n" << text_block << endl;
  }
  _tag_result = tagger->TagLine( text_block );
  if ( debug ){
    LOG << "IOB tagger out: " << endl;
    for ( size_t i=0; i < _tag_result.size(); ++i ){
      LOG << "[" << i << "] : word=" << _tag_result[i].word()
	  << " tag=" << _tag_result[i].assignedTag()
	  << " confidence=" << _tag_result[i].confidence() << endl;
    }
  }
  post_process( swords );
}

void IOBTagger::post_process( frog_data& words ){
  if ( debug ){
    LOG << "IOB postprocess...." << endl;
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

void IOBTagger::add_result( folia::Sentence* s,
			    const frog_data& fd,
			    const vector<folia::Word*>& wv ) const {
  folia::ChunkingLayer *el = 0;
  folia::Chunk *iob = 0;
  double iob_conf = 0.0; //accummulated confidence
  size_t i = 0;
  for ( const auto& word : fd.units ){
    if ( word.iob_tag[0] == 'B' ){
      if ( el == 0 ){
	// create a layer, we need it
	folia::KWargs args;
	args["set"] = getTagset();
	args["generate_id"] = s->id();
	el = new folia::ChunkingLayer( args, s->doc() );
	s->append(el);
      }
      // a new entity starts here
      if ( iob != 0 ){
	// add this iob to the layer
	iob->confidence( iob_conf );
	iob_conf = 0.0;
	el->append( iob );
      }
      // now make new entity
      folia::KWargs args;
      args["set"] = getTagset();
      args["generate_id"] = el->id();
      args["class"] = word.iob_tag.substr(2);
      args["confidence"] = TiCC::toString(word.iob_confidence);
      iob = new folia::Chunk( args, s->doc() );
      iob_conf = word.iob_confidence;
      iob->append( wv[i] );
    }
    else if ( word.iob_tag[0] == 'I' ){
      // continue in an entity
      iob_conf *= word.iob_confidence;
      iob->append( wv[i] );
    }
    else if ( word.iob_tag[0] == '0' ){
      if ( iob != 0 ){
	iob->confidence( iob_conf );
	iob_conf = 0.0;
	el->append( iob );
	iob = 0;
      }
    }
    ++i;
  }
  if ( iob != 0 ){
    // some leftovers
    iob->confidence( iob_conf );
    el->append( iob );
  }
}
