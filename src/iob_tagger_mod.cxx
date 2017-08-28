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

bool IOBTagger::init( const Configuration& config ){
  return BaseTagger::init( config );
}

void IOBTagger::addChunk( ChunkingLayer *chunks,
			  const vector<Word*>& words,
			  const vector<double>& confs,
			  const string& IOB ){
  double conf = 1;
  for ( auto const& val : confs )
    conf *= val;
  KWargs args;
  args["class"] = IOB;
  args["set"] = tagset;
  args["confidence"] = toString(conf);
  args["generate_id"] = chunks->id();
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }
  Chunk *chunk = 0;
#pragma omp critical (foliaupdate)
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
#pragma omp critical (foliaupdate)
    {
      chunk->append( word );
    }
  }
}

void IOBTagger::addIOBTags( const vector<Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() ){
    return;
  }
  ChunkingLayer *el = 0;
#pragma omp critical (foliaupdate)
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
  for ( size_t i=0; i < tags.size(); ++i ){
    if (debug){
      LOG << "tag = " << tags[i] << endl;
    }
    vector<string> tagwords;
    size_t num_words = TiCC::split_at( tags[i], tagwords, "_" );
    if ( num_words != 2 ){
      LOG << "expected <POS>_<IOB>, got: " << tags[i] << endl;
      throw;
    }
    vector<string> iob;
    if (debug){
      LOG << "IOB = " << tagwords[1] << endl;
    }
    if ( tagwords[1] == "O" ){
      if ( !stack.empty() ){
	if (debug) {
	  LOG << "O spit out " << curIOB << endl;
	  using TiCC::operator<<;
	  LOG << "spit out " << stack << endl;
	}
	addChunk( el, stack, dstack, curIOB );
	dstack.clear();
	stack.clear();
      }
      continue;
    }
    else {
      num_words = TiCC::split_at( tagwords[1], iob, "-" );
      if ( num_words != 2 ){
	LOG << "expected <IOB>-tag, got: " << tagwords[1] << endl;
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
	addChunk( el, stack, dstack, curIOB );
	dstack.clear();
	stack.clear();
      }
      curIOB = iob[1];
    }
    dstack.push_back( confs[i] );
    stack.push_back( words[i] );
  }
  if ( !stack.empty() ){
    if ( debug ){
      LOG << "END spit out " << curIOB << endl;
      using TiCC::operator<<;
      LOG << "spit out " << stack << endl;
    }
    addChunk( el, stack, dstack, curIOB );
  }
}

void IOBTagger::addDeclaration( Document& doc ) const {
#pragma omp critical (foliaupdate)
  {
    doc.declare( AnnotationType::CHUNKING,
		 tagset,
		 "annotator='frog-chunker-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void IOBTagger::post_process( const std::vector<folia::Word*>& swords ){
  vector<double> conf;
  vector<string> tags;
  for ( const auto& tag : _tag_result ){
    tags.push_back( tag.assignedTag() );
    conf.push_back( tag.confidence() );
  }
  addIOBTags( swords, tags, conf );
}
