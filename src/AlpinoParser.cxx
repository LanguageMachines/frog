/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2019
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
#include "frog/Frog-util.h"
#include "frog/FrogData.h"

using namespace std;

using TiCC::operator<<;

//#define DEBUG_ALPINO

xmlDoc *alpino_server_parse( frog_data& fd ){
  string host = "localhost"; // config.lookUp( "host", "alpino" );
  string port = "8004"; // config.lookUp( "port", "alpino" );
  Sockets::ClientSocket client;
  if ( !client.connect( host, port ) ){
    cerr << "failed to open Alpino connection: "<< host << ":" << port << endl;
    cerr << "Reason: " << client.getMessage() << endl;
    exit( EXIT_FAILURE );
  }
  //  cerr << "start input loop" << endl;
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
  return doc;
}

ostream& operator<<( ostream& os, const dp_tree *node ){
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
  const dp_tree *pnt = store;
  while ( pnt ){
    cerr << std::string(indent, ' ') << pnt << endl;
    print_nodes( indent+3, pnt->link );
    pnt = pnt->next;
  }
}

const dp_tree *extract_hd( const dp_tree *node ){
  const dp_tree *pnt = node->link;
  while ( pnt ){
    if ( pnt->rel == "hd" ){
      return pnt;
    }
    else if ( pnt->rel == "crd" ){
      return pnt;
    }
    pnt = pnt->next;
  }
  return 0;
}

int mwu_size( const dp_tree *node ){
  int result = 0;
  dp_tree *pnt = node->link;
  while ( pnt ){
    if ( pnt->rel == "mwp" ){
      ++result;
    }
    pnt = pnt->next;
  }
  return result;
}

void extract_dp( const dp_tree *store, const dp_tree *root,
		 const string& h,
		 vector<pair<string,int>>& result ){
  const dp_tree *pnt = store;
  //  cerr << "LOOP over: " << pnt << endl;
  while ( pnt ){
    //    cerr << "\tLOOP: " << pnt->rel << endl;
    if ( pnt->begin+1 == pnt->end ){
      if ( pnt == root ){
	if ( root->rel == "--"  ){
	  //	  cerr << "THAT A ";
	  //	  cerr << "0\tROOT" << endl;
	  result.push_back( make_pair("ROOT",0) );
	}
	else if ( h == "--" ){
	  //	  cerr << "THAT B ";
	  //	  cerr << "0\tROOT" << endl;
	  result.push_back( make_pair("ROOT",0) );
	}
	else {
	  //	  cerr << "THAT C ";
	  //	  cerr << store->begin << "\t" << h << endl;
	  result.push_back( make_pair(h,store->begin ) );
	}
      }
      else if ( root && root->end > 0 ){
	//	cerr << "THOSE A ";
	result.push_back( make_pair(pnt->rel,root->end ) );
	//	cerr << root->end << "\t" << pnt->rel << endl;
      }
      else if ( pnt->rel == "--" ){
	//	cerr << "THOSE B ";
	result.push_back( make_pair("punct",pnt->begin ) );
	//	cerr << pnt->begin << "\tpunct" << endl;
      }
      else {
	//	cerr << "THOSE C ";
	result.push_back( make_pair(pnt->rel, pnt->begin ) );
	//	cerr << pnt->begin << "\t" << pnt->rel << endl;
      }
    }
    else {
      if ( pnt->link ){
	// if ( root ){
	//   cerr << "DADA " << root->end << "\t" << pnt->rel << endl;
	// }
	const dp_tree *new_root = extract_hd( pnt );
	// if ( new_root ){
	//   cerr << pnt->rel << ", root at this level: " << new_root << endl;
	// }
	extract_dp( pnt->link, new_root, pnt->rel, result );
      }
    }
    pnt = pnt->next;
  }
}

dp_tree *parse_node( xmlNode *node, int& index ){
  auto atts = TiCC::getAttributes( node );
  //  cerr << "attributes: " << atts << endl;
  dp_tree *dp = new dp_tree();
  dp->id = TiCC::stringTo<int>( atts["id"] );
  dp->begin = TiCC::stringTo<int>( atts["begin"] );
  dp->end = TiCC::stringTo<int>( atts["end"] );
  dp->rel = atts["rel"];
  dp->word = atts["word"];
  if ( !dp->word.empty() ){
    dp->word_index = ++index;
  }
  dp->link = 0;
  dp->next = 0;
  return dp;
}

dp_tree *parse_nodes( xmlNode *node, int& index ){
  dp_tree *result = 0;
  xmlNode *pnt = node;
  dp_tree *last = 0;
  while ( pnt ){
    if ( TiCC::Name( pnt ) == "node" ){
      dp_tree *parsed = parse_node( pnt, index );
      // cerr << "parsed ";
      // print_node( parsed );
      if ( result == 0 ){
	result = parsed;
	last = result;
      }
      else {
	last->next = parsed;
	last = last->next;
      }
      if ( pnt->children ){
	dp_tree *childs = parse_nodes( pnt->children, index );
	last->link = childs;
      }
    }
    pnt = pnt->next;
  }
  return result;
}

dp_tree *resolve_mwus( dp_tree *in,
		       int& new_index,
		       int& compensate ){
  dp_tree *result = in;
  dp_tree *pnt = in;
  while ( pnt ){
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
      delete tmp;
      ++new_index;
      pnt->end = pnt->begin+1;
      compensate = count;
    }
    else {
      pnt->begin -= compensate;
      pnt->end -= compensate;
      if ( !pnt->word.empty() ){
	pnt->word_index = ++new_index;
      }
    }
    pnt->link = resolve_mwus( pnt->link, new_index, compensate );
    pnt = pnt->next;
  }
  return result;
}

vector<pair<string,int>> extract_dp( xmlDoc *alp_doc ){
  string txtfile = "/tmp/debug.xml";
  xmlSaveFormatFileEnc( txtfile.c_str(), alp_doc, "UTF8", 1 );
  xmlNode *top_node = TiCC::xPath( alp_doc, "//node[@rel='top']" );
  int index = 0;
  dp_tree *dp = parse_nodes( top_node, index );
  //  cerr << endl << "done parsing, dp nodes:" << endl;
  //  print_nodes( 0, dp );
  index = 0;
  int compensate = 0;
  dp = resolve_mwus( dp, index, compensate );
  cerr << endl << "done resolving, dp nodes:" << endl;
  print_nodes( 0, dp );
  vector<pair<string,int>> result;
  if ( dp->rel == "top" ){
    extract_dp( dp->link, 0, "BUG", result );
  }
  else {
    cerr << "PANIEK!, geen top node" << endl;
  }
  return result;
}
