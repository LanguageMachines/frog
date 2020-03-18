/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2020
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

#include "frog/AlpinoParser.h"

#include <string>
#include <iostream>

#include <ticcutils/XMLtools.h>

#include "ticcutils/StringOps.h"
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/SocketBasics.h"
#include "ticcutils/FileUtils.h"
#include "frog/Frog-util.h"
#include "frog/FrogData.h"

using namespace std;

using TiCC::operator<<;

#define LOG *TiCC::Log(errLog)
#define DBG *TiCC::Dbg(dbgLog)

//#define DEBUG_ALPINO
//#define DEBUG_MWU
//#define DEBUG_EXTRACT

const string alpino_tagset = "http://ilk.uvt.nl/folia/sets/alpino-parse-nl";
const string alpino_mwu_tagset = "http://ilk.uvt.nl/folia/sets/alpino-mwu-nl";

void AlpinoParser::add_provenance( folia::Document& doc,
				   folia::processor *main ) const {
  /// add provenance data for the AlpinoParser to the Frog provenance
  /*!
    \param doc the current folia::Document
    \param main a pointer to the provenance processor of Frog
  */
  string _label = "alpino-parser";
  if ( !main ){
    throw logic_error( "AlpinoParser::add_provenance() without arguments." );
  }
  folia::KWargs args;
  args["name"] = _label;
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  folia::processor *proc = doc.add_processor( args, main );
  args.clear();
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::DEPENDENCY, alpino_tagset, args );
  args["name"] = "alpino-mwu";
  args["generate_id"] = "auto()";
  args["version"] = _version;
  args["begindatetime"] = "now()";
  proc = doc.add_processor( args, main );
  args.clear();
  args["processor"] = proc->id();
  doc.declare( folia::AnnotationType::ENTITY, alpino_mwu_tagset, args );
}


bool AlpinoParser::init( const TiCC::Configuration& configuration ){
  /// initaliaze an AlpinoParser class from a configuration
  /*!
    \param configuration the configuration to use
  */
  filter = 0;
  bool problem = false;
  LOG << "initiating alpino parser ... " << endl;
  string val = configuration.lookUp( "debug", "parser" );
  if ( !val.empty() ){
    int level;
    if ( TiCC::stringTo<int>( val, level ) ){
      if ( level > 5 ){
	dbgLog->setlevel( LogLevel::LogDebug );
      }
    }
  }
  val = configuration.lookUp( "version", "parser" );
  if ( val.empty() ){
    _version = "1.0";
  }
  else {
    _version = val;
  }
  val = configuration.lookUp( "set", "parser" );
  if ( val.empty()
       || val == "http://ilk.uvt.nl/folia/sets/frog-depparse-nl" ){
    // remove all remains of the frog dependency parser
    dep_tagset = alpino_tagset;
  }
  else {
    dep_tagset = val;
  }
  val = configuration.lookUp( "set", "tagger" );
  if ( val.empty() ){
    POS_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
  }
  else {
    POS_tagset = val;
  }
  string charFile = configuration.lookUp( "char_filter_file", "tagger" );
  if ( charFile.empty() ){
    charFile = configuration.lookUp( "char_filter_file" );
  }
  if ( !charFile.empty() ){
    charFile = prefix( configuration.configDir(), charFile );
    filter = new TiCC::UniFilter();
    filter->fill( charFile );
  }

  val = configuration.lookUp( "alpino_host", "parser" );
  if ( !val.empty() ){
    // so using an external Alpino server
    _host = val;
    val = configuration.lookUp( "alpino_port", "parser" );
    if ( !val.empty() ){
      _port = val;
    }
    else {
      LOG << "missing port option" << endl;
      problem = true;
    }
  }
  val = configuration.lookUp( "alpinoserver", "parser" );
  _alpino_server = ( val == "true" );
  if ( _alpino_server ){
    if ( _host.empty() ){
      LOG << "missing 'alpino_host' value in configuration" << endl;
      problem = true;
    }
    else {
      string mess = check_server( _host, _port, "Alpino" );
      if ( !mess.empty() ){
	LOG << "FAILED to find a server for the Alpino parser:" << endl;
	LOG << mess << endl;
	LOG << "Alpino server not running??" << endl;
	problem = true;
      }
      else {
	LOG << "using Alpino Parser on " << _host << ":" << _port << endl;
      }
    }
  }
  else {
    string cmd = "which Alpino > /dev/null 2>&1";
    int res = system( cmd.c_str() );
    if ( res ){
      string outline = "Cannot find Alpino executable!\n"
	"possible solution:\n"
	"export ALPINO_HOME=$HOME/Alpino\n"
	"export PATH=$PATH:$ALPINO_HOME/bin\n";
      LOG << outline << endl;
      problem = true;
    }
    else {
      LOG << "using locally installed Alpino." << endl;
    }
  }
  if ( problem ) {
    return false;
  }

  string cls = configuration.lookUp( "outputclass" );
  if ( !cls.empty() ){
    textclass = cls;
  }
  else {
    textclass = "current";
  }
  isInit = true;
  return true;
}

