/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2024
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
#ifndef CSIDP_H
#define CSIDP_H
#include <cstddef>
#include <vector>
#include <string>
#include <utility>

namespace TiCC { class LogStream; }
namespace Timbl { class ClassDistribution; }
struct parsrel;

/// this class stores a triple of Timbl results
/*!
  1. the assigned class
  2. the Confidence value for the class
  3. the class distribution as a vector of class/double values
 */
class timbl_result {
 public:
  timbl_result( const std::string&,
		double,
		const Timbl::ClassDistribution& );
  timbl_result( const std::string&,
		double,
		const std::vector<std::pair<std::string,double>>& );
  std::string cls() const {
    /// return the cls value
    return _cls;
  };
  double confidence() const {
    /// return the confidence of  the value
    return _confidence;
  };
  const std::vector< std::pair<std::string,double>>& dist() const {
    /// return the distribution where the cls value is part of
    return _dist;
  };
private:
  std::string _cls;
  double _confidence;
  std::vector< std::pair<std::string,double>> _dist;
};


std::vector<parsrel> parse( const std::vector<timbl_result>& p_res,
			    const std::vector<timbl_result>& r_res,
			    const std::vector<timbl_result>& d_res,
			    size_t sent_len,
			    int maxDist,
			    TiCC::LogStream *dbg_log );

#endif
