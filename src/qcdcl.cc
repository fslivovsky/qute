#include "qcdcl.hh"

namespace Qute {

QCDCL_solver::QCDCL_solver(): variable_data_store(nullptr), constraint_database(nullptr), propagator(nullptr), decision_heuristic(nullptr), dependency_manager(nullptr), restart_scheduler(nullptr), learning_engine(nullptr), interrupt_flag(false) {}

QCDCL_solver::~QCDCL_solver() {}

void QCDCL_solver::addVariable(string original_name, char variable_type, bool auxiliary) {
  bool var_type = (variable_type == 'a');
  variable_data_store->addVariable(original_name, var_type);
  propagator->addVariable();
  decision_heuristic->addVariable(auxiliary);
  dependency_manager->addVariable(auxiliary);
}

void QCDCL_solver::addConstraint(vector<Literal>& literals, ConstraintType constraint_type) {
  sort(literals.begin(), literals.end());
  literals.erase(unique(literals.begin(), literals.end()), literals.end());
  CRef constraint_reference = constraint_database->addConstraint(literals, constraint_type, false);
  propagator->addConstraint(constraint_reference, constraint_type);
}

void QCDCL_solver::addDependency(Variable of, Variable on) {
  dependency_manager->addDependency(of, on); 
  solver_statistics.nr_dependencies++; // For now, we assume that dependencies do not get added twice.
}

lbool QCDCL_solver::solve() {
  constraint_database->notifyStart();
  dependency_manager->notifyStart();
  decision_heuristic->notifyStart();
  while (true) {
    if (interrupt_flag) {
      return l_Undef;
    }
    ConstraintType constraint_type;
    CRef conflict_constraint_reference = propagator->propagate(constraint_type);
    if (conflict_constraint_reference == CRef_Undef) {
      Literal l = decision_heuristic->getDecisionLiteral();
      enqueue(l, CRef_Undef);
      solver_statistics.nr_decisions++;
    } else {
      uint32_t decision_level_backtrack_before;
      Literal unit_literal;
      vector<Literal> literal_vector; // Represents a learned constraint or a set of new dependencies to be learned.
      bool constraint_learned;
      learning_engine->analyzeConflict(conflict_constraint_reference, constraint_type, literal_vector, decision_level_backtrack_before, unit_literal, constraint_learned);
      if (constraint_learned) {
        if (literal_vector.empty()) {
          return lbool(constraint_type);
        } else {
          CRef learned_constraint_reference = constraint_database->addConstraint(literal_vector, constraint_type, true);
          backtrackBefore(decision_level_backtrack_before);
          enqueue(unit_literal ^ constraint_type, learned_constraint_reference);
          propagator->addConstraint(learned_constraint_reference, constraint_type);
          solver_statistics.learned_total[constraint_type]++;
        }
      } else {
        Variable unit_variable = var(unit_literal);
        for (Literal literal: literal_vector) {
          assert(unit_variable != var(literal));
          dependency_manager->addDependency(unit_variable, var(literal));
          solver_statistics.nr_dependencies++;
        }
        auto decision_level_backtrack_before = variable_data_store->varDecisionLevel(unit_variable);
        backtrackBefore(decision_level_backtrack_before);
        solver_statistics.backtracks_dep++;
      }
      constraint_database->notifyConflict(constraint_type);
      bool do_restart = restart_scheduler->notifyConflict(constraint_type);
      if (do_restart) {
        restart();
        constraint_database->notifyRestart();
      }
    }
  }
}

bool QCDCL_solver::enqueue(Literal l, CRef reason) {
  Variable v = var(l);
  if (variable_data_store->isAssigned(v)) {
    return (variable_data_store->assignment(v) == sign(l));
  } else {
    //BOOST_LOG_TRIVIAL(trace) << "Enqueue literal" << (reason == CRef_Undef ? "(decision)": "") << ": " << (sign(l) ? "" : "-") << var(l);
    variable_data_store->appendToTrail(l, reason);
    propagator->notifyAssigned(l);
    decision_heuristic->notifyAssigned(l);
    dependency_manager->notifyAssigned(v);
    solver_statistics.nr_assignments++;
    return true;
  }
}

void QCDCL_solver::undoLast() {
  Literal l = variable_data_store->popFromTrail();
  //propagator->notifyUnassigned(l); // Not needed if we use watched literals.
  decision_heuristic->notifyUnassigned(l);
  //dependency_manager->notifyUnassigned(l); // Not needed if we use watched variables.
}

void QCDCL_solver::backtrackBefore(uint32_t target_decision_level) {
  solver_statistics.backtracks_total++;
  BOOST_LOG_TRIVIAL(trace) << "Backtracking before decision level: " << target_decision_level;
  propagator->notifyBacktrack(target_decision_level);
  decision_heuristic->notifyBacktrack(target_decision_level); // Target decision level must be passed to the VMTF decision heuristic.
  while (!variable_data_store->trailIsEmpty() && variable_data_store->decisionLevel() >= target_decision_level) {
    undoLast();
  }
}

void QCDCL_solver::restart() {
  backtrackBefore(0);
}

uint64_t QCDCL_solver::computeNrTrivial() {
  uint64_t nr_variables_of_type[2] = {0, 0};
  for (Variable v = 1; v <= variable_data_store->lastVariable(); v++) {
    nr_variables_of_type[variable_data_store->varType(v)]++;
  }
  return nr_variables_of_type[false] * nr_variables_of_type[true];
}

}