void AlpinoParser::Parse( frog_data& fd, TimerBlock& timers ){
  /// parse a frog_data structure using Alpino
  /*!
    \param fd the frog_data structure
    \param timers used for storing timing information

    this function will use an Alpino Server or a locally installed Alpino
    executable, depending on the current options and settings.

    The results are stored in the \e fd parameter
  */
  timers.parseTimer.start();
  if ( !isInit ){
    LOG << "Parser is not initialized! EXIT!" << endl;
    throw runtime_error( "Parser is not initialized!" );
  }
  if ( fd.empty() ){
    LOG << "unable to parse an analysis without words" << endl;
    return;
  }

#ifdef DEBUG_ALPINO
  cerr << "Testing Alpino parsing" << endl;
  cerr << "voor server_parse:" << endl << fd << endl;
#endif
  vector<parsrel> solution;
  if ( _alpino_server ){
    solution = alpino_server_parse( fd );
    if ( solution.empty() ){
      LOG << "parsing failed" << endl;
      throw runtime_error( "Is the Alpino Parser running?" );
    }
  }
  else {
    solution = alpino_parse( fd );
    if ( solution.empty() ){
      LOG << "parsing failed" << endl;
      throw runtime_error( "Is the Alpino runtime installed?" );
    }
  }
#ifdef DEBUG_ALPINO
  cerr << "NA parse:" << endl << fd << endl;
  int count = 0;
  for( const auto& sol: solution ){
    if ( count == 0 ){
      ++count;
      continue;
    }
    if ( sol.head == 0 && sol.deprel.empty() ){
      ++count;
      continue;
    }
    cerr << count << "\t" << fd.mw_units[count-1].word << "\t" << sol.head << "\t" << sol.deprel << endl;
    ++count;
  }
  cerr << endl;
#endif
  appendParseResult( fd, solution );
#ifdef DEBUG_ALPINO
  cerr << "NA appending:" << endl << fd << endl;
#endif
  timers.parseTimer.stop();
}

void AlpinoParser::add_mwus( const frog_data& fd,
			     const vector<folia::Word*>& wv ) const {
  /// add the MWU's stored in a frog_data record as folia Entities
  /*!
    \param fd the frog_data structure to use
    \param wv the list of folia::Word nodes that was parsed into the \e fd
    frog_data structure

    So we create an entities layer on the Sentence above the Word vector
    and add all words as MWU entities in that layer
  */
  folia::Sentence *s = wv[0]->sentence();
  folia::KWargs args;
  if ( !s->id().empty() ){
    args["generate_id"] = s->id();
  }
  args["set"] = alpino_mwu_tagset;
  folia::EntitiesLayer *el;
#pragma omp critical (foliaupdate)
  {
    el = new folia::EntitiesLayer( args, s->doc() );
    s->append( el );
  }
  for ( const auto& mwu : fd.mwus ){
    if ( !el->id().empty() ){
      args["generate_id"] = el->id();
    }
    if ( textclass != "current" ){
      args["textclass"] = textclass;
    }
#pragma omp critical (foliaupdate)
    {
      folia::Entity *e = new folia::Entity( args, s->doc() );
      el->append( e );
      for ( size_t pos = mwu.first; pos <= mwu.second; ++pos ){
	e->append( wv[pos] );
      }
    }
  }
}

