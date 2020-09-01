#ifndef decision_heuristic_vsids_deplearn_hh
#define decision_heuristic_vsids_deplearn_hh

#include "decision_heuristic.hh"
#include "solver_types.hh"
#include "constraint.hh"
#include "variable_data.hh"
#include "dependency_manager_watched.hh"
#include "qcdcl.hh"

#include "minisat/mtl/Heap.h"
#include "minisat/mtl/IntMap.h"

using std::find;
using Minisat::Heap;
using Minisat::IntMap;

namespace Qute {

class DecisionHeuristicVSIDSdeplearn: public DecisionHeuristic {

public:
  DecisionHeuristicVSIDSdeplearn(QCDCL_solver& solver, bool no_phase_saving, double score_decay_factor, double score_increment, bool tiebreak_scores, bool use_secondary_occurrences_for_tiebreaking, bool prefer_fewer_occurrences);
  virtual void addVariable(bool auxiliary);
  virtual void notifyStart();
  virtual void notifyAssigned(Literal l);
  virtual void notifyUnassigned(Literal l);
  virtual void notifyEligible(Variable v);
  virtual void notifyLearned(Constraint& c, ConstraintType constraint_type, vector<Literal>& conflict_side_literals);
  virtual void notifyBacktrack(uint32_t decision_level_before);
  virtual Literal getDecisionLiteral();

protected:
  void precomputeVariableOccurrences(bool use_secondary_occurrences_for_tiebreaking);
  void bumpVariableScore(Variable v);
  void rescaleVariableScores();
  void decayVariableScores();
  Variable popFromVariableQueue();
  double getBestDecisionVariableScore();
  vector<Variable> getVariablesWithTopScore();
  void popVariableWithTopScore(Variable v);
  Variable pickVarUsingOccurrences(vector<Variable>& candidates, bool prefer_fewer_occurrences);

  struct CompareVariables
  {
    CompareVariables(const IntMap<Variable, double>& variable_activity, const IntMap<Variable, int>& nr_literal_occurrences, bool tiebreak_scores, bool prefer_fewer_occurrences): tiebreak_scores(tiebreak_scores), prefer_fewer_occurrences(prefer_fewer_occurrences), variable_activity(variable_activity), nr_literal_occurrences(nr_literal_occurrences) {}
    bool operator()(const Variable first, const Variable second) const {
      if (!tiebreak_scores) {
        return variable_activity[first] > variable_activity[second];
      } else if (prefer_fewer_occurrences) {
        return (variable_activity[first] > variable_activity[second]) || (variable_activity[first] == variable_activity[second] && nr_literal_occurrences[first] < nr_literal_occurrences[second]);
      } else {
        return (variable_activity[first] > variable_activity[second]) || (variable_activity[first] == variable_activity[second] && nr_literal_occurrences[first] > nr_literal_occurrences[second]);
      }
    }
    bool tiebreak_scores;
    bool prefer_fewer_occurrences;
    const IntMap<Variable, double>& variable_activity;
    const IntMap<Variable, int>& nr_literal_occurrences;
  };

  vector<bool> is_auxiliary;
  bool no_phase_saving;
  double score_decay_factor;
  double score_increment;
  uint32_t backtrack_decision_level_before;
  bool tiebreak_scores;
  bool use_secondary_occurrences_for_tiebreaking;
  IntMap<Variable, int> nr_literal_occurrences;
  IntMap<Variable, double> variable_activity;
  Heap<Variable,CompareVariables> variable_queue;
};

// Implementation of inline methods

inline void DecisionHeuristicVSIDSdeplearn::precomputeVariableOccurrences(bool use_secondary_occurrences_for_tiebreaking) {
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    if (!is_auxiliary[v - 1] && solver.dependency_manager->isDecisionCandidate(v)) {
      /* Existentials are 'primary' literals in clauses and 'secondary' in terms.
         Conversely, universals are 'primary' in terms and 'secondary' in clauses. */
      ConstraintType constraint_type = constraint_types[use_secondary_occurrences_for_tiebreaking ^ solver.variable_data_store->varType(v)];
      nr_literal_occurrences.insert(v, nrLiteralOccurrences(mkLiteral(v, true), constraint_type) + nrLiteralOccurrences(mkLiteral(v, false), constraint_type));
    }
  }
}

inline void DecisionHeuristicVSIDSdeplearn::addVariable(bool auxiliary) {
  saved_phase.push_back(l_Undef);
  variable_activity.insert(solver.variable_data_store->lastVariable(), 0);
  is_auxiliary.push_back(auxiliary);
}

inline void DecisionHeuristicVSIDSdeplearn::notifyStart() {
  precomputeVariableOccurrences(use_secondary_occurrences_for_tiebreaking);
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    if (!is_auxiliary[v - 1] && solver.dependency_manager->isDecisionCandidate(v)) {
      variable_queue.insert(v);
    }
  }
}

inline void DecisionHeuristicVSIDSdeplearn::notifyAssigned(Literal l) {
  saved_phase[var(l) - 1] = sign(l);
}

inline void DecisionHeuristicVSIDSdeplearn::notifyEligible(Variable v) {
  if (!is_auxiliary[v - 1]) {
    variable_queue.update(v);
  }
}

inline void DecisionHeuristicVSIDSdeplearn::notifyLearned(Constraint& c, ConstraintType constraint_type, vector<Literal>& conflict_side_literals) {
  for (auto literal: c) {
    Variable v = var(literal);
    if (solver.variable_data_store->isAssigned(v) && !is_auxiliary[v - 1]) {
      bumpVariableScore(v);
    }
  }
  decayVariableScores();
}

inline void DecisionHeuristicVSIDSdeplearn::notifyBacktrack(uint32_t decision_level_before) {
  backtrack_decision_level_before = decision_level_before;
}

inline void DecisionHeuristicVSIDSdeplearn::bumpVariableScore(Variable v) {
  variable_activity[v] += score_increment;
  if (variable_queue.inHeap(v)) {
    variable_queue.update(v);
  }
  if (variable_activity[v] > 1e60) {
    rescaleVariableScores();
  }
}

inline void DecisionHeuristicVSIDSdeplearn::rescaleVariableScores() {
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    variable_activity[v] *= 1e-60;
    if (variable_queue.inHeap(v)) {
      variable_queue.update(v);
    }
  }
  score_increment *= 1e-60;
}

inline void DecisionHeuristicVSIDSdeplearn::decayVariableScores() {
  score_increment *= (1 / score_decay_factor);
}

inline Variable DecisionHeuristicVSIDSdeplearn::popFromVariableQueue() {
  assert(!variable_queue.empty());
  return variable_queue.removeMin();
}

inline double DecisionHeuristicVSIDSdeplearn::getBestDecisionVariableScore() {
  bool assigned = false;
  double best_decision_variable_score;
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    if (((variable_activity[v] > best_decision_variable_score) || !assigned) && solver.dependency_manager->isDecisionCandidate(v)) {
      best_decision_variable_score = variable_activity[v];
      assigned = true;
    }
  }
  return best_decision_variable_score;
}

}

#endif
