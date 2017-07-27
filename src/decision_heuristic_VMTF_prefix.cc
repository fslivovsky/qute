#include "decision_heuristic_VMTF_prefix.hh"

namespace Qute {

DecisionHeuristicVMTFprefix::DecisionHeuristicVMTFprefix(QCDCL_solver& solver, bool no_phase_saving): DecisionHeuristic(solver), last_variable(0), no_phase_saving(no_phase_saving) {}

void DecisionHeuristicVMTFprefix::addVariable(bool auxiliary) {
  last_variable++;
  decision_list.emplace_back(last_variable);
  is_auxiliary.push_back(auxiliary);
  saved_phase.push_back(l_Undef);
  if (!auxiliary) {
    if (last_variable == 1 || solver.variable_data_store->varType(last_variable) != solver.variable_data_store->varType(last_variable - 1)) {
      // New quantifier block.
      vmtf_data_for_block.emplace_back(last_variable);
    } else { // Variable added to existing quantifier block.
      vmtf_data_for_block.back().num_vars_unassigned++;
      Variable first_of_block = vmtf_data_for_block.back().list_head;
      Variable last_of_block = decision_list[first_of_block - 1].prev;
      decision_list[last_variable - 1].next = first_of_block;
      decision_list[last_variable - 1].prev = last_of_block;
      decision_list[first_of_block - 1].prev = last_variable;
      decision_list[last_of_block - 1].next = last_variable;
    }
  }
  variable_depth.push_back(vmtf_data_for_block.size() - 1);
}

void DecisionHeuristicVMTFprefix::notifyStart() {
  active_block[false] = active_block[true] = vmtf_data_for_block.size() + 1;
  if (vmtf_data_for_block.size() > 0) { // At least one quantifier block.
    active_block[solver.variable_data_store->varType(1)] = 0;
    if (vmtf_data_for_block.size() > 1) { // At least two quantifier blocks.
      active_block[!solver.variable_data_store->varType(1)] = 1;
    }
  }
  for (Variable v = 1; v <= last_variable; v++) {
    decision_list[v - 1].timestamp = last_variable - v;
  }
  timestamp = last_variable - 1;
}

void DecisionHeuristicVMTFprefix::notifyUnassigned(Literal l) {
  Variable v = var(l);
  if (!is_auxiliary[v - 1]) {
    bool qtype = solver.variable_data_store->varType(v);
    uint32_t depth = variable_depth[v - 1];
    /* Maintain the invariant that all blocks of type i before active_block[i]
    * have all variables assigned. */
    if (active_block[qtype] > depth)
      active_block[qtype] = depth;
    /* Maintain the invariant that all variables before next_search (with a higher timestamp)
    * are assigned. */
    if (decision_list[v - 1].timestamp > decision_list[vmtf_data_for_block[depth].next_search - 1].timestamp) {
      vmtf_data_for_block[depth].next_search = v;
    }
    vmtf_data_for_block[depth].num_vars_unassigned++;
  }
}

void DecisionHeuristicVMTFprefix::notifyLearned(Constraint& c) {
  // Bump every assigned variable in the learned constraint.
  for (Literal l: c) {
    Variable v = var(l);
    if (solver.variable_data_store->isAssigned(v)) {
      moveToFront(v);
    }
  }
}

Literal DecisionHeuristicVMTFprefix::getDecisionLiteral() {
  /* Update active blocks so that they point to the leftmost block
   * with unassigned variables of the respective type. */
  for (unsigned type = 0; type < 2; type++) {
    while (active_block[type] < vmtf_data_for_block.size() && vmtf_data_for_block[active_block[type]].num_vars_unassigned == 0) {
      active_block[type] += 2;
    }
  }
  /* Based on active blocks, determine the interval [free_blocks_begin, free_blocks_end)
   * of blocks considered for assignment. */
  uint32_t free_blocks_begin, free_blocks_end;
  if (active_block[false] < active_block[true]) {
    free_blocks_begin = active_block[false];
    free_blocks_end = active_block[true] < vmtf_data_for_block.size() ? active_block[true] : vmtf_data_for_block.size();
  } else {
    free_blocks_begin = active_block[true];
    free_blocks_end = active_block[false] < vmtf_data_for_block.size() ? active_block[false] : vmtf_data_for_block.size();
  }
  Variable candidate = 0;
  uint32_t candidate_timestamp = 0;
  // Only one quantifier type is ready for assignment, so iterate blocks at a step of 2.
  for (uint32_t current_block = free_blocks_begin; current_block < free_blocks_end; current_block += 2) {
    // Not all blocks in the range need to actually contain unassigned variables.
    if (vmtf_data_for_block[current_block].num_vars_unassigned == 0) {
      continue;
    }
    /* Find the unassigned variable with the largest timestamp in the current block.
    * Only search as long as the timestamps are greater than the candidate timestamp,
    * we don't want smaller ones anyway. Equality is included, because the 0 at the
    * beginning is actually smaller than all timestamps, but we cannot use -1 for
    * an unsigned int. All timestamps are pairwise different. */
    while (solver.variable_data_store->isAssigned(vmtf_data_for_block[current_block].next_search) && decision_list[vmtf_data_for_block[current_block].next_search - 1].timestamp >= candidate_timestamp) {
      vmtf_data_for_block[current_block].next_search = decision_list[vmtf_data_for_block[current_block].next_search - 1].next;
    }
    Variable new_candidate = vmtf_data_for_block[current_block].next_search;
    /* If new candidate is better than what we had so far, replace.
      * Trick with equality again. */
    if (decision_list[new_candidate - 1].timestamp >= candidate_timestamp) {
      candidate = new_candidate;
      candidate_timestamp = decision_list[new_candidate - 1].timestamp;
    }
  }
  assert(candidate != 0);
  assert(solver.dependency_manager->isDecisionCandidate(candidate));
  if (no_phase_saving || saved_phase[candidate - 1] == l_Undef) {
    saved_phase[candidate - 1] = phaseHeuristic(candidate);
  }
  return mkLiteral(candidate, saved_phase[candidate - 1]);
}

void DecisionHeuristicVMTFprefix::resetTimestamps() {
  /* Perform a k-way merge to traverse the variables in DESCENDING order of their timestamps
    * (k is the number of blocks) (descending because that way CompareVariables can be reused as is). */
  timestamp = last_variable - 1; // Reset largest timestamp.
  // Vector of pointers to the queues that keeps track of position in each queue as we move through them.
  vector<Variable> next_in_line;
  next_in_line.resize(vmtf_data_for_block.size());
  /* This is basically like heapsort, except the heap can never grow larger than k (the number of blocks),
    * because we always know that the next element is one of the k next-in-lines. Hence the complexity is
    * O(n*log(k)), n being the number of variables, k the number of blocks. */
  priority_queue<Variable, vector<Variable>, CompareVariables> merger(decision_list);
  for (unsigned block = 0; block < vmtf_data_for_block.size(); block++) {
    // Insert the head of each list to the priority queue, designate its successor as next in line.
    merger.push(vmtf_data_for_block[block].list_head);
    next_in_line[block] = decision_list[vmtf_data_for_block[block].list_head].next;
  }
  while (!merger.empty()) {
    Variable current_variable = merger.top();
    merger.pop();
    uint32_t current_depth = variable_depth[current_variable - 1];
    decision_list[current_variable - 1].timestamp = timestamp--;
    // If we hadn't depleted block current_depth yet, add its next_in_line.
    if (next_in_line[current_depth] != vmtf_data_for_block[current_depth].list_head) {
      merger.push(next_in_line[current_depth]);
      next_in_line[current_depth] = decision_list[next_in_line[current_depth]].next;
    }
  }
  timestamp = last_variable - 1;
}

void DecisionHeuristicVMTFprefix::moveToFront(Variable variable) {
  
  if (is_auxiliary[variable - 1]) {
    return;
  }

  Variable current_head = vmtf_data_for_block[variable_depth[variable - 1]].list_head;

  if (current_head == variable) {
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
  vmtf_data_for_block[variable_depth[variable - 1]].list_head = variable;
}

}