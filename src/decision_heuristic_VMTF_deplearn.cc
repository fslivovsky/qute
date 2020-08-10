#include "decision_heuristic_VMTF_deplearn.hh"

namespace Qute {

DecisionHeuristicVMTFdeplearn::DecisionHeuristicVMTFdeplearn(QCDCL_solver& solver, bool no_phase_saving): DecisionHeuristic(solver), list_head(0), next_search(0), timestamp(0), overflow_queue(CompareVariables(decision_list)), no_phase_saving(no_phase_saving) {}

void DecisionHeuristicVMTFdeplearn::addVariable(bool auxiliary) {
  saved_phase.push_back(l_Undef);
  is_auxiliary.push_back(auxiliary);
  if (decision_list.empty()) {
    decision_list.emplace_back();
    list_head = next_search = 1;
  } else {
    /* Add new variable at the end of the list if it's not an auxiliary variable.
     * Each auxiliary is contained in a singleton list. */
    Variable old_last = decision_list[list_head - 1].prev;
    ListEntry new_entry;
    if (!auxiliary) {
      new_entry.next = list_head;
      new_entry.timestamp = 0;
      new_entry.prev = old_last;
      decision_list[list_head - 1].prev = decision_list.size() + 1;
      decision_list[old_last - 1].next = decision_list.size() + 1;
    } else {
      new_entry.next = decision_list.size() + 1;
      new_entry.prev = decision_list.size() + 1;
      new_entry.timestamp = 0;
    }
    decision_list.push_back(new_entry);
  }
}

void DecisionHeuristicVMTFdeplearn::notifyUnassigned(Literal l) {
  Variable variable = var(l);
  if (!is_auxiliary[variable - 1]) {
    Variable watcher = solver.dependency_manager->watcher(variable);
    /* If variable will be unassigned after backtracking but its watcher still assigned,
      variable is eligible for assignment after backtracking. If its timestamp is better
      than that of next_search, we must update next_search. */
    if ((watcher == 0 || (solver.variable_data_store->isAssigned(watcher) && (solver.variable_data_store->varDecisionLevel(watcher) < backtrack_decision_level_before))) &&
        (decision_list[variable - 1].timestamp > decision_list[next_search - 1].timestamp)) {
      next_search = variable;
    }
  }
}

void DecisionHeuristicVMTFdeplearn::notifyLearned(Constraint& c, ConstraintType constraint_type, vector<Literal>& conflict_side_literals) {
  // Bump every assigned variable in the learned constraint.
  for (Literal l: c) {
    Variable v = var(l);
    if (solver.variable_data_store->isAssigned(v)) {
      moveToFront(v);
    }
  }
}

void DecisionHeuristicVMTFdeplearn::notifyBacktrack(uint32_t decision_level_before) {
  backtrack_decision_level_before = decision_level_before;
  clearOverflowQueue();
}

Literal DecisionHeuristicVMTFdeplearn::getDecisionLiteral() {
  Variable candidate = 0;
  // First, check the overflow queue for an unassigned variable.
  while (!overflow_queue.empty() && solver.variable_data_store->isAssigned(overflow_queue.top())) {
    overflow_queue.pop();
  }
  if (!overflow_queue.empty()) {
    candidate = overflow_queue.top();
    overflow_queue.pop();
  } else {
    // If no suitable variable was found in the overflow queue, search in the linked list, starting from next_search.
    while (!solver.dependency_manager->isDecisionCandidate(next_search) && decision_list[next_search - 1].next != list_head) {
      next_search = decision_list[next_search - 1].next;
    }
    candidate = next_search;
  }
  assert(candidate != 0);
  assert(!is_auxiliary[candidate - 1]);
  assert(solver.dependency_manager->isDecisionCandidate(candidate));
  assert(decision_list[candidate - 1].timestamp == maxTimestampEligible());
  if (no_phase_saving || saved_phase[candidate - 1] == l_Undef) {
    saved_phase[candidate - 1] = phaseHeuristic(candidate);
  }
  return mkLiteral(candidate, saved_phase[candidate - 1]);
}

void DecisionHeuristicVMTFdeplearn::resetTimestamps() {
  /* Assign timestamps in ascending order, starting from the back of the
     list. */
  timestamp = 0;
  Variable list_ptr = list_head;
  do {
    list_ptr = decision_list[list_ptr - 1].prev;
    decision_list[list_ptr - 1].timestamp = timestamp++;
  } while (list_ptr != list_head);
}

void DecisionHeuristicVMTFdeplearn::moveToFront(Variable variable) {
  Variable current_head = list_head;

  /* If the variable is already at the head of the list or an auxiliary variable,
     don't do anything. */
  if (current_head == variable || is_auxiliary[variable - 1]) {
    return;
  }

  /* Increase timestamp value */
  if (timestamp == ((uint32_t)-1)) {
    resetTimestamps();
  }
  decision_list[variable - 1].timestamp = ++timestamp;

  /* Detach variable from list */
  Variable current_prev = decision_list[variable - 1].prev;
  Variable current_next = decision_list[variable - 1].next;
  decision_list[current_prev - 1].next = current_next;
  decision_list[current_next - 1].prev = current_prev;

  /* Insert variable as list head */
  Variable current_head_prev = decision_list[current_head - 1].prev;
  decision_list[current_head - 1].prev = variable;
  decision_list[variable - 1].next = current_head;
  decision_list[variable - 1].prev = current_head_prev;
  decision_list[current_head_prev - 1].next = variable;
  list_head = variable;
  assert(checkOrder());
}

bool DecisionHeuristicVMTFdeplearn::checkOrder() {
  Variable list_ptr = list_head;
  Variable next = decision_list[list_ptr - 1].next;
  while (decision_list[list_ptr - 1].timestamp > decision_list[next - 1].timestamp) {
    list_ptr = decision_list[list_ptr - 1].next;
    next = decision_list[list_ptr - 1].next;
  }
  return next == list_head;
}

}