#include "decision_heuristic_VSIDS_deplearn.hh"

namespace Qute {

DecisionHeuristicVSIDSdeplearn::DecisionHeuristicVSIDSdeplearn(QCDCL_solver& solver, bool no_phase_saving, double score_decay_factor, double score_increment, bool tiebreak_scores, bool use_secondary_occurrences_for_tiebreaking, bool prefer_fewer_occurrences): DecisionHeuristic(solver), variable_queue(CompareVariables(tiebreak_scores, prefer_fewer_occurrences)), no_phase_saving(no_phase_saving), score_decay_factor(score_decay_factor), score_increment(score_increment), tiebreak_scores(tiebreak_scores), use_secondary_occurrences_for_tiebreaking(use_secondary_occurrences_for_tiebreaking) {}

void DecisionHeuristicVSIDSdeplearn::notifyUnassigned(Literal l) {
  Variable v = var(l);
  if (!is_auxiliary[v - 1]) {
    Variable watcher = solver.dependency_manager->watcher(v);
    /* If variable will be unassigned after backtracking but its watcher still assigned,
      variable is eligible for assignment after backtracking. */
    if ((watcher == 0 || (solver.variable_data_store->isAssigned(watcher) && solver.variable_data_store->varDecisionLevel(watcher) < backtrack_decision_level_before && !in_queue[v - 1]))) {
      uint32_t nr_occurrences = tiebreak_scores ? nr_literal_occurrences[v - 1] : 0;
      handles[v - 1] = variable_queue.push(VariableNode(v, variable_score[v - 1], nr_occurrences));
      in_queue[v - 1] = true;
    }
  }
}

Literal DecisionHeuristicVSIDSdeplearn::getDecisionLiteral() {
  Variable candidate = 0;
  while (!variable_queue.empty() && !solver.dependency_manager->isDecisionCandidate(variable_queue.top().v)) {
    popFromVariableQueue();
  }
  candidate = popFromVariableQueue();
  assert(candidate != 0);
  assert(!is_auxiliary[candidate - 1]);
  assert(solver.dependency_manager->isDecisionCandidate(candidate));
  assert(variable_score[candidate - 1] == getBestDecisionVariableScore());
  if (no_phase_saving || saved_phase[candidate - 1] == l_Undef) {
    saved_phase[candidate - 1] = phaseHeuristic(candidate);
  }
  return mkLiteral(candidate, saved_phase[candidate - 1]);
}

}