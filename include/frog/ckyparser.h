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
  _score( score ), _r( r ), edgeLabel( label ){
  }
 SubTree( ):
  _score( 0.0 ), _r( -1 ), edgeLabel( "" ){
  }
  std::set<Constraint*>  satisfiedConstraints;
  double score() const { return _score; };
 private:
  double _score;
  int _r;
  std::string edgeLabel;
};

class CKYParser {
public:
  CKYParser( size_t );
  void addConstraint( Constraint * );
  void parse();
private:
  std::string bestEdge( SubTree& , SubTree& , size_t , size_t,
			std::set<Constraint*>&, double& );
  size_t numTokens;
  std::vector< std::vector<Constraint*>> inDepConstraints;
  std::vector< std::vector<Constraint*>> outDepConstraints;
  std::vector< std::vector< std::vector<Constraint*>>> edgeConstraints;
};

#endif
