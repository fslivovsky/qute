#ifndef decision_heuristic_vsids_deplearn_hh
#define decision_heuristic_vsids_deplearn_hh

#include <boost/heap/binomial_heap.hpp>
#include "qcdcl.hh"
#include "solver_types.hh"
#include "constraint.hh"

using namespace boost::heap;
using std::priority_queue;

namespace Qute {

class DecisionHeuristicVSIDSdeplearn: public DecisionHeuristic {

public:
  DecisionHeuristicVSIDSdeplearn(QCDCL_solver& solver, bool no_phase_saving, double score_decay_factor, double score_increment, bool tiebreak_scores, bool use_secondary_occurrences_for_tiebreaking, bool prefer_fewer_occurrences);
  virtual void addVariable(bool auxiliary);
  virtual void notifyStart();
  virtual void notifyAssigned(Literal l);
  virtual void notifyUnassigned(Literal l);
  virtual void notifyEligible(Variable v);
  virtual void notifyLearned(Constraint& c);
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

  struct VariableNode {
    Variable v;
    double activity;
    uint32_t occurrences;
    VariableNode(Variable v, double activity, uint32_t occurrences): v(v), activity(activity), occurrences(occurrences) {}
  };

  struct CompareVariables
  {
    CompareVariables(bool tiebreak_scores, bool prefer_fewer_occurrences): tiebreak_scores(tiebreak_scores), prefer_fewer_occurrences(prefer_fewer_occurrences) {}
    bool operator()(const VariableNode& first, const VariableNode& second) const {
      if (!tiebreak_scores) {
        return first.activity < second.activity;
      } else if (prefer_fewer_occurrences) {
        return (first.activity < second.activity) || (first.activity == second.activity && first.occurrences < second.occurrences);
      } else {
        return (first.activity < second.activity) || (first.activity == second.activity && first.occurrences > second.occurrences);
      }
    }
    bool tiebreak_scores;
    bool prefer_fewer_occurrences;
  };

  vector<double> variable_score;
  binomial_heap<VariableNode,boost::heap::compare<CompareVariables>> variable_queue;
  vector<bool> is_auxiliary;
  vector<bool> in_queue;
  vector<binomial_heap<VariableNode,compare<CompareVariables>>::handle_type> handles;
  bool no_phase_saving;
  vector<lbool> saved_phase;
  double score_decay_factor;
  double score_increment;
  uint32_t backtrack_decision_level_before;
  vector<uint32_t> nr_literal_occurrences;
  bool tiebreak_scores;
  bool use_secondary_occurrences_for_tiebreaking;

};

// Implementation of inline methods

inline void DecisionHeuristicVSIDSdeplearn::precomputeVariableOccurrences(bool use_secondary_occurrences_for_tiebreaking) {
  nr_literal_occurrences.resize(variable_score.size());
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    if (!is_auxiliary[v - 1] && solver.dependency_manager->isDecisionCandidate(v)) {
      /* Existentials are 'primary' literals in clauses and 'secondary' in terms.
         Conversely, universals are 'primary' in terms and 'secondary' in clauses. */
      ConstraintType constraint_type = constraint_types[use_secondary_occurrences_for_tiebreaking ^ solver.variable_data_store->varType(v)];
      nr_literal_occurrences[v - 1] = nrLiteralOccurrences(mkLiteral(v, true), constraint_type) + nrLiteralOccurrences(mkLiteral(v, false), constraint_type);
    }
  }
}

inline void DecisionHeuristicVSIDSdeplearn::addVariable(bool auxiliary) {
  saved_phase.push_back(l_Undef);
  variable_score.push_back(0);
  is_auxiliary.push_back(auxiliary);
  in_queue.push_back(false);
}

inline void DecisionHeuristicVSIDSdeplearn::notifyStart() {
  handles.resize(variable_score.size());
  precomputeVariableOccurrences(use_secondary_occurrences_for_tiebreaking);
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    if (!is_auxiliary[v - 1] && solver.dependency_manager->isDecisionCandidate(v)) {
      uint32_t nr_occurrences = tiebreak_scores ? nr_literal_occurrences[v - 1] : 0;
      handles[v - 1] = variable_queue.push(VariableNode(v, 0, nr_occurrences));
      in_queue[v - 1] = true;
    }
  }
}

inline void DecisionHeuristicVSIDSdeplearn::notifyAssigned(Literal l) {
  saved_phase[var(l) - 1] = sign(l);
}

inline void DecisionHeuristicVSIDSdeplearn::notifyEligible(Variable v) {
  if (!in_queue[v - 1]) {
    uint32_t nr_occurrences = tiebreak_scores ? nr_literal_occurrences[v - 1] : 0;
    handles[v - 1] = variable_queue.push(VariableNode(v, variable_score[v - 1], nr_occurrences));
    in_queue[v - 1] = true;
  }
}

inline void DecisionHeuristicVSIDSdeplearn::notifyLearned(Constraint& c) {
  for (auto literal: c) {
    Variable v = var(literal);
    if (solver.variable_data_store->isAssigned(v) && !is_auxiliary[v - 1]) {
      bumpVariableScore(v);
    }
  }
}

inline void DecisionHeuristicVSIDSdeplearn::notifyBacktrack(uint32_t decision_level_before) {
  backtrack_decision_level_before = decision_level_before;
}

inline void DecisionHeuristicVSIDSdeplearn::bumpVariableScore(Variable v) {
  variable_score[v - 1] += score_increment;
  if (in_queue[v - 1]) {
    (*handles[v - 1]).activity = variable_score[v - 1];
    variable_queue.increase(handles[v - 1]);
  }
  if (variable_score[v - 1] > 1e60) {
    rescaleVariableScores();
  }
}

inline void DecisionHeuristicVSIDSdeplearn::rescaleVariableScores() {
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    variable_score[v] *= 1e-60;
    if (in_queue[v - 1]) {
      (*handles[v - 1]).activity = variable_score[v - 1];
      variable_queue.decrease(handles[v - 1]);
    }
  }
  score_increment *= 1e-60;
}

inline void DecisionHeuristicVSIDSdeplearn::decayVariableScores() {
  score_increment *= (1 / score_decay_factor);
}

inline Variable DecisionHeuristicVSIDSdeplearn::popFromVariableQueue() {
  assert(!variable_queue.empty());
  VariableNode n = variable_queue.top();
  variable_queue.pop();
  in_queue[n.v - 1] = false;
  return n.v;
}

inline double DecisionHeuristicVSIDSdeplearn::getBestDecisionVariableScore() {
  bool assigned = false;
  double best_decision_variable_score;
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    if (((variable_score[v - 1] > best_decision_variable_score) || !assigned) && solver.dependency_manager->isDecisionCandidate(v)) {
      best_decision_variable_score = variable_score[v - 1];
      assigned = true;
    }
  }
  return best_decision_variable_score;
}

}

#endif