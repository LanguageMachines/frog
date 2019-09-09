#ifndef CSIDP_H
#define CSIDP_H
#include "timbl/TimblAPI.h"
#include "frog/ckyparser.h"

class timbl_result {
 public:
  timbl_result( const std::string& cls,
		double conf,
		const Timbl::ValueDistribution* );
  timbl_result( const std::string& cls,
		double conf,
		const std::vector<std::pair<std::string,double>>& );
  std::string cls() const { return _cls; };
  double confidence() const { return _confidence; };
  std::vector< std::pair<std::string,double> > dist() const { return _dist; };
private:
  std::string _cls;
  double _confidence;
  std::vector< std::pair<std::string,double> > _dist;
};


std::vector<parsrel> parse( const std::vector<timbl_result>&,
			    const std::vector<timbl_result>&,
			    const std::vector<timbl_result>&,
			    size_t,
			    int,
			    TiCC::LogStream* );

#endif