void AlpinoParser::add_result( const frog_data& fd,
			       const vector<folia::Word*>& wv ) const {
  /// add the parser information in \e fd as Dependencies and Entities to
  /// the FoLiA
  /*!
    \param fd the frog_data structure with all information
    \param wv the list of folia::Word nodes that was parsed into the \e fd
    frog_data structure
   */
  ParserBase::add_result( fd, wv );
  add_mwus( fd, wv );
}

ostream& operator<<( ostream& os, const dp_tree *node ){
  /// print out one dp_tree structure node
  /*!
    \param os the output stream
    \param node the node to print
   */
  if ( node ){
    os << node->rel << "[" << node->begin << "," << node->end << "] ("
       << node->word << ")";
    if ( node->word_index > 0 ){
      os << "#" << node->word_index;
    }
  }
  else {
    os << "null";
  }
  return os;
}

void print_nodes( int indent, const dp_tree *store ){
  /// recursively pretty print out a dp_tree tree to stderr
  /*!
    \param indent indentation level
    \param store the top node to print
   */
  const dp_tree *pnt = store;
  while ( pnt ){
    cerr << std::string(indent, ' ') << pnt << endl;
    print_nodes( indent+3, pnt->link );
    pnt = pnt->next;
  }
}

const dp_tree *extract_hd( const dp_tree *node ){
  /// search a dp_tree for a head node
  /*!
    \param node the top node to start searching
    \return the dp_tree structure holding a head

    a head is defined as a node with a \e rel value of 'hd', 'crd' ot 'cmp'
   */
  const dp_tree *pnt = node->link;
  while ( pnt ){
    if ( pnt->rel == "hd" ){
      return pnt;
    }
    else if ( pnt->rel == "crd" ){
      return pnt;
    }
    else if ( pnt->rel == "cmp" ){
      return pnt;
    }
    pnt = pnt->next;
  }
  return 0;
}

dp_tree *parse_node( xmlNode *node ){
  /// convert a node in an Alpino XML tree into a much simpler dp_tree node
  /*!
    \param node The Alpino XML node to parse
  */
  auto atts = TiCC::getAttributes( node );
  //  cerr << "attributes: " << atts << endl;
  dp_tree *dp = new dp_tree();
  dp->id = TiCC::stringTo<int>( atts["id"] );
  dp->begin = TiCC::stringTo<int>( atts["begin"] );
  dp->end = TiCC::stringTo<int>( atts["end"] );
  dp->rel = atts["rel"];
  dp->word = atts["word"];
  if ( !dp->word.empty() ){
    dp->word_index = dp->end;
  }
  dp->link = 0;
  dp->next = 0;
  //  cerr << "created dp_tree: " << dp << endl;
  return dp;
}

dp_tree *parse_nodes( xmlNode *node ){
  /// recurively convert an Alpino XML tree into a much simpler dp_tree tree
  /*!
    \param node The Alpino XML node to parse
  */
  dp_tree *result = 0;
  xmlNode *pnt = node;
  dp_tree *last = 0;
  while ( pnt ){
    if ( TiCC::Name( pnt ) == "node" ){
      dp_tree *parsed = parse_node( pnt );
      // cerr << "parsed ";
      // print_node( parsed );
      if ( parsed->begin+1 < parsed->end
	   && pnt->children == NULL ){
	// an aggregate with NO children.
	// just leave it out
	delete parsed;
      }
      else {
	if ( result == 0 ){
	  result = parsed;
	  last = result;
	}
	else {
	  last->next = parsed;
	  last = last->next;
	}
      }
      if ( pnt->children ){
	dp_tree *childs = parse_nodes( pnt->children );
	last->link = childs;
      }
    }
    pnt = pnt->next;
  }
  return result;
}

