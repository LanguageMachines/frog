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

#include "frog/ner_tagger_mod.h"

#include "mbt/MbtAPI.h"
#include "frog/Frog-util.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"

using namespace std;
using namespace Tagger;
using TiCC::operator<<;

#define LOG *TiCC::Log(tag_log)

static string POS_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";

NERTagger::NERTagger( TiCC::LogStream *l ):
  BaseTagger( l, "NER" ),
  gazets_only(false),
  max_ner_size(20)
{ gazet_ners.resize( max_ner_size + 1 );
  override_ners.resize( max_ner_size + 1 );
}

bool NERTagger::init( const TiCC::Configuration& config ){
  if ( !BaseTagger::init( config ) ){
    return false;
  }
  string val = config.lookUp( "max_ner_size", "NER" );
  if ( !val.empty() ){
    max_ner_size = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "known_ners", "NER" );
  if ( !val.empty() ){
    if ( !read_gazets( val, config.configDir() ) ){
      return false;
    }
  }
  val = config.lookUp( "ner_override", "NER" );
  if ( !val.empty() ){
    if ( !read_overrides( val, config.configDir() ) ){
      return false;
    }
  }
  val = config.lookUp( "only_gazets", "NER" );
  if ( !val.empty() ){
    gazets_only = TiCC::stringTo<bool>( val );
  }
  val = config.lookUp( "set", "tagger" );
  if ( !val.empty() ){
    POS_tagset = val;
  }
  return true;
}

bool NERTagger::fill_ners( const string& cat,
			   const string& name,
			   const string& config_dir,
			   vector<unordered_map<string,set<string>>>& ners ){
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
      vector<string> parts = TiCC::split( line );
      if ( parts.size() > (unsigned)max_ner_size ){
	if ( ++long_err_cnt > 50 ){
	  LOG << "too many long entries in additional wordlist file. " << file_name << endl;
	  LOG << "consider raising the max_ner_size in the configuration. (now "
	      << max_ner_size << ")" << endl;
	  return false;
	}
	else {
	  if ( debug > 1 ){
	    LOG << "expected 1 to " << max_ner_size
		<< " SPACE-separated parts in line: '" << line
		<< "', ignoring it" << endl;
	  }
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
      ners[parts.size()][line].insert( cat );
      ++ner_cnt;
    }
  }
  LOG << "loaded " << ner_cnt << " additional " << cat
      << " Named Entities from: " << file_name << endl;
  return true;
}

