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
#include "frog/ner_tagger_mod.h"

using namespace std;
using namespace folia;
using namespace TiCC;
using namespace Tagger;

const int KNOWN_NERS_SIZE = 10;

#define LOG *Log(nerLog)

NERTagger::NERTagger(TiCC::LogStream * logstream):
  tagger(0),
  debug(0),
  filter(0)
{
  nerLog = new LogStream( logstream, "ner-" );
  known_ners.resize( KNOWN_NERS_SIZE+1 );
}

NERTagger::~NERTagger(){
  delete tagger;
  delete nerLog;
  delete filter;
}

bool NERTagger::init( const Configuration& config ){
  debug = 0;
  string val = config.lookUp( "debug", "NER" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  switch ( debug ){
  case 0:
  case 1:
    nerLog->setlevel(LogNormal);
    break;
  case 2:
  case 3:
  case 4:
    nerLog->setlevel(LogDebug);
    break;
  case 5:
  case 6:
  case 7:
    nerLog->setlevel(LogHeavy);
    break;
  default:
    nerLog->setlevel(LogExtreme);
  }
  if (debug){
    LOG << "NER Tagger Init" << endl;
  }
  if ( tagger != 0 ){
    LOG << "NER Tagger is already initialized!" << endl;
    return false;
  }
  val = config.lookUp( "settings", "NER" );
  if ( val.empty() ){
    LOG << "Unable to find settings for NER" << endl;
    return false;
  }
  string settings;
  if ( val[0] == '/' ){
    // an absolute path
    settings = val;
  }
  else {
    settings = config.configDir() + val;
  }
  val = config.lookUp( "version", "NER" );
  if ( val.empty() ){
    version = "1.0";
  }
  else {
    version = val;
  }
  val = config.lookUp( "set", "NER" );
  if ( val.empty() ){
    tagset = "http://ilk.uvt.nl/folia/sets/frog-ner-nl";
  }
  else {
    tagset = val;
  }
  string charFile = config.lookUp( "char_filter_file", "NER" );
  if ( charFile.empty() )
    charFile = config.lookUp( "char_filter_file" );
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new Tokenizer::UnicodeFilter();
    filter->fill( charFile );
  }
  val = config.lookUp( "known_ners", "NER" );
  if ( !val.empty() ){
    string file_name;
    if ( val[0] == '/' ) {
      // an absolute path
      file_name = val;
    }
    else {
      file_name = config.configDir() + val;
    }
    if ( !fill_known_ners( file_name ) ){
      LOG << "Unable to fill known NER's from file: '" << file_name << "'" << endl;
      return false;
    }
  }

  string init = "-s " + settings + " -vcf";
  tagger = new MbtAPI( init, *nerLog );
  return tagger->isInit();
}

bool NERTagger::fill_known_ners( const string& file_name ){
  ifstream is( file_name );
  if ( !is ){
    return false;
  }
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    vector<string> parts;
    if ( TiCC::split_at( line, parts, "\t" ) != 2 ){
      LOG << "expected 2 TAB-separated parts in line: '" << line << "'" << endl;
      return false;
    }
    line = parts[0];
    string ner_value = parts[1];
    size_t num = TiCC::split( line, parts );
    if ( num < 1 || num > KNOWN_NERS_SIZE ){
      LOG << "expected 1 to " << KNOWN_NERS_SIZE
		   << " SPACE-separated parts in line: '" << line
		   << "'" << endl;
      return false;
    }
    line = "";
    for ( const auto& part : parts ){
      line += part;
      if ( &part != &parts.back() ){
	line += " ";
      }
    }
    known_ners[num][line] = ner_value;
  }
  return true;
}

size_t count_sp( const string& sentence, string::size_type pos ){
  int sp = 0;
  for ( string::size_type i=0; i < pos; ++i ){
    if ( sentence[i] == ' ' ){
      ++sp;
    }
  }
  return sp;
}