dp_tree *resolve_mwus( dp_tree *in,
		       int& compensate,
		       int& restart,
		       frog_data& fd ){
  /// extract MWU information into frog_data.
  /*!
    \param in the dp_tree to search through
    \param compensate complicated
    \param restart complicated
    \param fd the frog_data structure to register the MWU in.
    \return the modified dp_tree

    this function searches the dp_tree for MWUS (rel='mwp').
    It will merge the found MWU words in a modified dp_tree AND register
    the start and finish indexes of the mwu in \e fd

    The indices of the MWU's in dp_tree are not always consecutively, so
    we have to check that using \e restart.

    Also, after modifying the tree, the indices are wrong by the size of the
    MWU. so we need to compensate for that too.
   */
  dp_tree *result = in;
  dp_tree *pnt = in;
#ifdef DEBUG_MWU
  cerr << "RESOLVE MWUS" << endl;
  cerr << "compensate = " << compensate << " restart=" << restart << endl;
#endif
  while ( pnt ){
#ifdef DEBUG_MWU
    cerr << "bekijk " << pnt << " compensate=" << compensate << endl;

    cerr << "begin=" << pnt->begin << " restart=" << restart << endl;
#endif
    if ( pnt->link && pnt->link->rel == "mwp" ){
      dp_tree *tmp = pnt->link;
      pnt->word = tmp->word;
      pnt->word_index = tmp->word_index;
      int count = 0;
      tmp = tmp->next;
      while ( tmp ){
	++count;
	pnt->word += "_" + tmp->word;
	tmp = tmp->next;
      }
      tmp = pnt->link;
      pnt->link = 0;
      pnt->end = pnt->begin+1;
      compensate = count;
      restart = pnt->end;
      fd.mwus[tmp->word_index-1] = tmp->word_index + count-1;
      delete tmp;
    }
    else if ( pnt->begin < restart ){
      // ignore?
    }
    else {
      pnt->begin -= compensate;
      pnt->end -= compensate;
      if ( !pnt->word.empty() ){
	pnt->word_index -= compensate;
      }
    }
    pnt->link = resolve_mwus( pnt->link, compensate, restart, fd );
    pnt = pnt->next;
  }
  return result;
}

map<int,dp_tree*> serialize_top( dp_tree *in ){
  /// No clue atm
  map<int,dp_tree*> result;
  dp_tree *pnt = in->link;
  // only loop over nodes direcly under the top node!
  while ( pnt ){
    result[pnt->begin] = pnt;
    pnt = pnt->next;
  }
  auto it = result.begin();
  while ( it != result.end() ) {
    auto nit = next(it);
    dp_tree *next = 0;
    if ( nit != result.end() ){
      next = nit->second;
    }
    //    cerr << "hang " << next << " achter " << it->second << endl;
    it->second->next = next;
    ++it;
  }
  in->link = result.begin()->second;
  return result;
}

dp_tree *resolve_mwus( dp_tree *in, frog_data& fd ){
  /// moielijk moeilijk
  map<int,dp_tree*> top_nodes = serialize_top( in );
  //  cerr << "after serialize: ";
  //  print_nodes(4, in );
  //  cerr << endl;
  int compensate = 0;
  int restart = 0;
  for ( const auto& it : top_nodes ){
#ifdef DEBUG_MWU
    cerr << "voor resolve MWU's " << it.first << endl;
#endif
    resolve_mwus( it.second, compensate, restart, fd );
    break;
  }
  return in;
}

void extract_dependencies( list<pair<const dp_tree*,const dp_tree*>>& result,
			   const dp_tree *store,
			   const dp_tree *root ){
  /// recursively extract all head-dependent pairs form store
  const dp_tree *pnt = store->link;
  if ( !pnt ){
    return;
  }
  const dp_tree *my_root = extract_hd( store );
#ifdef DEBUG_EXTRACT
  cerr << "LOOP over: " << store << "head= " << my_root << endl;
#endif
  while ( pnt ){
    if ( pnt == my_root ){
      result.push_back( make_pair(pnt,root) );
    }
    else {
      result.push_back( make_pair(pnt, my_root) );
    }
    extract_dependencies( result, pnt, pnt );
    pnt = pnt->next;
  }
}

