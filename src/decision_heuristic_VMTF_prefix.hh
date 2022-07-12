#ifndef decision_heuristic_VMTF_prefix_hh
#define decision_heuristic_VMTF_prefix_hh

#include "decision_heuristic.hh"
#include <vector>
#include <queue>

namespace Qute {

class QCDCL_solver;

class DecisionHeuristicVMTFprefix: public DecisionHeuristic {

public:
  DecisionHeuristicVMTFprefix(QCDCL_solver& solver, bool no_phase_saving);
  virtual void addVariable(bool auxiliary);
  virtual void notifyStart();
  virtual void notifyAssigned(Literal l);
  virtual void notifyEligible(Variable v);
  virtual void notifyUnassigned(Literal l);
  virtual void notifyLearned(Constraint& c, ConstraintType constraint_type, std::vector<Literal>& conflict_side_literals);
  virtual void notifyBacktrack(uint32_t decision_level_before);
  virtual Literal getDecisionLiteral();

protected:
  void resetTimestamps();
  void moveToFront(Variable variable);

  struct ListEntry
  { // we store two lists in one place
    Variable prev;
    Variable prev_ooo;
    uint32_t timestamp;
    Variable next;
    Variable next_ooo;
    ListEntry(Variable v): prev(v), prev_ooo(v), timestamp(0), next(v), next_ooo(v) {}
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
    std::vector<ListEntry>& decision_list;
    bool operator()(const Variable& first, const Variable& second) const {
      return decision_list[first - 1].timestamp < decision_list[second - 1].timestamp;
    }
    CompareVariables(vector<ListEntry>& decision_list): decision_list(decision_list) {}
  };

  inline void detachVar(Variable v);
  inline void detachVarOOO(Variable v);
  inline void makeVarBlockHead(Variable v);
  inline void makeVarOOOHead(Variable v);

  /* decision_list contains a set of linked lists for each quantifier block (linked by .prev and .next)
   * and also another global linked list to determine eligibility for out-of-order decisions (linked by .prev_ooo and .next_ooo)
   * the heads and search pointers for each block are stored in vmtf_data_for_block,
   * the global head and search pointer are ooo_head and next_search_ooo
   * TODO: clean this up
   */
  std::vector<ListEntry> decision_list;
  Variable next_search_ooo;
  Variable ooo_head;
  std::vector<uint32_t> variable_depth;
  std::vector<VMTFMetadata> vmtf_data_for_block;
  uint32_t timestamp;
  uint32_t active_block[2];
  Variable last_variable;
  bool no_phase_saving;
  std::vector<bool> is_auxiliary;
};

// Implementation of inline methods.

inline void DecisionHeuristicVMTFprefix::notifyAssigned(Literal l) {
  if (!is_auxiliary[var(l) - 1]) {
    vmtf_data_for_block[variable_depth[var(l) - 1]].num_vars_unassigned--;
    saved_phase[var(l) - 1] = sign(l);
  }
}

inline void DecisionHeuristicVMTFprefix::notifyBacktrack(uint32_t decision_level_before) {}

}

#endif