void NERTagger::handle_known_ners( const vector<string>& words,
				   vector<string>& tags ){
  if ( debug ){
    LOG << "search for known NER's" << endl;
  }
  string sentence = " ";
  for ( const auto& w : words ){
    sentence += w + " ";
  }
  // so sentence starts AND ends with a space!
  if ( debug ){
    LOG << "Sentence = " << sentence << endl;
  }
  for ( size_t i = KNOWN_NERS_SIZE; i > 0; --i ){
    auto const& mp = known_ners[i];
    if ( mp.empty() ){
      continue;
    }
    for( auto const& it : mp ){
      string blub = " " + it.first + " ";
      string::size_type pos = sentence.find( blub );
      while ( pos != string::npos ){
	size_t sp = count_sp( sentence, pos );
	if ( debug ){
	  LOG << "matched '" << it.first << "' to '"
		       << sentence << "' at position " << sp
		       << " : " << it.second << endl;
	}
	bool safe = true;
	for ( size_t j=0; j < i && safe; ++j ){
	  safe = ( tags[sp+j] == "O" );
	}
	if ( safe ){
	  // we can safely change the tag (don't trample upon hits of longer known ners!)
	  tags[sp] = "B-" + it.second;
	  for ( size_t j=1; j < i; ++j ){
	    tags[sp+j] = "I-" + it.second;
	  }
	}
	pos = sentence.find( blub, pos + blub.length() );
      }
    }
  }
}

void NERTagger::merge( const vector<string>& ktags, vector<string>& tags,
		       vector<double>& conf ){
  if ( debug ){
    using TiCC::operator<<;
    LOG << "merge " << ktags << endl << "with  " << tags << endl;
  }
  for ( size_t i=0; i < ktags.size(); ++i ){
    if ( ktags[i] == "O" ){
      if ( i > 0 && ktags[i-1] != "O" ){
	// so we did some merging.  check that we aren't in the middle of some tag now
	size_t j = i;
	while ( j < tags.size() && tags[j][0] == 'I' ) {
	  tags[j] = "O";
	  ++j;
	}
      }
      continue;
    }
    else if ( ktags[i][0] == 'B' ){
      // maybe we landed in the middel of some tag.
      if ( tags[i][0] == 'I' ){
	//indeed, so erase it backwards
	size_t j = i;
	while ( tags[j][0] == 'I' ){
	  tags[j] = "O";
	  --j;
	}
	tags[j] = "O";
      }
      // now copy
      tags[i] = ktags[i];
      conf[i] = 1.0;
    }
    else {
      tags[i] = ktags[i];
      conf[i] = 1.0;
    }
  }
  if ( debug ){
    LOG << "Merge gave " << tags << endl;
  }
}

static void addEntity( Sentence *sent,
		       const string& tagset,
		       const vector<Word*>& words,
		       const vector<double>& confs,
		       const string& NER ){
  EntitiesLayer *el = 0;
#pragma omp critical(foliaupdate)
  {
    try {
      el = sent->annotation<EntitiesLayer>();
    }
    catch(...){
      KWargs args;
      args["generate_id"] = sent->id();
      el = new EntitiesLayer( args, sent->doc() );
      sent->append( el );
    }
  }
  double c = 0;
  for ( auto const& val : confs ){
    c += val;
  }
  c /= confs.size();
  KWargs args;
  args["class"] = NER;
  args["confidence"] =  toString(c);
  args["set"] = tagset;
  args["generate_id"] = el->id();
  Entity *e = 0;
#pragma omp critical(foliaupdate)
  {
    e = new Entity( args, el->doc() );
    el->append( e );
  }
  for ( const auto& word : words ){
#pragma omp critical(foliaupdate)
    {
      e->append( word );
    }
  }
}

