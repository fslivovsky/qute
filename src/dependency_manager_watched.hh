#ifndef dependency_manager_watched_hh
#define dependency_manager_watched_hh

#include "dependency_manager.hh"
#include <vector>
#include <queue>
#include <unordered_set>
#include <string>
#include <cstring>

using std::vector;
using std::unordered_set;
using std::string;

namespace Qute {

class QCDCL_solver;

class DependencyManagerWatched: public DependencyManager {

friend class DecisionHeuristicVMTFdeplearn;
friend class DecisionHeuristicVSIDSdeplearn;
friend class DecisionHeuristicSGDB;

public:
  DependencyManagerWatched(QCDCL_solver& solver, string dependency_learning_strategy);
  virtual void addVariable(bool auxiliary);
  virtual void addDependency(Variable of, Variable on);
  virtual void notifyStart();
  virtual void notifyAssigned(Variable v);
  virtual void notifyUnassigned(Variable v);
  virtual bool isDecisionCandidate(Variable v) const;
  virtual bool dependsOn(Variable of, Variable on) const;
  virtual void learnDependencies(Variable unit_variable, vector<Literal>& literal_vector);

  // these methods only do something in DependencyManagerRRS
  virtual void reduceWithRRS(std::vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type) {};
  virtual void filterIndependentVariables(Variable unit_variable, vector<Literal>& literal_vector) {};

protected:
  void (DependencyManagerWatched::*learnDependenciesPtr)(Variable unit_variable, vector<Literal>& literal_vector);
  void learnAllDependencies(Variable unit_variable, vector<Literal>& literal_vector);
  void learnOutermostDependency(Variable unit_variable, vector<Literal>& literal_vector);
  void learnDependencyWithFewestDependencies(Variable unit_variable, vector<Literal>& literal_vector);
  Variable watcher(Variable v) const;
  bool findWatchedDependency(Variable v, bool remove_from_old);
  void setWatchedDependency(Variable variable, Variable new_watched, bool remove_from_old);

  struct DependencyData
  {
    Variable watcher;
    uint32_t watcher_index:31;
    bool independencies_known:1;
    unordered_set<Variable> dependent_on;
    vector<Variable> dependent_on_vector;
    vector<Variable> independent_of;
    DependencyData(): watcher(0), independencies_known(false) {};
  };

  vector<DependencyData> variable_dependencies;
  vector<vector<Variable>> variables_watched_by;

  QCDCL_solver& solver;
  bool prefix_mode;
  vector<bool> is_auxiliary;

};

// Implementation of inline methods.

inline void DependencyManagerWatched::addVariable(bool auxiliary) {
  variables_watched_by.emplace_back();
  variable_dependencies.emplace_back();
  is_auxiliary.push_back(auxiliary);
}

inline void DependencyManagerWatched::notifyStart() {}

inline void DependencyManagerWatched::notifyUnassigned(Variable v) {}

inline bool DependencyManagerWatched::dependsOn(Variable of, Variable on) const {
  if (prefix_mode) {
    return on < of;
  } else {
    return variable_dependencies[of - 1].dependent_on.find(on) != variable_dependencies[of - 1].dependent_on.end();
  }
}

inline void DependencyManagerWatched::learnDependencies(Variable unit_variable, vector<Literal>& literal_vector) {
  (*this.*learnDependenciesPtr)(unit_variable, literal_vector);
}

inline Variable DependencyManagerWatched::watcher(Variable v) const {
  return variable_dependencies[v - 1].watcher;
}

}

#endif
