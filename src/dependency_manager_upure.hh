#ifndef dependency_manager_upure_hh
#define dependency_manager_upure_hh

#include "dependency_manager_watched.hh"
#include <vector>
#include <unordered_set>
#include <string>
#include <algorithm>

using std::vector;
using std::unordered_set;
using std::string;

namespace Qute {

class QCDCL_solver;

class DependencyManagerUPure: public DependencyManagerWatched {

friend class DecisionHeuristicVMTFdeplearn;
friend class DecisionHeuristicVSIDSdeplearn;
friend class DecisionHeuristicSGDB;

public:
  DependencyManagerUPure(QCDCL_solver& solver, string dependency_learning_strategy, string out_of_order_decisions);
  virtual void reduceWithDepscheme(std::vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type);
  virtual void filterIndependentVariables(Variable unit_variable, vector<Literal>& literal_vector);

protected:
  void getDepsUPure(Variable v);
  vector<bool> getReachable(Literal l);
  bool notDependsOn(Variable of, Variable on) const;
  //bool checkDependency(Variable of, Literal lof);
  bool independenciesKnown(Variable of) const;
  bool numIndependencies(Variable of) const;
  bool leftmostIndependent(Variable of) const;
  void addNonDependency(Variable of, Variable on);

};

// Implementation of inline methods.

inline bool DependencyManagerUPure::independenciesKnown(Variable on) const {
  return variable_dependencies[on - 1].independencies_known;
}

inline bool DependencyManagerUPure::numIndependencies(Variable on) const {
  return variable_dependencies[on - 1].independent_of.size();
}

inline bool DependencyManagerUPure::leftmostIndependent(Variable on) const {
  return variable_dependencies[on - 1].independent_of[0];
}

inline bool DependencyManagerUPure::notDependsOn(Variable of, Variable on) const {
	return std::binary_search(variable_dependencies[on - 1].independent_of.begin(),
							  variable_dependencies[on - 1].independent_of.end(),
							  of);
}

}

#endif
