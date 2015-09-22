#ifndef CSIDP_H
#define CSIDP_H
#include "frog/ckyparser.h"

std::vector<parsrel> parse( const std::string& dep_file, const std::string& mod_file,
		       const std::string& dir_file, int maxDist,
		       const std::string& in_file );

#endif
