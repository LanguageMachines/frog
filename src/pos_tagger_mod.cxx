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
#include "ucto/unicode.h"
#include "frog/Frog.h"
#include "frog/ucto_tokenizer_mod.h"
#include "frog/pos_tagger_mod.h"

using namespace std;
using namespace TiCC;
using namespace Tagger;


#define LOG *Log(tag_log)

POSTagger::POSTagger( TiCC::LogStream *logstream, const string& label ){
  debug = 0;
  tagger = 0;
  filter = 0;
  tag_log = new LogStream( logstream, label+"-tagger-" );
}

POSTagger::~POSTagger(){
  delete tagger;
  delete filter;
  delete tag_log;
}

bool fill_set( const string& file, set<string>& st ){
  ifstream is( file );
  if ( !is ){
    return false;
  }
  string line;
  while( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' )
      continue;
    line = TiCC::trim( line );
    st.insert( line );
  }
  return true;
}

bool POSTagger::fill_map( const string& file, map<string,string>& mp ){
  ifstream is( file );
  if ( !is ){
    return false;
  }
  string line;
  while( getline( is, line ) ){
    if ( line.empty() || line[0] == '#' )
      continue;
    vector<string> parts;
    size_t num = TiCC::split_at( line, parts, "\t" );
    if ( num != 2 ){
      LOG << "invalid entry in '" << file << "'" << endl;
      LOG << "expected 2 tab-separated values, but got: '"
	  << line << "'" << endl;
      return false;
    }
    mp[ parts[0] ] = parts[1];
  }
  return true;
}

bool POSTagger::init( const Configuration& config, const string& label ){
  debug = 0;
  if ( tagger != 0 ){
    LOG << "POS-Tagger is already initialized!" << endl;
    return false;
  }
  string val = config.lookUp( "debug", label );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debug = TiCC::stringTo<int>( val );
  }
  switch ( debug ){
  case 0:
  case 1:
    break;
  case 2:
  case 3:
  case 4:
    tag_log->setlevel(LogDebug);
    break;
  case 5:
  case 6:
  case 7:
    tag_log->setlevel(LogHeavy);
    break;
  default:
    tag_log->setlevel(LogExtreme);
  }
  val = config.lookUp( "settings", label );
  if ( val.empty() ){
    LOG << "Unable to find settings for: " << label << endl;
    return false;
  }
  string settings;
  if ( val[0] == '/' ) // an absolute path
    settings = val;
  else
    settings =  config.configDir() + val;

  val = config.lookUp( "version", label );
  if ( val.empty() ){
    version = "1.0";
  }
  else
    version = val;
  val = config.lookUp( "set", label );
  if ( val.empty() ){
    LOG << "missing set declaration in config" << endl;
    return false;
  }
  else {
    tagset = val;
  }
  string charFile = config.lookUp( "char_filter_file", label );
  if ( charFile.empty() )
    charFile = config.lookUp( "char_filter_file" );
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new Tokenizer::UnicodeFilter();
    filter->fill( charFile );
  }
  string tokFile = config.lookUp( "token_trans_file", label );
  if ( tokFile.empty() )
    tokFile = config.lookUp( "token_trans_file" );
  if ( !tokFile.empty() ){
    tokFile = prefix( config.configDir(), tokFile );
    if ( !fill_map( tokFile, token_tag_map ) ){
      LOG << "failed to load a token translation file from: '"
		   << tokFile << "'"<< endl;
      return false;
    }
  }
  string tagsFile = config.lookUp( "tags_file", label );
  if ( tagsFile.empty() )
    tagsFile = config.lookUp( "tags_file" );
  if ( !tagsFile.empty() ){
    tagsFile = prefix( config.configDir(), tagsFile );
    if ( !fill_set( tagsFile, valid_tags ) ){
      LOG << "failed to load a tags file from: '"
		   << tagsFile << "'"<< endl;
      return false;
    }
  }
  string cls = config.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }
  if ( debug ){
    LOG << "POS taggger textclass= " << textclass << endl;
  }
  string init = "-s " + settings + " -vcf";
  tagger = new MbtAPI( init, *tag_log );
  return tagger->isInit();
}

