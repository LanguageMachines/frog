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
#include "frog/enr_ner_tagger_mod.h"

using namespace std;
using namespace TiCC;
using namespace Tagger;

#define LOG *Log(tag_log)

// should come from the config!
const string cgn_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";

bool ENERTagger::init( const Configuration& config ){
  if ( !BaseTagger::init( config ) ){
    return false;
  }
  string val = config.lookUp( "max_ner_size", "NER" );
  if ( !val.empty() ){
    max_ner_size = TiCC::stringTo<int>( val );
  }
  known_ners.resize( max_ner_size + 1 );
  val = config.lookUp( "known_ners", "NER" );
  if ( !val.empty() ){
    if ( !read_gazets( val, config.configDir() ) ){
      return false;
    }
  }
  return true;
}

bool ENERTagger::fill_ners( const string& cat,
			    const string& name,
			    const string& config_dir ){
  string file_name = name;
  if ( !TiCC::isFile( file_name ) ){
    file_name = config_dir + "/" + name;
    if ( !TiCC::isFile( file_name ) ){
      LOG << "unable to load additional NE from file: " << file_name << endl;
      return false;
    }
  }
  ifstream is( file_name );
  int long_err_cnt = 0;
  size_t ner_cnt = 0;
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    else {
      vector<string> parts;
      size_t num = TiCC::split( line, parts );
      if ( num > (unsigned)max_ner_size ){
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
      // reconstruct the NER with single spaces
      line = "";
      for ( const auto& part : parts ){
	line += part;
	if ( &part != &parts.back() ){
	  line += " ";
	}
      }
      known_ners[num][line].insert( cat );
      ++ner_cnt;
    }
  }
  LOG << "loaded " << ner_cnt << " additional " << cat
      << " Named Entities from: " << file_name << endl;
  return true;
}

bool ENERTagger::read_gazets( const string& name, const string& config_dir ){
  string file_name = name;
  if ( name[0] != '/' ) {
    file_name = config_dir + "/" + file_name;
  }
  ifstream is( file_name );
  if ( !is ){
    LOG << "Unable to find Named Enties file " << file_name << endl;
    return false;
  }
  LOG << "READ  " << file_name << endl;
  int err_cnt = 0;
  size_t file_cnt = 0;
  string line;
  while ( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' ){
      continue;
    }
    // we search for entries of the form 'category\tfilename'
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
    string cat  = parts[0];
    string file = parts[1];
    if ( fill_ners( cat, file, config_dir ) ){
      ++file_cnt;
    }
  }
  if ( file_cnt < 1 ){
    LOG << "unable to load any additional Named Enties." << endl;
    return false;
  }
  else {
    LOG << "loaded " << file_cnt << " additional Named Entities files" << endl;
    return true;
  }
}

static size_t count_sp( const string& sentence, string::size_type pos ){
  int sp = 0;
  for ( string::size_type i=0; i < pos; ++i ){
    if ( sentence[i] == ' ' ){
      ++sp;
    }
  }
  return sp;
}

static void serialize( const vector<set<string>>& stags, vector<string>& tags ){
  size_t pos = 0;
  for ( const auto& it : stags ){
    // using TiCC::operator<<;
    // cerr << "stag[" << pos << "] =" << it << endl;
    if ( !it.empty() ){
      string res;
      for ( const auto& s : it ){
	res += s + "+";
      }
      //      cerr << "res=" << res << endl;
      tags[pos] = res;
    }
    ++pos;
  }
}

void ENERTagger::create_ner_list( const vector<string>& words,
				 vector<string>& tags ){
  tags.clear();
  tags.resize( words.size(), "O" );
  vector<set<string>> stags( words.size() );
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
      if ( debug ){
	LOG << "blub = " << blub << endl;
      }
      string::size_type pos = sentence.find( blub );
      while ( pos != string::npos ){
	size_t sp = count_sp( sentence, pos );
	if ( debug ){
	  LOG << "matched '" << it.first << "' to '"
	      << sentence << "' at position " << sp
	      << " : " << it.second << endl;
	}
	for ( size_t j=0; j < i; ++j ){
	  stags[sp+j].insert( it.second.begin(), it.second.end() );
	}
	pos = sentence.find( blub, pos + blub.length() );
      }
    }
  }
  serialize( stags, tags );
}

static void addEntity( folia::Sentence *sent,
		       const string& tagset,
		       const vector<folia::Word*>& words,
		       const vector<double>& confs,
		       const string& NER ){
  folia::EntitiesLayer *el = 0;
#pragma omp critical(foliaupdate)
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
  folia::Entity *e = 0;
#pragma omp critical(foliaupdate)
  {
    e = new folia::Entity( args, el->doc() );
    el->append( e );
  }
  for ( const auto& word : words ){
#pragma omp critical(foliaupdate)
    {
      e->append( word );
    }
  }
}

void ENERTagger::addNERTags( const vector<folia::Word*>& words,
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

void ENERTagger::addDeclaration( folia::Document& doc ) const {
#pragma omp critical(foliaupdate)
  {
    doc.declare( folia::AnnotationType::ENTITY,
		 tagset,
		 "annotator='frog-ner-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

void ENERTagger::Classify( const vector<folia::Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    vector<string> ptags;
    extract_words_tags( swords, cgn_tagset, words, ptags );
    vector<string> ktags;
    create_ner_list( words, ktags );
    string text_block;
    string prev = "_";
    string prevN = "_";
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
      string ktag = ktags[i];
      text_block += "\t" + prevN + "\t" + ktag + "\t";
      prevN = ktag;
      if ( i < swords.size() - 1 ){
	text_block += ktags[i+1];
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
      LOG << "NER tagger out: " << endl;
      for ( size_t i=0; i < _tag_result.size(); ++i ){
	LOG << "[" << i << "] : word=" << _tag_result[i].word()
	    << " tag=" << _tag_result[i].assignedTag()
	    << " confidence=" << _tag_result[i].confidence() << endl;
      }
    }
  }
  post_process( swords );
}

void ENERTagger::post_process( const std::vector<folia::Word*>& swords ){
  vector<string> tags;
  vector<double> conf;
  for ( const auto& tag : _tag_result ){
    tags.push_back( tag.assignedTag() );
    conf.push_back( tag.confidence() );
  }
  addNERTags( swords, tags, conf );
}

bool ENERTagger::Generate( const std::string& opt_line ){
  return tagger->GenerateTagger( opt_line );
}