vector<parsrel> extract( list<pair<const dp_tree*,const dp_tree*>>& l ){
  /// convert a list of head-dependent pairs into a list of parsrel records
  /*!
    \param l a list of head-dependent pairs
    \return a list of parsrel records
   */
  vector<parsrel> result(l.size());
  for ( const auto& it : l ){
#ifdef DEBUG_EXTRACT
    cerr << "bekijk: " << it << endl;
#endif
    int pos = 0;
    int dep = 0;
    string rel;
    if ( it.second == 0 ){
      // root node
      if ( it.first->word.empty() ){
	if ( it.first->link
	     && it.first->rel != "--" ){
#ifdef DEBUG_EXTRACT
	  cerr << "AHA   some special thing: " << it.first->rel << endl;
	  cerr << "          IN            : " << it << endl;
#endif
	  // not a word but an aggregate
	  const dp_tree *my_head = extract_hd( it.first );
	  if ( my_head ){
#ifdef DEBUG_EXTRACT
	    cerr << "TEMP ROOT=" << my_head << endl;
#endif
	    pos = my_head->word_index;
	    rel = "ROOT";
	    dep = 0;
	  }
	  else {
	    continue;
	  }
	}
	else {
	  continue;
	  // ignore this
	}
      }
      else {
	pos = it.first->word_index;
	rel = "punct";
	dep = it.first->begin;
#ifdef DEBUG_EXTRACT
	cerr << "A match[" << pos << "] " << rel << " " << dep << endl;
#endif
      }
    }
    else if ( it.first->word.empty() ){
      // not a word but an aggregate
      const dp_tree *my_head = extract_hd( it.first );
      //      cerr << "TEMP ROOT=" << my_head << endl;
      if ( !my_head ){
	//	cerr << "PANIC" << it << endl;
	pos = -1;
      }
      else {
	pos = my_head->word_index;
	rel = it.first->rel;
	dep = it.second->word_index;
#ifdef DEBUG_EXTRACT
	cerr << "B match[" << pos << "] " << rel << " " << dep << endl;
#endif
      }
    }
    else if ( it.first->rel == "hd"
	      || it.first->rel == "crd"
	      || it.first->rel == "cmp" ){
      if ( it.second->rel == "--" ){
	pos = it.first->word_index;
	rel = "ROOT";
	dep = 0;
#ifdef DEBUG_EXTRACT
	cerr << "C match[" << pos << "] " << rel << " " << dep << endl;
#endif
      }
      else {
	pos = it.first->word_index;
	rel = it.second->rel;
	dep = it.second->end;
#ifdef DEBUG_EXTRACT
	cerr << "D match[" << pos << "] " << rel << " " << dep << endl;
#endif
      }
    }
    else {
      pos = it.first->word_index;
      rel = it.first->rel;
      dep = it.second->word_index;
#ifdef DEBUG_EXTRACT
      cerr << "E match[" << pos << "] " << rel << " " << dep << endl;
#endif
    }
    if ( pos >= 0 && result[pos].deprel.empty() ){
#ifdef DEBUG_EXTRACT
      cerr << "store[" << pos << "] " << rel << " " << dep << endl;
#endif
      parsrel tmp;
      tmp.head = dep;
      tmp.deprel = rel;
      result[pos] = tmp;
    }
  }
  return result;
}

