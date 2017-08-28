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
#include "ticcutils/FileUtils.h"
#include "frog/ner_tagger_mod.h"

using namespace std;
using namespace TiCC;
using namespace Tagger;

#define LOG *Log(tag_log)

bool NERTagger::init( const Configuration& config ){
  if ( !POSTagger::init( config ) ){
    return false;
  }
  string val = config.lookUp( "max_ner_size", _label );
  if ( !val.empty() ){
    max_ner_size = TiCC::stringTo<int>( val );
  }
  known_ners.resize( max_ner_size + 1 );
  val = config.lookUp( "known_ners", _label );
  if ( !val.empty() ){
    if ( !fill_known_ners( val, config.configDir() ) ){
      return false;
    }
  }
  return true;
}

bool NERTagger::fill_known_ners( const string& name, const string& config_dir ){
  string file_name = name;
  if ( name[0] != '/' ) {
    file_name = config_dir + file_name;
  }
  ifstream is( file_name );
  if ( !is ){
    LOG << "unable to load additional NE from " << file_name << endl;
    return false;
  }
  int err_cnt = 0;
  int long_err_cnt = 0;
  size_t ner_cnt = 0;
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    if ( TiCC::isFile( line ) ){
      if ( !fill_known_ners( line, "" ) ){
	LOG << "unable to load additional NE from included file: "
	    << line << endl;
	return false;
      }
    }
    else if ( TiCC::isFile( config_dir + line ) ){
      if ( !fill_known_ners( line, config_dir ) ){
	LOG << "unable to load additional NE from include file: "
	    << config_dir + line << endl;
	return false;
      }
    }
    else {
      vector<string> parts;
      if ( TiCC::split_at( line, parts, "\t" ) != 2 ){
	LOG << "expected 2 TAB-separated parts in line: '" << line << "'" << endl;
	if ( ++err_cnt > 50 ){
	  LOG << "too many errors in additional wordlist file: " << file_name << endl;
	  return false;
	}
	else {
	  LOG << "ignoring entry" << endl;
	  continue;
	}
      }
      line = parts[0];
      string ner_value = parts[1];
      size_t num = TiCC::split( line, parts );
      if ( num == 1 ){
	// ignore single word NE's
	continue;
      }
      if ( num < 1 || num > (unsigned)max_ner_size ){
	// LOG << "expected 1 to " << max_ner_size
	// 		   << " SPACE-separated parts in line: '" << line
	// 		   << "'" << endl;
	if ( ++long_err_cnt > 50 ){
	  LOG << "too many long entries in additional wordlist file. " << file_name << endl;
	  LOG << "consider raising the max_ner_size in the configuration. (now "
	      << max_ner_size << ")" << endl;
	  return false;
	}
	else {
	//	LOG << "ignoring entry" << endl;
	  continue;
	}
      }
      line = "";
      for ( const auto& part : parts ){
	line += part;
	if ( &part != &parts.back() ){
	  line += " ";
	}
      }
      known_ners[num][line] = ner_value;
      ++ner_cnt;
    }
  }
  LOG << "loaded " << ner_cnt << " additional Named Entities from"
      << file_name << endl;
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
  for ( size_t i = max_ner_size; i > 0; --i ){
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
      // maybe we landed in the middle of some tag.
      if ( i > 0 && tags[i][0] == 'I' ){
	// indeed, so erase it backwards
	// except when at the start. then an 'I' should be a 'B' anyway
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

static void addEntity( folia::Sentence *sent,
		       const string& tagset,
		       const vector<folia::Word*>& words,
		       const vector<double>& confs,
		       const string& NER,
		       const string& textclass ){
  folia::EntitiesLayer *el = 0;
#pragma omp critical (foliaupdate)
  {
    try {
      el = sent->annotation<folia::EntitiesLayer>();
    }
    catch(...){
      folia::KWargs args;
      args["generate_id"] = sent->id();
      el = new folia::EntitiesLayer( args, sent->doc() );
      sent->append( el );
    }
  }
  double c = 0;
  for ( auto const& val : confs ){
    c += val;
  }
  c /= confs.size();
  folia::KWargs args;
  args["class"] = NER;
  args["confidence"] =  toString(c);
  args["set"] = tagset;
  args["generate_id"] = el->id();
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }
  folia::Entity *e = 0;
#pragma omp critical (foliaupdate)
  {
    e = new folia::Entity( args, el->doc() );
    el->append( e );
  }
  for ( const auto& word : words ){
#pragma omp critical (foliaupdate)
    {
      e->append( word );
    }
  }
}

void NERTagger::addNERTags( const vector<folia::Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() ) {
    return;
  }
  folia::Sentence *sent = words[0]->sentence();
  vector<folia::Word*> stack;
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
	addEntity( sent, tagset, stack, dstack, curNER, textclass );
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
	addEntity( sent, tagset, stack, dstack, curNER, textclass );
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
    addEntity( sent, tagset, stack, dstack, curNER, textclass );
  }
}

void NERTagger::addDeclaration( folia::Document& doc ) const {
#pragma omp critical (foliaupdate)
  {
    doc.declare( folia::AnnotationType::ENTITY,
		 tagset,
		 "annotator='frog-ner-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void NERTagger::Classify( const vector<folia::Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    string sentence = extract_sentence( swords, words );
    if (debug){
      LOG << "NER in: " << sentence << endl;
    }
    vector<TagResult> tagv = tagger->TagLine(sentence);
    if ( tagv.size() != swords.size() ){
      LOG << "NER tagger is confused" << endl;
      throw runtime_error( "NER is confused" );
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