bool NERTagger::read_gazets( const string& name,
			     const string& config_dir,
			     vector<unordered_map<string,set<string>>>& ners ){
  string file_name = name;
  string lookup_dir = config_dir;
  if ( name[0] != '/' ) {
    file_name = config_dir + "/" + file_name;
  }
  else {
    lookup_dir = TiCC::dirname( file_name );
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
    if ( fill_ners( cat, file, lookup_dir, ners ) ){
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

static vector<string> serialize( const vector<set<string>>& stags ){
  // for every non empty set {el1,el2,..}, we compose a string like: el1+el2+...
  vector<string> ambitags( stags.size(), "O" );
  size_t pos = 0;
  for ( const auto& it : stags ){
    if ( !it.empty() ){
      string res;
      for ( const auto& s : it ){
	res += s + "+";
      }
      ambitags[pos] = res;
    }
    ++pos;
  }
  return ambitags;
}

vector<string> NERTagger::create_ner_list( const vector<string>& words,
					   std::vector<std::unordered_map<std::string,std::set<std::string>>>& ners ){
  vector<set<string>> stags( words.size() );
  if ( debug > 1 ){
    LOG << "search for known NER's" << endl;
  }
  for ( size_t j=0; j < words.size(); ++j ){
    // cycle through the words
    string seq;
    size_t len = 1;
    for ( size_t i = 0; i < min( words.size() - j, (size_t)max_ner_size); ++i ){
      // start looking for sequences of length len
      auto const& mp = ners[len++];
      if ( mp.empty() ){
	continue;
      }
      seq += words[j+i];
      if ( debug > 1 ){
	LOG << "sequence = '" << seq << "'" << endl;
      }
      auto const& tags = mp.find(seq);
      if ( tags != mp.end() ){
	if ( debug > 1 ){
	  LOG << "FOUND tags " << tags->first << "-" << tags->second << endl;
	}
	for ( size_t k = 0; k <= i; ++k ){
	  stags[k+j].insert( tags->second.begin(), tags->second.end() );
	}
      }
      seq += " ";
    }
  }
  return serialize( stags );
}

void NERTagger::addEntity( folia::Sentence *sent,
			   const vector<pair<folia::Word*,double>>& entity,
			   const string& NER ){
  /// add the sequence of Words in @entity with class @NER to the Sentence @sent
  /// may create an EntitiesLayer in the Sentence, if not already present
  folia::EntitiesLayer *el = 0;
  if ( debug > 8 ){
    LOG << "add NER " << NER << " to entities layer" << endl;
  }

#pragma omp critical (foliaupdate)
  {
    try {
      el = sent->annotation<folia::EntitiesLayer>(tagset);
      if ( debug > 8 ){
	LOG << "GOT existing layer " << el << endl;
      }
    }
    catch(...){
      folia::KWargs args;
      string s_id = sent->id();
      if ( !s_id.empty() ){
	args["generate_id"] = s_id;
      }
      args["set"] = tagset;
      el = new folia::EntitiesLayer( args, sent->doc() );
      if ( debug > 8 ){
	LOG << "CREATED layer " << el << endl;
      }
      sent->append( el );
    }
  }
  double c = 0;
  for ( auto const& it : entity ){
    c += it.second;
  }
  c /= entity.size();
  folia::KWargs args;
  args["class"] = NER;
  args["confidence"] = TiCC::toString(c);
  args["set"] = tagset;
  string el_id = el->id();
  if ( !el_id.empty() ){
    args["generate_id"] = el_id;
  }
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }
  folia::Entity *e = 0;
#pragma omp critical (foliaupdate)
  {
    e = new folia::Entity( args, el->doc() );
    el->append( e );
  }
  for ( const auto& it : entity ){
#pragma omp critical (foliaupdate)
    {
      e->append( it.first );
    }
  }
}

void NERTagger::post_process( const vector<folia::Word*>& words,
			      const vector<tc_pair>& ners ){
  /// @ners is a sequence of NE tags (maybe 'O') with their confidence
  /// these are appended to the corresponding @words
  /// On the fly, classification errors are fixed:
  /// - a NE starting with with I is added as starting with B
  /// - a change in NE value is handled as a new NE start
  if ( words.empty() ) {
    return;
  }
  folia::Sentence *sent = words[0]->sentence();
  vector<pair<folia::Word*,double>> entity;
  string curNER;
  size_t pos = 0;
  for ( const auto& it : ners ){
    if (debug > 1){
      LOG << "NER = " << it.first << endl;
    }
    vector<string> ner;
    if ( it.first == "O" ){
      if ( !entity.empty() ){
	if (debug > 1) {
	  LOG << "O spit out " << curNER << endl;
	  LOG << "entity= " << entity << endl;
	}
	addEntity( sent, entity, curNER );
	entity.clear();
      }
      ++pos;
      continue;
    }
    if ( it.first[0] == 'B' ){
      if ( !entity.empty() ){
	if ( debug > 1 ){
	  LOG << "B spit out " << curNER << endl;
	  LOG << "entity= " << entity << endl;
	}
	addEntity( sent, entity, curNER );
	entity.clear();
      }
      size_t num_words = TiCC::split_at( it.first, ner, "-" );
      if ( num_words != 2 ){
	LOG << "expected <NER>-tag, got: " << it.first << endl;
	throw runtime_error( "NER: unable to retrieve a NER tag from: "
			     + it.first );
      }
      curNER = ner[1];
    }
    entity.push_back( make_pair(words[pos++], it.second) );
  }
  if ( !entity.empty() ){
    if ( debug > 1 ){
      LOG << "END spit out " << curNER << endl;
      LOG << "entity= " << entity << endl;
    }
    addEntity( sent, entity, curNER );
  }
}

void NERTagger::addDeclaration( folia::Document& doc ) const {
  doc.declare( folia::AnnotationType::ENTITY,
	       tagset,
	       "annotator='frog-ner-" + version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void NERTagger::Classify( const vector<folia::Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    vector<string> pos_tags;
    extract_words_tags( swords, POS_tagset, words, pos_tags );
    vector<tc_pair> ner_tags;
    if ( gazets_only ){
      vector<string> gazet_tags = create_ner_list( words, gazet_ners );
      string last = "O";
      cerr << "bekijk gazet tags: " << gazet_tags << endl;
      for ( const auto& it : gazet_tags ){
	vector<string> parts = TiCC::split_at( it, "+" );
	string tag = parts[0];
	cerr << "AHA: last = " << last << " Nieuw=" << tag << endl;
	if ( tag == "O" ){
	  last = tag;
	}
	else {
	  if ( tag == last ){
	    tag = "I-" + tag;
	  }
	  else {
	    last = tag;
	    tag = "B-" + tag;
	  }
	}
	cerr << "add a tag: " << tag << endl;
	ner_tags.push_back( make_pair( tag, 1.0 ) );
      }
    }
    else {
      vector<string> gazet_tags = create_ner_list( words, gazet_ners );
      vector<string> override_v = create_ner_list( words, override_ners );
      vector<tc_pair> override_tags;
      for ( const auto& it : override_v ){
	override_tags.push_back( make_pair( it, 1.0 ) );
      }
      string text_block;
      string prev_pos = "_";
      string prev_ner = "_";
      for ( size_t i=0; i < swords.size(); ++i ){
	text_block += words[i] + "\t" + prev_pos + "\t" + pos_tags[i] + "\t";
	prev_pos = pos_tags[i];
	if ( i < swords.size() - 1 ){
	  text_block += pos_tags[i+1];
	}
	else {
	  text_block += "_";
	}
	text_block += "\t" + prev_ner + "\t" + gazet_tags[i] + "\t";
	prev_ner = gazet_tags[i];
	if ( i < swords.size() - 1 ){
	  text_block += gazet_tags[i+1];
	}
	else {
	  text_block += "_";
	}

	text_block += "\t??\n";
      }
      if ( debug > 1 ){
	LOG << "TAGGING TEXT_BLOCK\n" << text_block << endl;
      }
      _tag_result = tagger->TagLine( text_block );
      if ( debug > 1 ){
	LOG << "NER tagger out: " << endl;
	for ( size_t i=0; i < _tag_result.size(); ++i ){
	  LOG << "[" << i << "] : word=" << _tag_result[i].word()
	      << " tag=" << _tag_result[i].assignedTag()
	      << " confidence=" << _tag_result[i].confidence() << endl;
	}
      }
      string last;
      for ( const auto& tag : _tag_result ){
	// we might have to correct for tags that start with 'I-'
	// (the MBT tagger may deliver those)
	string assigned = tag.assignedTag();
	if ( assigned == "O" ){
	  last = "";
	}
	else {
	  vector<string> parts = TiCC::split_at( assigned, "-" );
	  vector<string> vals = TiCC::split_at( parts[1], "+" );
	  string val = vals[0];
	  if ( val == last ){
	    assigned = "I-" + val;
	  }
	  else {
	    if ( debug > 1 ){
	      LOG << "replace " << assigned << " by " << "B-" << val << endl;
	    }
	    last = val;
	    assigned = "B-" + val;
	  }
	}
	ner_tags.push_back( make_pair(assigned, tag.confidence() ) );
      }
      if ( !override_tags.empty() ){
	vector<string> empty;
	merge_override( ner_tags, override_tags, true, empty );
      }
    }
    post_process( swords, ner_tags );
  }
}

void NERTagger::post_process( const vector<folia::Word*>& ){
  throw logic_error( "NER tagger call undefined postprocess() member" );
}

string to_tag( const string& label, bool inside ){
  vector<string> parts = TiCC::split_at( label, "+" );
  if ( parts.size() > 1 ){
    // undecided
    return "O";
  }
  else {
    string result;
    if ( inside ){
      result += "I-";
    }
    else {
      result += "B-";
    }
    result += parts[0];
    return result;
  }
}

void NERTagger::merge_override( vector<tc_pair>& tags,
				const vector<tc_pair>& overrides,
				bool unconditional,
				const vector<string>& POS_tags ) const{
  string label;
  for ( size_t i=0; i < tags.size(); ++i ){
    if ( overrides[i].first != "O"
	 && ( POS_tags.empty()
	      || POS_tags[i].find("SPEC(") != string::npos
	      || POS_tags[i].find("N(") != string::npos ) ){
      // if ( i == 0 ){
      //  	 cerr << "override = " << overrides << endl;
      //  	 cerr << "ner tags = " << tags << endl;
      //  	 cerr << "POS tags = " << POS_tags << endl;
      // }
      if ( tags[i].first[0] != 'O'
	   && !unconditional ){
	// don't tamper with existing tags
	continue;
      }
      bool inside = (label == overrides[i].first );
      //      cerr << "step i=" << i << " override=" << overrides[i].first << endl;
      string replace = to_tag(overrides[i].first, inside );
      //      cerr << "replace=" << replace << endl;
      if ( replace != "O" ){
	// there is something to override.
	// NB: replace wil return "O" for ambi tags too!
	if ( tags[i].first[0] == 'I' && !inside ){
	  //	  cerr << "before  whiping tags = " << tags << endl;
	  //oops, inside a NER tag, and now a new override
	  // whipe previous I tags, and one B
	  if ( i == 0 ){
	    // strange exception, in fact starting with an I tag is impossible
	    tags[i].first[0] = 'B'; // fix it on the fly
	    continue; // next i
	  }
	  for ( size_t j = i-1; j > 0; --j ){
	    if ( tags[j].first[0] == 'B' ){
	      // we are done
	      tags[j].first = "O";
	      break;
	    }
	    tags[j].first = "O";
	  }
	  // whipe next I tags too, to be sure
	  for ( size_t j = i+1; j < tags.size(); ++j ){
	    if ( tags[j].first[0] != 'I' ){
	      // a B or O
	      break;
	    }
	    // still inside
	    tags[j].first = "O";
	  }
	}
	//	cerr << POS_tags[i] << "REPLACE " << tags[i] << " by " << replace << endl;
	tags[i].first = replace;
	tags[i].second = 1;
	if ( !inside ){
	  label = overrides[i].first;
	}
      }
    }
    else {
      label = "";
    }
  }
}

bool NERTagger::Generate( const std::string& opt_line ){
  return tagger->GenerateTagger( opt_line );
}
