/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2017
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

#include "mbt/MbtAPI.h"
#include "frog/Frog.h"
#include "ucto/unicode.h"
#include "frog/iob_tagger_mod.h"

using namespace std;
using namespace folia;
using namespace TiCC;
using namespace Tagger;

#define LOG *Log(tag_log)

const string cgn_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";

bool EIOBTagger::init( const Configuration& config ){
  return BaseTagger::init( config );
}

void EIOBTagger::addChunk( ChunkingLayer *chunks,
			   const vector<Word*>& words,
			   const vector<double>& confs,
			   const string& IOB,
			   const string& textclass ){
  double conf = 1;
  for ( auto const& val : confs )
    conf *= val;
  KWargs args;
  args["class"] = IOB;
  args["set"] = tagset;
  args["confidence"] = toString(conf);
  string parent_id = chunks->id();
  if ( !parent_id.empty() ){
    args["generate_id"] = chunks->id();
  }
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }
  Chunk *chunk = 0;
#pragma omp critical(foliaupdate)
  {
    try {
      chunk = new Chunk( args, chunks->doc() );
      chunks->append( chunk );
    }
    catch ( exception& e ){
      LOG << "addChunk failed: " << e.what() << endl;
      throw;
    }
  }
  for ( const auto& word : words ){
    if ( word->isinstance(PlaceHolder_t) ){
      continue;
    }
#pragma omp critical(foliaupdate)
    {
      chunk->append( word );
    }
  }
}

void EIOBTagger::addIOBTags( const vector<Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() ){
    return;
  }
  ChunkingLayer *el = 0;
#pragma omp critical(foliaupdate)
  {
    Sentence *sent = words[0]->sentence();
    try {
      el = sent->annotation<ChunkingLayer>(tagset);
    }
    catch(...){
      KWargs args;
      args["generate_id"] = sent->id();
      args["set"] = tagset;
      el = new ChunkingLayer( args, sent->doc() );
      sent->append( el );
    }
  }
  vector<Word*> stack;
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

void EIOBTagger::addDeclaration( Document& doc ) const {
#pragma omp critical(foliaupdate)
  {
    doc.declare( AnnotationType::CHUNKING,
		 tagset,
		 "annotator='frog-chunker-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void EIOBTagger::Classify( const vector<Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    vector<string> ptags;
    extract_words_tags( swords, cgn_tagset, words, ptags );
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

void EIOBTagger::post_process( const std::vector<folia::Word*>& swords ){
  if ( debug ){
    LOG << "EIOB postprocess...." << endl;
  }
  vector<double> conf;
  vector<string> tags;
  for ( const auto& tag : _tag_result ){
    tags.push_back( tag.assignedTag() );
    conf.push_back( tag.confidence() );
  }
  addIOBTags( swords, tags, conf );
}
