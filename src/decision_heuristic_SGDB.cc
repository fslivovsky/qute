#include "decision_heuristic_SGDB.hh"

namespace Qute {

DecisionHeuristicSGDB::DecisionHeuristicSGDB(QCDCL_solver& solver, bool no_phase_saving, double initial_learning_rate, double learning_rate_decay, double minimum_learning_rate, double lambda_factor): DecisionHeuristic(solver), no_phase_saving(no_phase_saving), bias(0), learning_rate(initial_learning_rate), learning_rate_decay(learning_rate_decay), minimum_learning_rate(minimum_learning_rate), lambda_factor(lambda_factor), lambda(initial_learning_rate*lambda_factor), universal_queue(CompareVariables(true, coefficient)), existential_queue(CompareVariables(false, coefficient)), current_activation(0) {}

void DecisionHeuristicSGDB::notifyUnassigned(Literal l) {
  auto v = var(l);
  current_activation -= coefficient[v];
  auto& variable_record = variable_data[v-1];
  if (!variable_record.is_auxiliary) {
    Variable watcher = solver.dependency_manager->watcher(v);
    /* If variable will be unassigned after backtracking but its watcher still assigned,
      variable is eligible for assignment after backtracking. */
    if ((watcher == 0 || (solver.variable_data_store->isAssigned(watcher) && solver.variable_data_store->varDecisionLevel(watcher) < backtrack_decision_level_before))) {
      auto variable_type = solver.variable_data_store->varType(v);
      if (variable_type) {
        universal_queue.update(v);
      } else {
        existential_queue.update(v);
      }
    }
  }
}

void DecisionHeuristicSGDB::notifyLearned(Constraint& c, ConstraintType constraint_type, vector<Literal>& conflict_side_literals) {
  double activation = bias;
  updateParameters();
  for (auto& l: c) {
    auto v = var(l);
    if (solver.variable_data_store->isAssigned(v)) {
      lazyRegularize(v);
      activation += coefficient[v];
      assigned_conflict_characteristic[v-1] = 1;
    }
  }
  for (auto& l: conflict_side_literals) {
    auto v = var(l);
    lazyRegularize(v);
    activation += coefficient[v];
    assigned_conflict_characteristic[v-1] = 1;
  }

  double prediction = sigmoid(activation); // Is this assignment a conflict (or a solution)?
  double sign = constraint_type ? 1 : -1;
  double error_gradient = sign * prediction * (1 - prediction);
  /* if (conflict_counter > output_last + 500) {
    output_last = conflict_counter;
    //cout << "Sigmoid for assignment: " << prediction << "\n";
    //cout << (constraint_type ? "Term" : "Clause") << "\n";
  } */
  // cout << "Error gradient: " << error_gradient << "\n";
  bias = bias * (1 - learning_rate*lambda/2) - learning_rate/2*(error_gradient);
  current_activation = bias;
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    if (assigned_conflict_characteristic[v-1] == 1) {
      coefficient[v] *=  1 - learning_rate*lambda/2;
      coefficient[v] -= learning_rate/2 * error_gradient;
      bool variable_type = solver.variable_data_store->varType(v);
      if (!variable_data[v-1].is_auxiliary) {
        if (variable_type) {
          universal_queue.update(v);
        } else {
          existential_queue.update(v);
        }
      }
    }
    assigned_conflict_characteristic[v-1] = 0;
    if (solver.variable_data_store->isAssigned(v)) {
      current_activation += coefficient[v];
    }
  }
}

void DecisionHeuristicSGDB::notifyConflict(ConstraintType constraint_type) {
  conflict_counter++;
}

Literal DecisionHeuristicSGDB::getDecisionLiteral() {
  bool assignment_predicted_conflict = assignmentPredictedConflict();
  Variable candidate = candidateFromVariableQueue(assignment_predicted_conflict);
  assert(!candidate || abs(maxCoeff(constraint_types[assignment_predicted_conflict]) - coefficient[candidate]) < 1e-7);
  if (candidate == 0) {
    candidate = candidateFromVariableQueue(!assignment_predicted_conflict);
    assert(!candidate || abs(maxCoeff(constraint_types[!assignment_predicted_conflict]) - coefficient[candidate]) < 1e-7);
  }
  assert(candidate != 0);
  lazyRegularize(candidate);
  assert(!variable_data[candidate-1].is_auxiliary);
  assert(solver.dependency_manager->isDecisionCandidate(candidate));
  // cout << "Candidate coefficient: " << coefficient[candidate] << "\n";
  if (no_phase_saving || saved_phase[candidate - 1] == l_Undef) {
    saved_phase[candidate - 1] = phaseHeuristic(candidate);
  }
  return mkLiteral(candidate, saved_phase[candidate - 1]);
}

}
