#ifndef CKYPARSER_H
#define CKYPARSER_H

#include <set>

enum dirType { ROOT, LEFT, RIGHT, ERROR };

class Constraint {
 public:
  enum ConstraintType { Incoming, Dependency, Direction };
 Constraint( double w, int i ): weight(w),tokenIndex(i){
  };
  virtual ~Constraint(){};
  virtual void put( std::ostream& os ) const {
    os << tokenIndex << " " << weight;
  };
  virtual ConstraintType type() const = 0;
  virtual dirType direct() const { return ERROR; };
  virtual std::string rel() const { return "NOREL"; };
  virtual int hIndex() const { abort(); };
  int tIndex() const { return tokenIndex; };
  double wght() const { return weight; };
 protected:
  double weight;
  int    tokenIndex;
};

std::ostream& operator<<( std::ostream& os, const Constraint& c );
std::ostream& operator<<( std::ostream& os, const Constraint* c );

class HasIncomingRel: public Constraint {
 public:
 HasIncomingRel( int i, const std::string& r, double w ):
  Constraint( w, i ), relType(r){
  };
  void put( std::ostream& ) const;
  ConstraintType type() const { return Incoming; }
  std::string rel() const { return relType; };
 private:
  std::string relType;
};

class HasDependency: public Constraint {
 public:
 HasDependency( int i, int h, const std::string& r, double w ):
  Constraint( w, i ), relType(r), headType(h) {
    };
  void put( std::ostream& ) const;
  ConstraintType type() const { return Dependency; };
  int hIndex() const { return headType; };
  std::string rel() const { return relType; };
 private:
  std::string relType;
  int headType;
};

class DependencyDirection: public Constraint {
 public:
 DependencyDirection( int i, const std::string& d, double w ):
  Constraint( w, i ), direction(toEnum(d)){
  }
  void put( std::ostream& ) const;
  ConstraintType type() const { return Direction; };
  dirType direct() const { return direction; };
 private:
  dirType toEnum( const std::string& s ){
    if ( s == "ROOT" )
      return ROOT;
    else if ( s == "LEFT" )
      return LEFT;
    else if ( s == "RIGHT" )
      return RIGHT;
    else {
      abort();
    }
  }
  dirType direction;
};

class SubTree {
 public:
 SubTree( double score, int r, const std::string& label ):
  _score( score ), _r( r ), _edgeLabel( label ){
  }
 SubTree( ):
  _score( 0.0 ), _r( -1 ), _edgeLabel( "" ){
  }
  std::set<const Constraint*>  satisfiedConstraints;
  double score() const { return _score; };
  int r() const { return _r; };
  std::string edgeLabel() const { return _edgeLabel; };
 private:
  double _score;
  int _r;
  std::string _edgeLabel;
};

struct parsrel {
  std::string deprel;
  int head;
};

class chart_rec {
 public:
  SubTree l_True;
  SubTree l_False;
  SubTree r_True;
  SubTree r_False;
};

class CKYParser {
public:
  CKYParser( size_t, const std::vector<const Constraint*>&, TiCC::LogStream* );
  ~CKYParser(){ delete ckyLog; };
  void parse();
  void leftIncomplete( int , int , std::vector<parsrel>& );
  void rightIncomplete( int , int , std::vector<parsrel>& );
  void leftComplete( int , int , std::vector<parsrel>& );
  void rightComplete( int , int , std::vector<parsrel>& );

private:
  void addConstraint( const Constraint * );
  std::string bestEdge( const SubTree& , const SubTree& , size_t , size_t,
			std::set<const Constraint*>&, double& );
  size_t numTokens;
  std::vector< std::vector<const Constraint*>> inDepConstraints;
  std::vector< std::vector<const Constraint*>> outDepConstraints;
  std::vector< std::vector< std::vector<const Constraint*>>> edgeConstraints;
  std::vector< std::vector<chart_rec>> chart;

  TiCC::LogStream *ckyLog;
};

#endif