vector<parsrel> extract_dp( xmlDoc *alp_doc,
			    frog_data& fd ){
  /// extract a list of parsrel records from an Alpino XML file and resolve
  /// MWU's
  /*!
    \param alp_doc an Alpino XML document
    \param fd a frog_data structure to receive the MWU information
    \return a list of parsrel records
   */
  // string txtfile = "/tmp/debug.xml";
  // xmlSaveFormatFileEnc( txtfile.c_str(), alp_doc, "UTF8", 1 );
  xmlNode *top_node = TiCC::xPath( alp_doc, "//node[@rel='top']" );
  dp_tree *dp = parse_nodes( top_node );
#if defined(DEBUG_EXTRACT) || defined(DEBUG_MWU)
  cerr << endl << "done parsing, dp nodes:" << endl;
  print_nodes( 0, dp );
#endif
  dp = resolve_mwus( dp, fd );
#if defined(DEBUG_EXTRACT) || defined(DEBUG_MWU)
  cerr << endl << "done resolving, dp nodes:" << endl;
  print_nodes( 0, dp );
#endif
  list<pair<const dp_tree*,const dp_tree*>> result;
  if ( dp->rel == "top" ){
    extract_dependencies( result, dp, 0 );
    // cerr << "found dependencies: "<< endl;
    // for ( const auto& it : result ){
    //   cerr << it.first << " " << it.second << endl;
    // }
  }
  else {
    throw runtime_error( "PANIEK!, geen top node" );
  }
  fd.resolve_mwus();
#ifdef DEBUG_EXTRACT
  cerr << "FD: Na resolve " << endl;
  cerr << fd << endl;
#endif
  vector<parsrel> pr = extract(result);
  delete dp;
  return pr;
}

vector<parsrel> AlpinoParser::alpino_server_parse( frog_data& fd ){
  /// parse a sentence into a group of parsrel records using an Alpino server
  /*!
    \param fd The frog_data record containing the information to parse
    \return a vector of parsrel structures

    This function connects to an Alpino server (which should be configured
    correctly), handles it the sentence contained in \e fd and parses the
    delivered XML to extract all dependency information
  */
  Sockets::ClientSocket client;
  if ( !client.connect( _host, _port ) ){
    cerr << "failed to open Alpino connection: "<< _host << ":" << _port << endl;
    cerr << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
#ifdef DEBUG_ALPINO
  cerr << "start Alpino input loop" << endl;
#endif
  string txt = fd.sentence();
  client.write( txt + "\n\n" );
  string result;
  string s;
  while ( client.read(s) ){
    result += s + "\n";
  }
#ifdef DEBUG_ALPINO
  cerr << "received data [" << result << "]" << endl;
#endif
  xmlDoc *doc = xmlReadMemory( result.c_str(), result.length(),
			       0, 0, XML_PARSE_NOBLANKS );
  return extract_dp(doc,fd);
}

vector<parsrel> AlpinoParser::alpino_parse( frog_data& fd ){
  /// parse a sentence into a group of parsrel records using a local ALpino
  /*!
    \param fd The frog_data record containing the information to parse
    \return a vector of parsrel structures

    This function uses a local Alpino porgram (which should be configured
    correctly) by feeding it the sentence contained in \e fd and parses the
    delivered XML to extract all dependency information
  */
#ifdef DEBUG_ALPINO
  cerr << "calling Alpino input:" << fd.sentence() << endl;
#endif
  vector<parsrel> result;
  string input_file = TiCC::tempname("alpino-parse.txt.");
  string tmp_dir = TiCC::dirname(input_file)+"/";
  string tmp_str = input_file.substr( input_file.size()-6 );
  string txt = tmp_str + "|" + fd.sentence();
  //    Alpino will use tmp_str for the output filename.
  ofstream os( input_file );
  os << txt;
  os.close();
  string parseCmd = "Alpino -veryfast -flag treebank " + tmp_dir +
    " end_hook=xml -parse <  " + input_file + " -notk > /dev/null 2>&1";
#ifdef DEBUG_ALPINO
  cerr << "run: " << parseCmd << endl;
#endif
  int res = system( parseCmd.c_str() );
  if ( res ){
    cerr << "Alpino failed: RES = " << res << " : " << strerror(res) << endl;
    return result;
  }
#ifndef DEBUG_ALPINO
  TiCC::erase( input_file );
#endif
  string xml_file = tmp_dir + tmp_str + ".xml";
  xmlDoc *xmldoc = xmlReadFile( xml_file.c_str(), 0, XML_PARSE_NOBLANKS );
  result = extract_dp(xmldoc,fd);
  xmlFreeDoc( xmldoc );
#ifndef DEBUG_ALPINO
  TiCC::erase( xml_file );
#endif
  return result;
}