void NERTagger::addNERTags( const vector<Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() ) {
    return;
  }
  Sentence *sent = words[0]->sentence();
  vector<Word*> stack;
  vector<double> dstack;
  string curNER;
  for ( size_t i=0; i < tags.size(); ++i ){
    if (debug){
      LOG << "NER = " << tags[i] << endl;
    }
    vector<string> ner;
    if ( tags[i] == "O" ){
      if ( !stack.empty() ){
	if (debug) {
	  LOG << "O spit out " << curNER << endl;
	  using TiCC::operator<<;
	  LOG << "ners  " << stack << endl;
	  LOG << "confs " << dstack << endl;
	}
	addEntity( sent, tagset, stack, dstack, curNER );
	dstack.clear();
	stack.clear();
      }
      continue;
    }
    else {
      size_t num_words = TiCC::split_at( tags[i], ner, "-" );
      if ( num_words != 2 ){
	LOG << "expected <NER>-tag, got: " << tags[i] << endl;
	throw runtime_error( "NER: unable to retrieve a NER tag from: "
			     + tags[i] );
      }
    }
    if ( ner[0] == "B" ||
	 ( ner[0] == "I" && stack.empty() ) ||
	 ( ner[0] == "I" && ner[1] != curNER ) ){
      // an I without preceding B is handled as a B
      // an I with a different TAG is also handled as a B
      if ( !stack.empty() ){
	if ( debug ){
	  LOG << "B spit out " << curNER << endl;
	  using TiCC::operator<<;
	  LOG << "spit out " << stack << endl;
	}
	addEntity( sent, tagset, stack, dstack, curNER );
	dstack.clear();
	stack.clear();
      }
      curNER = ner[1];
    }
    dstack.push_back( confs[i] );
    stack.push_back( words[i] );
  }
  if ( !stack.empty() ){
    if ( debug ){
      LOG << "END spit out " << curNER << endl;
      using TiCC::operator<<;
      LOG << "spit out " << stack << endl;
    }
    addEntity( sent, tagset, stack, dstack, curNER );
  }
}

void NERTagger::addDeclaration( Document& doc ) const {
#pragma omp critical(foliaupdate)
  {
    doc.declare( AnnotationType::ENTITY,
		 tagset,
		 "annotator='frog-ner-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void NERTagger::Classify( const vector<Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    string sentence; // the tagger needs the whole sentence
    for ( const auto& sw : swords ){
      UnicodeString word;
#pragma omp critical(foliaupdate)
      {
	word = sw->text();
      }
      if ( filter )
	word = filter->filter( word );
      sentence += UnicodeToUTF8(word);
      words.push_back( UnicodeToUTF8(word) );
      if ( &sw != &swords.back() ){
	sentence += " ";
      }
    }
    if (debug){
      LOG << "NER in: " << sentence << endl;
    }
    vector<TagResult> tagv = tagger->TagLine(sentence);
    if ( tagv.size() != swords.size() ){
      string out;
      for ( const auto& val : tagv ){
	out += val.word() + "//" + val.assignedTag() + " ";
      }
      if ( debug ){
	LOG << "NER tagger is confused" << endl;
	LOG << "sentences was: '" << sentence << "'" << endl;
	LOG << "but tagged:" << endl;
	LOG << out << endl;
      }
      throw runtime_error( "NER failed: '" + sentence + "' ==> '" + out + "'" );
    }
    if ( debug ){
      LOG << "NER tagger out: " << endl;
      for ( size_t i=0; i < tagv.size(); ++i ){
	LOG << "[" << i << "] : word=" << tagv[i].word()
	    << " tag=" << tagv[i].assignedTag()
	    << " confidence=" << tagv[i].confidence() << endl;
      }
    }
    vector<double> conf;
    vector<string> tags;
    for ( const auto& tag : tagv ){
      tags.push_back( tag.assignedTag() );
      conf.push_back( tag.confidence() );
    }
    vector<string> ktags( tagv.size(), "O" );
    handle_known_ners( words, ktags );
    merge( ktags, tags, conf );
    addNERTags( swords, tags, conf );
  }
}

vector<TagResult> NERTagger::tagLine( const string& line ){
  if ( tagger ){
    return tagger->TagLine(line);
  }
  throw runtime_error( "NERTagger is not initialized" );
}

string NERTagger::set_eos_mark( const string& eos ){
  if ( tagger ){
    return tagger->set_eos_mark(eos);
  }
  throw runtime_error( "NERTagger is not initialized" );
}
