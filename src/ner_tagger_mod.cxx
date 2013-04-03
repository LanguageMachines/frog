/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2013
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
#include "frog/ner_tagger_mod.h"

using namespace std;
using namespace folia;

NERTagger::NERTagger(){
  tagger = 0;
  nerLog = new LogStream( theErrLog, "ner-" );
}

NERTagger::~NERTagger(){
  delete tagger;
  delete nerLog;
}
 
bool NERTagger::init( const Configuration& conf ){
  debug = tpDebug;
  string db = conf.lookUp( "debug", "NER" );
  if ( !db.empty() )
    debug = TiCC::stringTo<int>( db );
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
  if (debug) 
    *Log(nerLog) << "NER Tagger Init" << endl;
  if ( tagger != 0 ){
    *Log(nerLog) << "NER Tagger is already initialized!" << endl;
    return false;
  }  
  string val = conf.lookUp( "settings", "NER" );
  if ( val.empty() ){
    *Log(nerLog) << "Unable to find settings for NER" << endl;
    return false;
  }
  string settings;
  if ( val[0] == '/' ) // an absolute path
    settings = val;
  else
    settings =  configuration.configDir() + val;

  val = conf.lookUp( "version", "NER" );
  if ( val.empty() ){
    version = "1.0";
  }
  else
    version = val;
  val = conf.lookUp( "set", "NER" );
  if ( val.empty() ){
    tagset = "http://ilk.uvt.nl/folia/sets/frog-ner-nl";
  }
  else
    tagset = val;

  string init = "-s " + settings + " -vcf";
  tagger = new MbtAPI( init, *nerLog );
  return tagger->isInit();
}

static void addEntity( EntitiesLayer *entities, 
		       const string& tagset,
		       const vector<Word*>& words,
		       const vector<double>& confs,
		       const string& NER ){
  double c = 1;
  for ( size_t i=0; i < confs.size(); ++i )
    c *= confs[i];
  KWargs args;
  args["class"] = NER;
  args["confidence"] =  toString(c);
  args["set"] = tagset;
  Entity *e = 0;
#pragma omp critical(foliaupdate)
  {
    e = new Entity(entities->doc(), args);
    entities->append( e );
  }
  for ( size_t i=0; i < words.size(); ++i ){
#pragma omp critical(foliaupdate)
    {
      e->append( words[i] );
    }
  }
}

void NERTagger::addNERTags( const vector<Word*>& words,
			    const vector<string>& tags,
			    const vector<double>& confs ){
  if ( words.empty() )
    return;
  EntitiesLayer *el = 0;
#pragma omp critical(foliaupdate)
  {
    Sentence *sent = words[0]->sentence();
    try {
      el = sent->annotation<EntitiesLayer>();
    }
    catch(...){
      el = new EntitiesLayer();
      sent->append( el );
    }
  }
  vector<Word*> stack;
  vector<double> dstack;
  string curNER;
  for ( size_t i=0; i < tags.size(); ++i ){
    if (debug) 
      *Log(nerLog) << "NER = " << tags[i] << endl;
    vector<string> ner;
    if ( tags[i] == "O" ){
      if ( !stack.empty() ){
	if (debug) {
	  *Log(nerLog) << "O spit out " << curNER << endl;
	  using TiCC::operator<<;
	  *Log(nerLog) << "spit out " << stack << endl;	
	}
	addEntity( el, tagset, stack, dstack, curNER );
	dstack.clear();
	stack.clear();
      }
      continue;
    }
    else {
      size_t num_words = TiCC::split_at( tags[i], ner, "-" );
      if ( num_words != 2 ){
	*Log(nerLog) << "expected <NER>-tag, got: " << tags[i] << endl;
	exit( EXIT_FAILURE );
      }
    }
    if ( ner[0] == "B" ||
	 ( ner[0] == "I" && stack.empty() ) ||
	 ( ner[0] == "I" && ner[1] != curNER ) ){
      // an I without preceding B is handled as a B
      // an I with a different TAG is also handled as a B
      if ( !stack.empty() ){
	if ( debug ){
	  *Log(nerLog) << "B spit out " << curNER << endl;
	  using TiCC::operator<<;
	  *Log(nerLog) << "spit out " << stack << endl;	
	}
	addEntity( el, tagset, stack, dstack, curNER );
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
      *Log(nerLog) << "END spit out " << curNER << endl;
      using TiCC::operator<<;
      *Log(nerLog) << "spit out " << stack << endl;	
    }
    addEntity( el, tagset, stack, dstack, curNER );
  }
}

void NERTagger::addDeclaration( Document& doc ) const {
  doc.declare( AnnotationType::ENTITY, 
	       tagset,
	       "annotator='frog-ner-" + version
	       + "', annotatortype='auto', datetime='" + getTime() + "'");
}

void NERTagger::Classify( const vector<Word *>& swords ){
  if ( !swords.empty() ) {
    vector<string> words;
    string sentence; // the tagger needs the whole sentence
    for ( size_t w = 0; w < swords.size(); ++w ) {
      sentence += swords[w]->str();
      words.push_back( swords[w]->str() );
      if ( w < swords.size()-1 )
	sentence += " ";
    }
    if (debug) 
      *Log(nerLog) << "NER in: " << sentence << endl;
    vector<TagResult> tagv = tagger->TagLine(sentence);
    if ( tagv.size() != swords.size() ){
      throw runtime_error( "NER tagger is confused" );
    }
    if ( debug ){
      *Log(nerLog) << "NER tagger out: " << endl;
      for ( size_t i=0; i < tagv.size(); ++i ){
	*Log(nerLog) << "[" << i << "] : word=" << tagv[i].word() 
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
    addNERTags( swords, tags, conf );
  }
}


