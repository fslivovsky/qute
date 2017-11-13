#include "constraint_DB.hh"

namespace Qute {

ConstraintDB::ConstraintDB(QCDCL_solver& solver, bool print_trace, double constraint_activity_decay, uint32_t max_learnt_clauses, uint32_t max_learnt_terms, uint32_t learnt_clauses_increment, uint32_t learnt_terms_increment, double clause_removal_ratio, double term_removal_ratio, bool use_activity_threshold, double constraint_increment): removal_ratio{clause_removal_ratio, term_removal_ratio}, solver(solver), print_trace(print_trace), constraints{ConstraintAllocator(print_trace), ConstraintAllocator(print_trace)}, constraint_inc{constraint_increment, constraint_increment}, constraint_activity_decay(constraint_activity_decay), learnts_max{max_learnt_clauses, max_learnt_terms}, learnts_increment{learnt_clauses_increment, learnt_terms_increment}, ca_to(nullptr), use_activity_threshold(use_activity_threshold) {}

CRef ConstraintDB::addConstraint(vector<Literal>& literals, ConstraintType constraint_type, bool learnt) {
  CRef constraint_reference = constraints[constraint_type].alloc(literals, learnt);
  if (learnt) {
    learnt_constraint_references[constraint_type].push_back(constraint_reference);
    Constraint& constraint = getConstraint(constraint_reference, constraint_type);
    updateLBD(constraint);
    bumpConstraintActivity(constraint, constraint_type);
    solver.decision_heuristic->notifyLearned(constraint);
  } else {
    input_constraint_references[constraint_type].push_back(constraint_reference);
    for (Literal l: literals) {
      literal_occurrences[constraint_type][l].push_back(constraint_reference);
    }
  }
  BOOST_LOG_TRIVIAL(trace) << (learnt ? "Learnt ": "Input ") << (constraint_type ? "term": "clause") << ": " << constraints[constraint_type][constraint_reference];
  return constraint_reference;
}

void ConstraintDB::updateLBD(Constraint& constraint) {
  vector<bool> levels(solver.variable_data_store->decisionLevel() + 1);
  std::fill(levels.begin(), levels.end(), false);
  uint32_t nr_levels = 0;
  for (Literal l: constraint) {
    Variable v = var(l);
    if (solver.variable_data_store->isAssigned(v) && !levels[solver.variable_data_store->varDecisionLevel(v)]) {
      levels[solver.variable_data_store->varDecisionLevel(v)] = true;
      nr_levels++;
    }
  }
  constraint.LBD() = nr_levels;
}

void ConstraintDB::relocConstraintReferences(ConstraintType constraint_type) {
  for (auto it = literal_occurrences[constraint_type].begin(); it != literal_occurrences[constraint_type].end(); ++it) {
    for (CRef& constraint_reference: it->second) {
      /* Since literal occurrences only consider input constraints, there is no need to check whether the
         corresponding constraint has been marked for removal. */
      relocate(constraint_reference, constraint_type); 
    }
  }
  for (CRef& constraint_reference: input_constraint_references[constraint_type]) {
    relocate(constraint_reference, constraint_type);
  }
  vector<CRef>::iterator i, j;
  for (i = j = learnt_constraint_references[constraint_type].begin(); i != learnt_constraint_references[constraint_type].end(); ++i) {
    Constraint& constraint = getConstraint(*i, constraint_type);
    if (!constraint.isMarked()) {
      relocate(*i, constraint_type); 
      *j++ = *i;
    }
  }
  learnt_constraint_references[constraint_type].resize(j - learnt_constraint_references[constraint_type].begin(), CRef_Undef);
}

void ConstraintDB::relocAll(ConstraintType constraint_type) {
  assert(ca_to == nullptr);
  ConstraintAllocator to(constraints[constraint_type].size() - constraints[constraint_type].wasted(), print_trace);
  ca_to = &to;

  solver.propagator->relocConstraintReferences(constraint_type);
  solver.variable_data_store->relocConstraintReferences(constraint_type);
  relocConstraintReferences(constraint_type);

  to.moveTo(constraints[constraint_type]);
  ca_to = nullptr;
}

void ConstraintDB::cleanConstraints(ConstraintType constraint_type) {
  sort(learnt_constraint_references[constraint_type].begin(), learnt_constraint_references[constraint_type].end(), ConstraintCompare(constraints[constraint_type]));
  uint32_t to_remove =  learnt_constraint_references[constraint_type].size() * removal_ratio[constraint_type];
  uint32_t removed_counter = 0;
  double threshold = constraint_inc[constraint_type] / learnt_constraint_references[constraint_type].size();
  for (CRef constraint_reference: learnt_constraint_references[constraint_type]) {
    Constraint& constraint = constraints[constraint_type][constraint_reference];
    if (!isLocked(constraint, constraint_reference, constraint_type) && constraint.LBD() > 2 &&
        (removed_counter < to_remove || (use_activity_threshold && constraint.activity() < threshold))) {
      constraint.mark();
      constraints[constraint_type].free(constraint_reference);
      removed_counter++;
    }
  }
  BOOST_LOG_TRIVIAL(info) << "Removed " << removed_counter << " learnt " << (constraint_type ? "terms": "clauses") << ".";
  relocAll(constraint_type);
}

bool ConstraintDB::isLocked(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type) {
  Variable v = var(constraint[0]);
  return (solver.variable_data_store->isAssigned(v) && solver.variable_data_store->varType(v) == constraint_type && solver.variable_data_store->varReason(v) == constraint_reference);
}

}