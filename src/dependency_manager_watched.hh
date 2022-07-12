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

#define ELIGIBLE UINT32_MAX
#define PERMANENTLY_INELIGIBLE UINT32_MAX-1

namespace Qute {

class QCDCL_solver;

class DependencyManagerWatched: public DependencyManager {

friend class DecisionHeuristicVMTFdeplearn;
friend class DecisionHeuristicVSIDSdeplearn;
friend class DecisionHeuristicSGDB;

public:
  DependencyManagerWatched(QCDCL_solver& solver, string dependency_learning_strategy, string out_of_order_decisions);
  virtual void addVariable(bool auxiliary, bool qtype);
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

  // methods around out-of-order decisions
  bool out_of_order_decisions[2];
  void setVariableAEL(Variable v, uint32_t level);
  void markVarPermanentlyUnassignable(Variable v);
  void markVarUnassignable(Variable v);
  void backtrackAET(uint32_t backtrack_level);
  inline uint32_t varAEL(Variable v);
  // the main interface method; says whether a variable is eligible for an out-of-order decision
  bool isEligibleOOO(Variable v) const {return AEL[v-1] == ELIGIBLE; }

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

  /* AEL = assignment eligibility level
   * AET = assignment eligibility trail
   * a variable v is eligible for assignment at decision level d iff
   * AEL[v-1] >= d
   *
   * ineligible variables are collected in increasing decision level order
   * on the assignment_eligibility_trail to be made eligible upon backtracking
   */
  vector<uint32_t> AEL;
  vector<Variable> AET;

};

// Implementation of inline methods.

inline void DependencyManagerWatched::addVariable(bool auxiliary, bool qtype) {
  variables_watched_by.emplace_back();
  variable_dependencies.emplace_back();
  is_auxiliary.push_back(auxiliary);
  if (!auxiliary) {
    if (out_of_order_decisions[qtype]) {
      AEL.push_back(ELIGIBLE);
    } else {
      AEL.push_back(PERMANENTLY_INELIGIBLE);
    }
  }
}

inline void DependencyManagerWatched::notifyUnassigned(Variable v) {}

inline void DependencyManagerWatched::learnDependencies(Variable unit_variable, vector<Literal>& literal_vector) {
  (*this.*learnDependenciesPtr)(unit_variable, literal_vector);
}

inline Variable DependencyManagerWatched::watcher(Variable v) const {
  return variable_dependencies[v - 1].watcher;
}

}

#endif
