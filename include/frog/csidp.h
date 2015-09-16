#ifndef CSIDP_H
#define CSIDP_H
#include "frog/ckyparser.h"

void parse( const std::string& dep_file, const std::string& mod_file,
	    const std::string& dir_file, int maxDist,
	    const std::string& out_file, const std::string& in_file );

#endif
