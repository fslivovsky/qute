#ifndef decision_heuristic_VMTF_deplearn_hh
#define decision_heuristic_VMTF_deplearn_hh

#include "decision_heuristic.hh"
#include "dependency_manager_watched.hh"
#include "variable_data.hh"
#include "qcdcl.hh"

#include <vector>
#include <queue>
#include <random>

using std::vector;
using std::priority_queue;
using std::random_device;
using std::bernoulli_distribution;

namespace Qute {

class DecisionHeuristicVMTFdeplearn: public DecisionHeuristic {

public:
  DecisionHeuristicVMTFdeplearn(QCDCL_solver& solver, bool no_phase_saving);

  virtual void addVariable(bool auxiliary);
  virtual void notifyStart();
  virtual void notifyAssigned(Literal l);
  virtual void notifyEligible(Variable v);
  virtual void notifyUnassigned(Literal l);
  virtual void notifyLearned(Constraint& c, ConstraintType constraint_type, vector<Literal>& conflict_side_literals);
  virtual void notifyBacktrack(uint32_t decision_level_before);
  virtual Literal getDecisionLiteral();

protected:
  void resetTimestamps();
  void moveToFront(Variable variable);
  void clearOverflowQueue();
  uint32_t maxTimestampEligible();
  bool checkOrder();

  struct ListEntry
  {
    Variable prev;
    uint32_t timestamp;
    Variable next;
    ListEntry(): prev(1), timestamp(0), next(1) {}
  };

  struct CompareVariables
  {
    vector<ListEntry>& decision_list;
    bool operator()(const Variable& first, const Variable& second) const {
      return decision_list[first - 1].timestamp < decision_list[second - 1].timestamp;
    }
    CompareVariables(vector<ListEntry>& decision_list): decision_list(decision_list) {}
  };

  Variable list_head;
  Variable next_search;
  uint32_t timestamp;
  vector<ListEntry> decision_list;
  priority_queue<Variable, vector<Variable>, CompareVariables> overflow_queue;
  uint32_t backtrack_decision_level_before;

  bool no_phase_saving;
  vector<bool> is_auxiliary;

};

// Implementation of inline methods.

inline void DecisionHeuristicVMTFdeplearn::notifyStart() {
  Variable list_ptr = list_head;
  if (list_head) {
    do {
      list_ptr = decision_list[list_ptr - 1].prev;
      decision_list[list_ptr - 1].timestamp = timestamp++;
    } while (list_ptr != list_head);
  }
}

inline void DecisionHeuristicVMTFdeplearn::notifyAssigned(Literal l) {
  saved_phase[var(l) - 1] = sign(l);
}

inline void DecisionHeuristicVMTFdeplearn::notifyEligible(Variable v) {
  if (decision_list[v - 1].timestamp > decision_list[next_search - 1].timestamp && !is_auxiliary[v - 1]) {
    overflow_queue.push(v);
  }
}

inline void DecisionHeuristicVMTFdeplearn::clearOverflowQueue() {
  while (!overflow_queue.empty()) {
    auto variable = overflow_queue.top();
    auto watcher = solver.dependency_manager->watcher(variable);
    if ((watcher == 0 || (solver.variable_data_store->isAssigned(watcher) && (solver.variable_data_store->varDecisionLevel(watcher) < backtrack_decision_level_before)) || solver.dependency_manager->isEligibleOOO(variable)) &&
         (decision_list[variable - 1].timestamp > decision_list[next_search - 1].timestamp)) {
      next_search = variable;
    }
    overflow_queue.pop();
  }
}

inline uint32_t DecisionHeuristicVMTFdeplearn::maxTimestampEligible() {
  Variable v = list_head;
  uint32_t max_timestamp = 0;
  do {
    if (solver.dependency_manager->isDecisionCandidate(v) && decision_list[v - 1].timestamp > max_timestamp) {
      assert(!is_auxiliary[v - 1]);
      max_timestamp = decision_list[v - 1].timestamp;
    }
    v = decision_list[v - 1].next;
  } while (v != list_head);
  return max_timestamp;
}

}

#endif
