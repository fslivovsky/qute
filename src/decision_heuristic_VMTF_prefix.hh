#ifndef decision_heuristic_VMTF_prefix_hh
#define decision_heuristic_VMTF_prefix_hh

#include <vector>
#include <queue>
#include "decision_heuristic.hh"
#include "qcdcl.hh"

using std::vector;
using std::priority_queue;

namespace Qute {

class DecisionHeuristicVMTFprefix: public DecisionHeuristic {

public:
  DecisionHeuristicVMTFprefix(QCDCL_solver& solver, bool no_phase_saving);
  virtual void addVariable(bool auxiliary);
  virtual void notifyStart();
  virtual void notifyAssigned(Literal l);
  virtual void notifyEligible(Variable v);
  virtual void notifyUnassigned(Literal l);
  virtual void notifyLearned(Constraint& c);
  virtual void notifyBacktrack(uint32_t decision_level_before);
  virtual Literal getDecisionLiteral();

protected:
  void resetTimestamps();
  void moveToFront(Variable variable);

  struct ListEntry
  {
    Variable prev;
    uint32_t timestamp;
    Variable next;
    ListEntry(Variable v): prev(v), timestamp(0), next(v) {}
  };

  struct VMTFMetadata
  {
    Variable list_head;
    Variable next_search;
    uint32_t num_vars_unassigned;
    VMTFMetadata(Variable first_variable): list_head(first_variable), next_search(first_variable), num_vars_unassigned(1) {}
  };

  struct CompareVariables
  {
    vector<ListEntry>& decision_list;
    bool operator()(const Variable& first, const Variable& second) const {
      return decision_list[first - 1].timestamp < decision_list[second - 1].timestamp;
    }
    CompareVariables(vector<ListEntry>& decision_list): decision_list(decision_list) {}
  };

  vector<lbool> saved_phase;
  vector<ListEntry> decision_list;
  vector<uint32_t> variable_depth;
  vector<VMTFMetadata> vmtf_data_for_block;
  uint32_t timestamp;
  uint32_t active_block[2];
  Variable last_variable;
  bool no_phase_saving;
  vector<bool> is_auxiliary;
};

// Implementation of inline methods.

inline void DecisionHeuristicVMTFprefix::notifyAssigned(Literal l) {
  if (!is_auxiliary[var(l) - 1]) {
    vmtf_data_for_block[variable_depth[var(l) - 1]].num_vars_unassigned--;
    saved_phase[var(l) - 1] = sign(l);
  }
}

inline void DecisionHeuristicVMTFprefix::notifyEligible(Variable v) {}

inline void DecisionHeuristicVMTFprefix::notifyBacktrack(uint32_t decision_level_before) {}

}

#endif