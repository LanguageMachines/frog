/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2023
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
#include "frog/ner_tagger_mod.h"

#include "mbt/MbtAPI.h"
#include "frog/Frog-util.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"

using namespace std;
using namespace icu;
using namespace Tagger;
using TiCC::operator<<;

#define LOG *TiCC::Log(err_log)
#define DBG *TiCC::Log(dbg_log)

static string POS_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";

NERTagger::NERTagger( TiCC::LogStream *l, TiCC::LogStream *d ):
  BaseTagger( l, d, "NER" ),
  gazets_only(false),
  max_ner_size(20)
{
  /// initialize a NERTagger instance
  /*!
    \param l a LogStream for errors
    \param d a LogStream for debugging
  */
  gazet_ners.resize( max_ner_size + 1 );
  override_ners.resize( max_ner_size + 1 );
}

bool NERTagger::init( const TiCC::Configuration& config ){
  /// initalize a NER tagger from 'config'
  /*!
    \param config the TiCC::Configuration
    \return true on succes, false otherwise

    first BaseTagger::init() is called to set generic values,
    then the NER specific values for the gazeteer file-names etc. are
    added and the files are read.
  */
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
			   vector<map<UnicodeString,set<string>>>& ners ){
  /// fill known Named Entities from one gazeteer file
  /*!
    \param cat The NE categorie (like 'loc' or 'org')
    \param name the filename to read
    \param config_dir the directory to search for files
    \param ners the structure to store the NE's in
  */
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
  UnicodeString line;
  while ( TiCC::getline( is, line ) ){
    if ( line.isEmpty() || line[0] == '#' ){
      continue;
    }
    else {
      vector<UnicodeString> parts = TiCC::split( line );
      if ( parts.size() > (unsigned)max_ner_size ){
	if ( ++long_err_cnt > 50 ){
	  LOG << "too many long entries in additional wordlist file. " << file_name << endl;
	  LOG << "consider raising the max_ner_size in the configuration. (now "
	      << max_ner_size << ")" << endl;
	  return false;
	}
	else {
	  if ( debug > 1 ){
	    DBG << "expected 1 to " << max_ner_size
		<< " SPACE-separated parts in line: '" << line
		<< "'" << endl;
	  }
	  continue;
	}
      }
      // reconstruct the NER with single spaces
      UnicodeString clean_line = "";
      for ( const auto& part : parts ){
	clean_line += part;
	if ( &part != &parts.back() ){
	  clean_line += " ";
	}
      }
      ners[parts.size()][clean_line].insert( cat );
      ++ner_cnt;
    }
  }
  DBG << "loaded " << ner_cnt << " additional " << cat
      << " Named Entities from: " << file_name << endl;
  return true;
}

