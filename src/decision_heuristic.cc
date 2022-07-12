#include "decision_heuristic.hh"
#include "variable_data.hh"
#include "watched_literal_propagator.hh"
#include "constraint_DB.hh"
#include "qcdcl.hh"

namespace Qute {

DecisionHeuristic::DecisionHeuristic(QCDCL_solver& solver): solver(solver), phase_heuristic(PhaseHeuristicOption::INVJW), conflict_counter(0) {
  distribution = bernoulli_distribution(0.5);
}

bool DecisionHeuristic::phaseHeuristic(Variable v) {
  switch (phase_heuristic)
  {
    case PhaseHeuristicOption::INVJW:
      return invJeroslowWang(v);
    case PhaseHeuristicOption::QTYPE:
      return qtypeDecHeur(v);
    case PhaseHeuristicOption::WATCHER:
      return watcherPhaseHeuristic(v);
    case PhaseHeuristicOption::RANDOM:
      return randomPhase();
    case PhaseHeuristicOption::PHFALSE:
      return false;
    case PhaseHeuristicOption::PHTRUE:
      return true;
  }
  return false;
  // return svmPhase(v);
}

/*bool DecisionHeuristic::svmPhase(Variable v) {
  if (conflict_counter == 0) {
    return false;
  }
  if (trainer_map.find(v) == trainer_map.end()) {
    trainer_map[v] = dlib::svm_pegasos<kernel_type>();
    trainer_map[v].set_lambda(0.00001);
    last_update[v] = 0;
  }
  auto& trainer = trainer_map[v];
  // auto variable_type = solver.variable_data_store->varType(v);

  auto nr_new_assignments = min(conflict_counter - last_update[v], static_cast<uint32_t>(500));
  //if (nr_new_assignments == 500) {
    assert(conflict_counter > 0);
    auto current_position = (conflict_counter - 1) % 500;
    assert(0 <= current_position && current_position < assignment_cache.size());
    for (unsigned i = 0; i < nr_new_assignments; i++) {
      auto index = (current_position - i) % 500;
      assert(0 <= index && index < assignment_cache.size());
      auto& assignment = assignment_cache[index];
      if (assignment.find(v) != assignment.end()) {//  && label_cache[index] != variable_type) {
        sample_type sample;
        for (auto& kv: assignment) {
          if (kv.first < v) {
            sample[kv.first] = 2 * kv.second - 1;
          }
        }
        trainer.train(sample, 2 * assignment[v] - 1);
      }
    }
  //}

  sample_type sample;
  for (auto it = solver.variable_data_store->trailBegin(); it != solver.variable_data_store->trailEnd(); ++it) {
    auto trail_literal = *it;
    auto trail_variable = var(trail_literal);
    if (trail_variable < v) {
      sample[trail_variable] = 2 * sign(trail_literal) - 1;
    }
  }
  last_update[v] = conflict_counter;
  return trainer(sample) > 0;

}*/

void DecisionHeuristic::notifyConflict(ConstraintType constraint_type) {
  conflict_counter++;
  /* map<Variable,bool> assignment;
  for (auto it = solver.variable_data_store->trailBegin(); it != solver.variable_data_store->trailEnd(); ++it) {
    Literal trail_literal = *it;
    assignment[var(trail_literal)] = sign(trail_literal);
  }
  if (conflict_counter <= 500) {
    assignment_cache.push_back(assignment);
    label_cache.push_back(constraint_type);
  } else {
    assignment_cache[conflict_counter % 500] = assignment;
    label_cache[conflict_counter % 500] = constraint_type;
  }*/
}

bool DecisionHeuristic::invJeroslowWang(Variable v) {
  /* We want to "guess" a polarity so that we do not have to undo this
     assignment later. For a universal variable, that means we want to get to
     a conflict and falsify every clause. */
  if (solver.variable_data_store->varType(v)) {
    // Universal variable.
    return (invJeroslowWangScore(mkLiteral(v, false), ConstraintType::clauses) >
            invJeroslowWangScore(mkLiteral(v, true), ConstraintType::clauses));
  }
  else {
    return (invJeroslowWangScore(mkLiteral(v, true), ConstraintType::terms) >
            invJeroslowWangScore(mkLiteral(v, false), ConstraintType::terms));
  }
}

inline bool DecisionHeuristic::watcherPhaseHeuristic(Variable v) {
  return solver.propagator->phaseAdvice(v);
}

double DecisionHeuristic::invJeroslowWangScore(Literal l, ConstraintType constraint_type) {
  double score = 0;
  for (vector<CRef>::const_iterator it = solver.constraint_database->literalOccurrencesBegin(l, constraint_type);
       it != solver.constraint_database->literalOccurrencesEnd(l, constraint_type);
       ++it) {
    Constraint& constraint = solver.constraint_database->getConstraint(*it, constraint_type);
    score += (1 << constraint.size());
  }
  return score;
}

bool DecisionHeuristic::qtypeDecHeur(Variable v) {
  /* We want to "guess" a polarity so that we do not have to undo this
     assignment later. For a universal variable, that means we want to get to
     a conflict and falsify every clause. */
  if (solver.variable_data_store->varType(v)) {
    // Universal variable.
    return (nrLiteralOccurrences(mkLiteral(v, false), ConstraintType::clauses) >
            nrLiteralOccurrences(mkLiteral(v, true), ConstraintType::clauses));
  }
  else {
    return (nrLiteralOccurrences(mkLiteral(v, true), ConstraintType::terms) >
            nrLiteralOccurrences(mkLiteral(v, false), ConstraintType::terms));
  }
}

int DecisionHeuristic::nrLiteralOccurrences(Literal l, ConstraintType constraint_type) {
  return solver.constraint_database->literalOccurrencesEnd(l, constraint_type) - solver.constraint_database->literalOccurrencesBegin(l, constraint_type);
}

}
