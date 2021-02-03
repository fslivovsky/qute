#ifndef dependency_manager_rrs_hh
#define dependency_manager_rrs_hh

#include "dependency_manager_watched.hh"
#include <vector>
#include <queue>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <iostream>

using std::vector;
using std::unordered_set;
using std::string;

namespace Qute {

class QCDCL_solver;
enum ConstraintType: unsigned short;

class DependencyManagerRRS: public DependencyManagerWatched {

friend class DecisionHeuristicVMTFdeplearn;
friend class DecisionHeuristicVSIDSdeplearn;
friend class DecisionHeuristicSGDB;

public:
  DependencyManagerRRS(QCDCL_solver& solver, string dependency_learning_strategy);
  virtual void reduceWithRRS(std::vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type);
  virtual void filterIndependentVariables(Variable unit_variable, vector<Literal>& literal_vector);

protected:
  void getDepsRRS(Variable v);
  vector<bool> getReachable(Literal l);
  bool notDependsOn(Variable of, Variable on) const;
  bool independenciesKnown(Variable of) const;
  bool numIndependencies(Variable of) const;
  bool leftmostIndependent(Variable of) const;
  void addNonDependency(Variable of, Variable on);

};

// Implementation of inline methods.

inline bool DependencyManagerRRS::independenciesKnown(Variable of) const {
  return variable_dependencies[of - 1].independencies_known;
}

inline bool DependencyManagerRRS::numIndependencies(Variable of) const {
  return variable_dependencies[of - 1].independent_of.size();
}

inline bool DependencyManagerRRS::leftmostIndependent(Variable of) const {
  return variable_dependencies[of - 1].independent_of[0];
}

inline bool DependencyManagerRRS::notDependsOn(Variable of, Variable on) const {
	return std::binary_search(variable_dependencies[of - 1].independent_of.begin(),
							  variable_dependencies[of - 1].independent_of.end(),
							  on);
}

}

#endif