bool NERTagger::read_gazets( const string& name,
			     const string& config_dir,
			     vector<map<UnicodeString,set<string>>>& ners ){
  /// fill known Named Entities from a list of gazeteer files
  /*!
    \param name the filename to read the gazeteer info from
    \param config_dir the directory to search for files
    \param ners the structure to store the NE's in

    NE's are stored in a vector of maps, ranked on their length. So NE's of
    length 1 ("London") are stored at position 1 and so on. ("dag van de arbeid"
    at position 4).

    the NE is stored in a map using as the mono-spaced string as key and adding
    the cetagory in a set. (as categories can be ambiguous)
  */
  string file_name = name;
  string lookup_dir = config_dir;
  if ( name[0] != '/' ) {
    file_name = prefix( config_dir, file_name );
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
    vector<string> parts = TiCC::split_at( line, "\t" );
    if ( parts.size() != 2 ){
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

static vector<UnicodeString> serialize( const vector<set<string>>& stags ){
  /// for every non empty set {el1,el2,..} in stags we compose a string like:
  /// el1+el2+...
  vector<UnicodeString> ambitags( stags.size(), "O" );
  size_t pos = 0;
  for ( const auto& it : stags ){
    if ( !it.empty() ){
      UnicodeString res;
      for ( const auto& s : it ){
	res += TiCC::UnicodeFromUTF8(s) + "+";
      }
      ambitags[pos] = res;
    }
    ++pos;
  }
  return ambitags;
}

vector<UnicodeString> NERTagger::create_ner_list( const vector<UnicodeString>& words,
						  vector<map<UnicodeString,set<string>>>& ners ){
  /// create a list of ambitags given a range of words
  /*!
    \param words a sentence as a list of words
    \param ners the NE structure to examine
   */
  vector<set<string>> stags( words.size() );
  if ( debug > 1 ){
    DBG << "search for known NER's" << endl;
  }
  for ( size_t j=0; j < words.size(); ++j ){
    // cycle through the words
    UnicodeString seq;
    size_t len = 1;
    for ( size_t i = 0; i < min( words.size() - j, (size_t)max_ner_size); ++i ){
      // start looking for sequences of length len
      auto const mp = ners[len++];
      if ( mp.empty() ){
	continue;
      }
      seq += words[j+i];
      if ( debug > 1 ){
	DBG << "sequence = '" << seq << "'" << endl;
      }
      auto const& tags = mp.find(seq);
      if ( tags != mp.end() ){
	if ( debug > 1 ){
	  DBG << "FOUND tags " << tags->first << "-" << tags->second << endl;
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

void NERTagger::add_declaration( folia::Document& doc,
				 folia::processor *proc ) const {
  /// add ENTITY annotation as an AnnotationType to the document
  /*!
    \param doc the Document the add to
    \param proc the processor to add
  */
  folia::KWargs args;
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::ENTITY, tagset, args );
}

void NERTagger::Classify( frog_data& swords ){
  /// Tag one sentence, given in frog_data format
  /*!
    \param swords the frog_data structure to analyze

    When tagging succeeds, 'swords' will be extended with the tag results
   */
  if ( debug ){
    DBG << "classify from DATA" << endl;
  }
  vector<UnicodeString> words;
  vector<UnicodeString> pos_tags;
#pragma omp critical (dataupdate)
  {
    for ( const auto& w : swords.units ){
      UnicodeString word = w.word;
      word = filter_spaces( word );
      words.push_back( word );
      pos_tags.push_back( w.tag );
    }
  }
  vector<tc_pair> ner_tags;
  if ( gazets_only ){
    vector<UnicodeString> gazet_tags = create_ner_list( words, gazet_ners );
    UnicodeString last = "O";
    cerr << "bekijk gazet tags: " << gazet_tags << endl;
    for ( const auto& it : gazet_tags ){
      vector<UnicodeString> parts = TiCC::split_at( it, "+" );
      UnicodeString tag = parts[0];
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
    vector<UnicodeString> gazet_tags = create_ner_list( words, gazet_ners );
    vector<UnicodeString> override_v = create_ner_list( words, override_ners );
    vector<tc_pair> override_tags;
    std::transform( override_v.cbegin(), override_v.cend(),
		    std::back_inserter(override_tags),
		    []( auto us ){ return make_pair(us,1.0); } );
    UnicodeString prev = "_";
    UnicodeString prevN = "_";
    vector<tag_entry> to_do;
    for ( size_t i=0; i < swords.size(); ++i ){
      tag_entry entry;
      entry.word = words[i];
      entry.enrichment = prev + "\t" + pos_tags[i];
      prev = pos_tags[i];
      if ( i < swords.size() - 1 ){
	entry.enrichment += "\t" + pos_tags[i+1];
      }
      else {
	entry.enrichment += "\t_";
      }
      entry.enrichment += "\t" + prevN + "\t" + gazet_tags[i];
      prevN = gazet_tags[i];
      if ( i < swords.size() - 1 ){
	entry.enrichment += "\t" + gazet_tags[i+1];
      }
      else {
	entry.enrichment += "\t_";
      }
      to_do.push_back( entry );
    }
    _tag_result = tag_entries( to_do );
    if ( debug > 1 ){
      DBG << "NER tagger out: " << endl;
      for ( size_t i=0; i < _tag_result.size(); ++i ){
	DBG << "[" << i << "] : word=" << _tag_result[i].word()
	    << " tag=" << _tag_result[i].assigned_tag()
	    << " confidence=" << _tag_result[i].confidence() << endl;
      }
    }
    //
    // we have to correct for tags that start with 'I-'
    // (the MBT tagger may deliver those)
    UnicodeString last;
    for ( const auto& tag : _tag_result ){
      UnicodeString assigned = tag.assigned_tag();
      if ( assigned == "O" ){
	last = "";
      }
      else {
	vector<UnicodeString> parts = TiCC::split_at( assigned, "-" );
	vector<UnicodeString> vals = TiCC::split_at( parts[1], "+" );
	UnicodeString val = vals[0];
	if ( val == last ){
	  assigned = "I-" + val;
	}
	else {
	  if ( debug > 1 ){
	    DBG << "replace " << assigned << " by " << "B-" << val << endl;
	  }
	  last = val;
	  assigned = "B-" + val;
	}
      }
      ner_tags.push_back( make_pair( assigned, tag.confidence() ) );
    }
    if ( !override_tags.empty() ){
      vector<UnicodeString> empty;
      merge_override( ner_tags, override_tags, true, empty );
    }
  }
  post_process( swords, ner_tags );
}

void NERTagger::post_process( frog_data& sentence,
			      const vector<tc_pair>& ners ){
  /// finish the NER processs by updating 'sentence'
  /*!
    \param sentence a frog_data structure to update with NER info
    \param ners a sequence of NE tags (maybe 'O') with their confidence

    This is a specialized version local to NERTagger. Don't use the normal
    post_process()
  */
  if ( sentence.size() == 0 ) {
    return;
  }
  vector<tc_pair> entity;
  UnicodeString curNER; // only used in debug messages
  for ( size_t i=0; i < ners.size(); ++i ){
    if (debug > 1){
      DBG << "NER = " << ners[i].first << endl;
    }
    if ( ners[i].first == "O" ){
      if ( !entity.empty() ){
	// end the previous entity
	if (debug > 1) {
	  DBG << "O spit out " << curNER << endl;
	  DBG << "ners  " << entity << endl;
	}
	addEntity( sentence, i, entity );
	entity.clear();
      }
      continue;
    }
    else if ( ners[i].first[0] == 'B' ){
      if ( !entity.empty() ){
	if ( debug > 1 ){
	  DBG << "B spit out " << curNER << endl;
	  DBG << "spit out " << entity << endl;
	  curNER = UnicodeString( ners[i].first, 2 );
	}
	addEntity( sentence, i, entity );
	entity.clear();
      }
    }
    entity.push_back( make_pair( ners[i].first, ners[i].second ) );
  }
  if ( !entity.empty() ){
    if ( debug > 1 ){
      DBG << "END spit out " << curNER << endl;
      DBG << "spit out " << entity << endl;
    }
    addEntity( sentence, sentence.size(), entity );
  }
}

void NERTagger::addEntity( frog_data& sent,
			   const size_t pos,
			   const vector<tc_pair>& entity ){
  /// add a NER entity to the frog_data structure
  /*!
    \param sent The frog_data to extend with NER info
    \param pos the curent position
    \param entity a Ner entity which may span several tc_pairs

    A Named Entity can span several TAGS with each a confidence.
    The confidence of the NE is calculated as the mean of the confidences
    of those tags.

    The NE tags and the mean confidence are assigned to the proper locations
    in the 'sent' structure.
  */
  double c = std::accumulate( entity.cbegin(), entity.cend(),
			      0.0,
			      []( const double& res, const tc_pair& p ){ return std::move(res) + p.second; } );
  c /= entity.size();
  for ( size_t i = 0; i < entity.size(); ++i ){
#pragma omp critical (foliaupdate)
    {
      sent.units[pos-entity.size()+i].ner_tag = entity[i].first;
      sent.units[pos-entity.size()+i].ner_confidence = c;
    }
  }
}


void NERTagger::post_process( frog_data& ){
  /// This implements BaseTagger::post_process by DISALLOWING it.
  throw logic_error( "NER tagger call undefined postprocess() member" );
}

UnicodeString to_tag( const UnicodeString& label, bool inside ){
  /// convert a label into a result tag
  /*!
    \param label the label as a string
    \param inside are we 'inside' the tag or at the begin?
    \return the result tag

    There are several cases:

    * The label is ambiguous, like "loc+org". then we return "O"
    * The label is usable like 'org'. Then we return 'I-org' when 'inside' or
    'B-org' othwerwise
  */
  vector<UnicodeString> parts = TiCC::split_at( label, "+" );
  if ( parts.size() > 1 ){
    // undecided
    return "O";
  }
  else {
    UnicodeString result;
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
				const vector<icu::UnicodeString>& POS_tags ) const {
  /// merge overrides into the tc_pair vector
  /*!
    \param tags the list of tc_pair to merge into
    \param overrides the override information (from gazeteers and such)
    \param unconditional when true, prefer the information from overrides,
    otherwise only override when there was no tag assigned yet
    \param POS_tags a list of POS-tags. When this list is non-empty, only
    override when it contains a N or a SPEC tag.
   */
  UnicodeString label;
  for ( size_t i=0; i < tags.size(); ++i ){
    if ( overrides[i].first != "O"
	 && ( POS_tags.empty()
	      || POS_tags[i].indexOf("SPEC(") == 0
	      || POS_tags[i].indexOf("N(") == 0 ) ){
      // if ( i == 0 ){
      //  	 cerr << "override = " << override << endl;
      //  	 cerr << "ner tags = " << tags << endl;
      //  	 cerr << "POS tags = " << POS_tags << endl;
      // }
      if ( tags[i].first[0] != 'O'
	   && !unconditional ){
	// don't tamper with existing tags
	continue;
      }
      bool inside = (label == overrides[i].first );
      //      cerr << "step i=" << i << " override=" << overrides[i] << endl;
      UnicodeString replace = to_tag(overrides[i].first, inside );
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
	    tags[i].first.replace(0,1,'B'); // fix it on the fly
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


bool NERTagger::Generate( const string& opt_line ){
  /// generate a new tagger using opt_line
  return tagger->GenerateTagger( opt_line );
}

void NERTagger::add_result( const frog_data& fd,
			    const vector<folia::Word*>& wv ) const {
  /// add the NER tags in 'fd' to the FoLiA list of Word
  /*!
    \param fd The tagged results
    \param wv The folia:Word vector
   */
  folia::Sentence *s = wv[0]->sentence();
  folia::EntitiesLayer *el = 0;
  folia::Entity *ner = 0;
  size_t i = 0;
  for ( const auto& word : fd.units ){
    if ( word.ner_tag[0] == 'B' ){
      if ( el == 0 ){
	// create a layer, we need it
	folia::KWargs args;
	args["set"] = getTagset();
	if ( !s->id().empty() ){
	  args["generate_id"] = s->id();
	}
#pragma omp critical (foliaupdate )
	{
	  el = s->add_child<folia::EntitiesLayer>( args );
	}
      }
      // a new entity starts here, so append the prevous one to the layer
      if ( ner != 0 ){
#pragma omp critical (foliaupdate )
	{
	  el->append( ner );
	}
      }
      // now make new entity
      folia::KWargs args;
      args["set"] = getTagset();
      args["generate_id"] = el->id();
      args["class"] = TiCC::UnicodeToUTF8(word.ner_tag).substr(2); // strip the B-
      args["confidence"] = TiCC::toString(word.ner_confidence);
      if ( textclass != "current" ){
	args["textclass"] = textclass;
      }
#pragma omp critical (foliaupdate )
      {
	ner = new folia::Entity( args, s->doc() );
	ner->append( wv[i] );
      }
    }
    else if ( word.ner_tag[0] == 'I' ){
      // continue in an entity
      if ( ner ){
#pragma omp critical (foliaupdate )
	{
	  ner->append( wv[i] );
	}
      }
      else {
	throw logic_error( "unexpected empty ner" );
      }
    }
    else if ( word.ner_tag[0] == '0' ){
      if ( ner != 0 ){
#pragma omp critical (foliaupdate )
	{
	  el->append( ner );
	}
	ner = 0;
      }
    }
    ++i;
  }
  if ( ner != 0 ){
    // some leftovers
#pragma omp critical (foliaupdate )
    {
      el->append( ner );
    }
  }
}
