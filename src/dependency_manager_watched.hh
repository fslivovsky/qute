#ifndef dependency_manager_watched_hh
#define dependency_manager_watched_hh

#include <vector>
#include <unordered_set>
#include "dependency_manager.hh"
#include "qcdcl.hh"

using std::vector;
using std::unordered_set;

namespace Qute {

class DependencyManagerWatched: public DependencyManager {

friend class DecisionHeuristicVMTFdeplearn;
friend class DecisionHeuristicVSIDSdeplearn;

public:
  DependencyManagerWatched(QCDCL_solver& solver, bool prefix_mode);
  virtual void addVariable(bool auxiliary);
  virtual void addDependency(Variable of, Variable on);
  virtual void notifyStart();
  virtual void notifyAssigned(Variable v);
  virtual void notifyUnassigned(Variable v);
  virtual bool isDecisionCandidate(Variable v) const;
  virtual bool dependsOn(Variable of, Variable on) const;

protected:
  Variable watcher(Variable v) const;
  bool findWatchedDependency(Variable v, bool remove_from_old);
  void setWatchedDependency(Variable variable, Variable new_watched, bool remove_from_old);

  struct DependencyData
  {
    Variable watcher;
    uint32_t watcher_index;
    unordered_set<Variable> dependent_on;
    vector<Variable> dependent_on_vector;
    DependencyData(): watcher(0) {};
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

inline Variable DependencyManagerWatched::watcher(Variable v) const {
  return variable_dependencies[v - 1].watcher;
}

}

#endif