void POSTagger::addTag( folia::Word *word,
			const string& inputTag,
			double confidence,
			bool /*known NOT USED yet*/ ){
  string pos_tag = inputTag;
  string ucto_class = word->cls();
  if ( debug ){
    LOG << "lookup ucto class= " << ucto_class << endl;
  }
  auto const tt = token_tag_map.find( ucto_class );
  if ( tt != token_tag_map.end() ){
    if ( debug ){
      LOG << "found translation ucto class= " << ucto_class
	  << " to POS-Tag=" << tt->second << endl;
    }
    pos_tag = tt->second;
    confidence = 1.0;
  }
  folia::KWargs args;
  args["set"]  = tagset;
  args["class"]  = pos_tag;
  args["confidence"]= toString(confidence);
  if ( textclass != "current" ){
    args["textclass"] = textclass;
  }
#pragma omp critical (foliaupdate)
  {
    word->addPosAnnotation( args );
  }
  //  folia::FoliaElement *pos = 0;
  //#pragma omp critical (foliaupdate)
  //  {
  //    pos = word->addPosAnnotation( args );
  //  }
  // if ( !known ){
  //   args.clear();
  //   args["class"] = "yes";
  //   args["subset"] = "unknown_word";
  //   folia::Feature *feat = new folia::Feature( args );
  //   pos->append( feat );
  // }
}

void POSTagger::addDeclaration( folia::Document& doc ) const {
#pragma omp critical (foliaupdate)
  {
    doc.declare( folia::AnnotationType::POS,
		 tagset,
		 "annotator='frog-mbpos-" + version
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
  }
}

vector<TagResult> POSTagger::tagLine( const string& line ){
  if ( tagger )
    return tagger->TagLine(line);
  else
    throw runtime_error( "POSTagger is not initialized" );
}

string POSTagger::set_eos_mark( const std::string& eos ){
  if ( tagger )
    return tagger->set_eos_mark( eos );
  else
    throw runtime_error( "POSTagger is not initialized" );
}

string POSTagger::extract_sentence( const vector<folia::Word*>& swords,
				    vector<string>& words ){
  words.clear();
  string sentence;
  for ( const auto& sword : swords ){
    UnicodeString word;
#pragma omp critical (foliaupdate)
    {
      word = sword->text( textclass );
    }
    if ( filter )
      word = filter->filter( word );
    string word_s = folia::UnicodeToUTF8( word );
    vector<string> parts;
    TiCC::split( word_s, parts );
    word_s.clear();
    for ( const auto& p : parts ){
      word_s += p;
    }
    words.push_back( word_s );
    sentence += word_s;
    if ( &sword != &swords.back() ){
      sentence += " ";
    }
  }
  return sentence;
}

void POSTagger::Classify( const vector<folia::Word*>& swords ){
  if ( !swords.empty() ) {
    vector<string> words; // not used here
    string sentence = extract_sentence( swords, words );
    if (debug){
      LOG << "POS tagger in: " << sentence << endl;
    }
    vector<TagResult> tagv = tagger->TagLine(sentence);
    if ( tagv.size() != swords.size() ){
      LOG << "mismatch between number of <w> tags and the tagger result." << endl;
      LOG << "words according to <w> tags: " << endl;
      for ( size_t w = 0; w < swords.size(); ++w ) {
	LOG << "w[" << w << "]= " << swords[w]->str( textclass ) << endl;
      }
      LOG << "words according to POS tagger: " << endl;
      for ( size_t i=0; i < tagv.size(); ++i ){
	LOG << "word[" << i << "]=" << tagv[i].word() << endl;
      }
      throw runtime_error( "POS tagger is confused" );
    }
    if ( debug ){
      LOG << "POS tagger out: " << endl;
      for ( size_t i=0; i < tagv.size(); ++i ){
	LOG << "[" << i << "] : word=" << tagv[i].word()
	    << " tag=" << tagv[i].assignedTag()
	    << " confidence=" << tagv[i].confidence() << endl;
      }
    }
    for ( size_t i=0; i < tagv.size(); ++i ){
      addTag( swords[i],
	      tagv[i].assignedTag(),
	      tagv[i].confidence(),
	      tagv[i].isKnown() );
    }
  }
}
