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
#include "frog/Frog.h"
#include "frog/iob_tagger_mod.h"

using namespace std;
using namespace folia;
using namespace TiCC;
using namespace Tagger;

IOBTagger::IOBTagger(TiCC::LogStream * logstream){
  tagger = 0;
  iobLog = new LogStream( logstream, "iob-" );
}

IOBTagger::~IOBTagger(){
  delete tagger;
  delete iobLog;
}

bool IOBTagger::init( const Configuration& config ){
  debug = 0;
  string val = config.lookUp( "debug", "IOB" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  switch ( debug ){
  case 0:
  case 1:
    iobLog->setlevel(LogNormal);
    break;
  case 2:
  case 3:
  case 4:
    iobLog->setlevel(LogDebug);
    break;
  case 5:
  case 6:
  case 7:
    iobLog->setlevel(LogHeavy);
    break;
  default:
    iobLog->setlevel(LogExtreme);
  }
  if (debug)
    *Log(iobLog) << "IOB Chunker Init" << endl;
  if ( tagger != 0 ){
    *Log(iobLog) << "IOBTagger is already initialized!" << endl;
    return false;
  }
  val = config.lookUp( "settings", "IOB" );
  if ( val.empty() ){
    *Log(iobLog) << "Unable to find settings for IOB" << endl;
    return false;
  }
  string settings;
  if ( val[0] == '/' ) // an absolute path
    settings = val;
  else
    settings = config.configDir() + val;

  val = config.lookUp( "version", "IOB" );
  if ( val.empty() ){
    version = "1.0";
  }
  else
    version = val;
  val = config.lookUp( "set", "IOB" );
  if ( val.empty() ){
    tagset = "http://ilk.uvt.nl/folia/sets/frog-chunker-nl";
  }
  else
    tagset = val;

  string init = "-s " + settings + " -vcf";
  tagger = new MbtAPI( init, *iobLog );
  return tagger->isInit();
}

void IOBTagger::addChunk( ChunkingLayer *chunks,
			  const vector<Word*>& words,
			  const vector<double>& confs,
			  const string& IOB ){
  double conf = 1;
  for ( size_t i=0; i < confs.size(); ++i )
    conf *= confs[i];
  KWargs args;
  args["class"] = IOB;
  args["set"] = tagset;
  args["confidence"] = toString(conf);
  args["generate_id"] = chunks->id();
  Chunk *chunk = 0;
#pragma omp critical(foliaupdate)
  {
    try {
      chunk = new Chunk( chunks->doc(), args );
      chunks->append( chunk );
    }
    catch ( exception& e ){
      *Log(iobLog) << "addChunk failed: " << e.what() << endl;
      exit( EXIT_FAILURE );
    }
  }
  for ( size_t i=0; i < words.size(); ++i ){
    if ( words[i]->isinstance(PlaceHolder_t) )
      continue;
#pragma omp critical(foliaupdate)
    {
      chunk->append( words[i] );
    }
  }
}

void IOBTagger::addIOBTags( const vector<Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() )
    return;
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
      el = new ChunkingLayer(sent->doc(),args);
      sent->append( el );
    }
  }
  vector<Word*> stack;
  vector<double> dstack;
  string curIOB;
  for ( size_t i=0; i < tags.size(); ++i ){
    if (debug)
      *Log(iobLog) << "tag = " << tags[i] << endl;
    vector<string> tagwords;
    size_t num_words = TiCC::split_at( tags[i], tagwords, "_" );
    if ( num_words != 2 ){
      *Log(iobLog) << "expected <POS>_<IOB>, got: " << tags[i] << endl;
      exit( EXIT_FAILURE );
    }
    vector<string> iob;
    if (debug)
      *Log(iobLog) << "IOB = " << tagwords[1] << endl;
    if ( tagwords[1] == "O" ){
      if ( !stack.empty() ){
	if (debug) {
	  *Log(iobLog) << "O spit out " << curIOB << endl;
	  using TiCC::operator<<;
	  *Log(iobLog) << "spit out " << stack << endl;
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
	*Log(iobLog) << "expected <IOB>-tag, got: " << tagwords[1] << endl;
	exit( EXIT_FAILURE );
      }
    }
    if ( iob[0] == "B" ||
	 ( iob[0] == "I" && stack.empty() ) ||
	 ( iob[0] == "I" && iob[1] != curIOB ) ){
      // an I without preceding B is handled as a B
      // an I with a different TAG is also handled as a B
      if ( !stack.empty() ){
	if ( debug ){
	  *Log(iobLog) << "B spit out " << curIOB << endl;
	  using TiCC::operator<<;
	  *Log(iobLog) << "spit out " << stack << endl;
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
      *Log(iobLog) << "END spit out " << curIOB << endl;
      using TiCC::operator<<;
      *Log(iobLog) << "spit out " << stack << endl;
    }
    addChunk( el, stack, dstack, curIOB );
  }
}

void IOBTagger::addDeclaration( Document& doc ) const {
#pragma omp critical(foliaupdate)
  {
    doc.declare( AnnotationType::CHUNKING,
		 tagset,
		 "annotator='frog-chunker-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void IOBTagger::Classify( const vector<Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    string sentence; // the tagger needs the whole sentence
    for ( size_t w = 0; w < swords.size(); ++w ) {
      string wrd;
#pragma omp critical(foliaupdate)
      {
	wrd = swords[w]->str();
      }
      sentence += wrd;
      words.push_back( wrd );
      if ( w < swords.size()-1 )
	sentence += " ";
    }
    if (debug)
      *Log(iobLog) << "IOB in: " << sentence << endl;
    vector<TagResult> tagv = tagger->TagLine(sentence);
    if ( tagv.size() != swords.size() ){
      throw runtime_error( "IOB tagger is confused" );
    }
    if ( debug ){
      *Log(iobLog) << "IOB tagger out: " << endl;
      for ( size_t i=0; i < tagv.size(); ++i ){
	*Log(iobLog) << "[" << i << "] : word=" << tagv[i].word()
		     << " tag=" << tagv[i].assignedTag()
		     << " confidence=" << tagv[i].confidence() << endl;
      }
    }
    vector<double> conf;
    vector<string> tags;
    for ( size_t i=0; i < tagv.size(); ++i ){
      tags.push_back( tagv[i].assignedTag() );
      conf.push_back( tagv[i].confidence() );
    }
    addIOBTags( swords, tags, conf );
  }
}

string IOBTagger::set_eos_mark( const string& eos ){
  if ( tagger )
    return tagger->set_eos_mark(eos);
  else
    throw runtime_error( "IOBTagger is not initialized" );
}
