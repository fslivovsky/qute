#include "constraint_DB.hh"
#include "debug_helper.hh"
#include "decision_heuristic.hh"
#include "dependency_manager_watched.hh"
#include "logging.hh"
#include "propagator.hh"
#include "qcdcl.hh"
#include "restart_scheduler.hh"
#include "standard_learning_engine.hh"
#include "tracer.hh"
#include "variable_data.hh"
#include "watched_literal_propagator.hh"

#include "external_propagator.hh"

#include <algorithm>
#include <iostream>
#include <string>

namespace Qute {

const string QCDCL_solver::string_result[3] = {"UNSAT", "SAT", "UNDEF"};

using std::string;

string QCDCL_solver::externalize(Literal lit) const {
  string out_string = (sign(lit) ? "" : "-");
  out_string += variable_data_store->originalName(var(lit));
  return out_string;
}

string QCDCL_solver::externalize(const vector<Literal> &lits) const {
  string out_string;
  for (Literal lit : lits) {
    out_string += externalize(lit);
    out_string += " ";
  }
  out_string += "0";
  return out_string;
}

string QCDCL_solver::externalize(const Constraint &lits) const {
  string out_string;
  for (Literal lit : lits) {
    out_string += externalize(lit);
    out_string += " ";
  }
  out_string += "0";
  return out_string;
}

QCDCL_solver::QCDCL_solver(double time_limit): variable_data_store(nullptr), constraint_database(nullptr), propagator(nullptr), decision_heuristic(nullptr), dependency_manager(nullptr), restart_scheduler(nullptr), learning_engine(nullptr), debug_helper(nullptr), time_limit(time_limit), result(l_Undef), filename(""), interrupt_flag(false) {
  t_birth = clock();
}

QCDCL_solver::~QCDCL_solver() {}

/* variable_type is one of 'a', 'e', and 'f' (universal, existential, free; free is treated
 * as existential for the purpose of solving, with the extra meaning that for enumeration
 * only assignments to free variables are considered. If there are no free variables,
 * winning assignments to all variables of the first block are enumerated.
 * 
 * the var_type equals the constraint_type for which that var_type is primary
 */
void QCDCL_solver::addVariable(string original_name, char variable_type, bool auxiliary) {
  bool var_type = (variable_type == 'a'); // universal variables have var_type true
  // TODO clean this up
  variable_data_store->addVariable(original_name, var_type);
  if (variable_type == 'f') {
    variable_data_store->to_enumerate.push_back(variable_data_store->lastVariable());
  }
  if (auxiliary) {
    variable_data_store->auxiliary.push_back(variable_data_store->lastVariable());
  } else {
    variable_data_store->ordinary.push_back(variable_data_store->lastVariable());
  }
  propagator->addVariable();
  decision_heuristic->addVariable(auxiliary);
  dependency_manager->addVariable(auxiliary, var_type);
}

CRef QCDCL_solver::addConstraint(vector<Literal>& literals, ConstraintType constraint_type) {
  sort(literals.begin(), literals.end());
  literals.erase(unique(literals.begin(), literals.end()), literals.end());
  CRef constraint_reference = constraint_database->addConstraint(literals, constraint_type, false, false);
  propagator->addConstraint(constraint_reference, constraint_type);
  return constraint_reference;
}

vector<Literal> QCDCL_solver::blockingConstraint() {
  vector<Literal> blocking_constraint;
  if (!variable_data_store->to_enumerate.empty()) {
    for (Variable v : variable_data_store->to_enumerate) {
      blocking_constraint.push_back(mkLiteral(v, !learning_engine->reducedLast(v)));
    }
  } else {
    bool first_type = variable_data_store->varType(1);
    for (Variable v = 1; v <= variable_data_store->lastVariable() && variable_data_store->varType(v) == first_type; v++) {
      blocking_constraint.push_back(mkLiteral(v, !learning_engine->reducedLast(v)));
    }
  }
  return blocking_constraint;
}

// TODO requires first to backtrack enough so that the added constraint is not in conflict
// would be better if this was taken care of here
void QCDCL_solver::addConstraintDuringSearch(std::vector<Literal>& literals, ConstraintType constraint_type, int id) {
  // adding a clause may invalidate existing terms, but deleting constraints
  // may be problematic when variables are assigned, so we restart
  ConstraintType opp = (ConstraintType) (1 - constraint_type);
  restart();
  CRef cref;

  cref = addConstraint(literals, constraint_type);
  if (options.trace) {
	  tracer->traceConstraint(constraint_database->getConstraint(cref, constraint_type), constraint_type, std::vector<uint32_t>());
  }

  // the original circuit-output term (the last of input terms) will have to be deleted,
  // because it will be replaced by a new output term
  CRef output_term_ref = *(constraint_database->constraintReferencesEnd(opp, false) - 1);
  Constraint& output_term = constraint_database->getConstraint(output_term_ref, opp);
  vector<Literal> orig_output(output_term.size()+1);
  for (size_t i = 0; i < output_term.size(); i++) {
    orig_output[i] = output_term[i];
  }
  
  /* TODO not all tainted constraints must necessarily be deleted
   * since we can always add fresh_pos (see below) to every tainted derivation
   * and thereby make it valid, and since we can always replace fresh_pos
   * by any literal of the clause in one Q-consensus step with a binary term,
   * if a derived term already contains a literal of the clause, then it is
   * still derivable by Q-consensus, and thus valid (but remains tainted)
   */
  constraint_database->cleanTaintedConstraints(opp, literals);

  // encode the clause into DNF
  //addVariable("sym" + std::to_string(id), 'a', true);
  addVariable("", 'a', true); // the empty name "" means that a name should be created automatically
  Variable fresh = variable_data_store->lastVariable();
  Literal fresh_neg = mkLiteral(fresh, false);
  Literal fresh_pos = mkLiteral(fresh, true);
  for (Literal l : literals) {
    addDependency(fresh, var(l));
    vector<Literal> binary = {fresh_neg, l};
    cref = addConstraint(binary, opp);
	  if (options.trace) {
		  tracer->traceConstraint(constraint_database->getConstraint(cref, opp), opp, std::vector<uint32_t>());
	  }
  }
  vector<Literal> top_level(literals.size()+1, fresh_pos);
  for (size_t i = 1; i < top_level.size(); i++) {
    top_level[i] = ~literals[i-1];
  }
  cref = addConstraint(top_level, opp);
  if (options.trace) {
	  tracer->traceConstraint(constraint_database->getConstraint(cref, opp), opp, std::vector<uint32_t>());
  }

  // DNF-conjoin the original output with the added clause
  //vector<Literal> new_output_term = {orig_output, fresh_pos};
  orig_output[orig_output.size()-1] = fresh_pos;
  cref = addConstraint(orig_output, opp);
  if (options.trace) {
	  tracer->traceConstraint(constraint_database->getConstraint(cref, opp), opp, std::vector<uint32_t>());
  }

  // tainted constraints will become invalid after the next addition of a clause
  constraint_database->taintLastConstraint(opp);
}

void QCDCL_solver::addDependency(Variable of, Variable on) {
  if (variable_data_store->varType(of) != variable_data_store->varType(on)) {
    dependency_manager->addDependency(of, on);
  }
}

void QCDCL_solver::notifyMaxVarDeclaration(Variable max_var) {}

void QCDCL_solver::notifyNumClausesDeclaration(uint32_t max_var) {}

Literal QCDCL_solver::getUnitLiteralAfterBacktrack(vector<Literal> &clause) {
  uint32_t highest_dl = 0;
  uint32_t second_highest_dl = 0;
  Literal unit_literal = Literal_Undef;
  for (Literal l : clause) {
    Variable v = var(l);
    uint32_t this_dl = variable_data_store->isAssigned(v) ? variable_data_store->varDecisionLevel(v) : UINT32_MAX;
    if (this_dl == highest_dl) {
      if (unit_literal != Literal_Undef) {
        unit_literal = Literal_Error;
      }
    }
    else if (this_dl > highest_dl) {
      second_highest_dl = highest_dl;
      highest_dl = this_dl;
      unit_literal = ~l;
    } else if (this_dl > second_highest_dl) {
      second_highest_dl = this_dl;
    }
  }
  uint32_t decision_level_backtrack_before = second_highest_dl == highest_dl ?
    highest_dl : second_highest_dl + 1;
  if (decision_level_backtrack_before < variable_data_store->decisionLevel()) {
    backtrackBefore(decision_level_backtrack_before);
  }
  return unit_literal;
}

lbool QCDCL_solver::solve() {
  t_solve_begin = clock();
  constraint_database->notifyStart();
  dependency_manager->notifyStart();
  decision_heuristic->notifyStart();
  if (options.trace) {
    tracer->notifyStart();
  }

  size_t num_solutions = 0;
  // --------- SMS specific startup ------------
  // if we are going to enumerate, or we are going to use an external propagator,
  // we need to make sure, constraints are tainted as necessary
  // this means, the output constraint should be tainted in the dual encoding
  // for model generation, we just need to taint every initial model, that happens elsewhere
  constraint_database->taintLastConstraint(ConstraintType::clauses);
  constraint_database->taintLastConstraint(ConstraintType::terms);
  // --------- SMS specific startup --- END -----

  while (true) {
    clock_t now = clock();
    if (interrupt_flag || ((double)now - t_solve_begin) / CLOCKS_PER_SEC > time_limit) {
      t_solve_end = now;
      result = l_Undef;
      return l_Undef;
    }
    ConstraintType constraint_type;
    CRef conflict_constraint_reference = propagator->propagate(constraint_type);
    if (conflict_constraint_reference == CRef_Undef) {
      if (ext_prop && !ext_prop->checkAssignment()) {
        /* there is a problem with adding clauses: learned terms become invalidated
         * in initial-model mode, any terms that do not satisfy a newly added clause must be deleted
         * in dual-encoding mode, any terms that are derived using the top-level output term are
         *   tainted, and must be deleted, unless they satisfy the newly added clause
         * this is taken care of in addConstraintDuringSearch
         */
        continue;
      }
      Literal l = decision_heuristic->getDecisionLiteral();
      enqueue(l, CRef_Undef);
      solver_statistics.nr_decisions++;
    } else {
      decision_heuristic->notifyConflict(constraint_type);
      uint32_t decision_level_backtrack_before;
      Literal unit_literal;
      vector<Literal> literal_vector; // Represents a learned constraint or a set of new dependencies to be learned.
      bool constraint_learned;
      vector<Literal> conflict_side_literals;
      vector<uint32_t> premises;
      bool result_is_tainted = false;
      /* with out-of-order decisions we can learn "pseudo-asserting" (pseudo-unit after backtracking) constraints
       * these are non-disabled constraints with a single unassigned primary literal, but such that there is an
       * unassigned blocked secondary that prevents propagation. These constraints prevent branching on the primary
       * variable, but do not enforce a value.
       */
      bool is_learned_constraint_unit = learning_engine->analyzeConflict(conflict_constraint_reference, constraint_type, literal_vector, decision_level_backtrack_before, unit_literal, constraint_learned, conflict_side_literals, premises, result_is_tainted);
      if (constraint_learned) {
        solver_statistics.learned_total[constraint_type]++;
        if (literal_vector.empty()) {
          // empty learned constraint: the formula is solved
          if (options.trace) {
            tracer->traceConstraint(literal_vector, constraint_type, premises);
          }
            /* TODO in order to implement enumeration with model generation:
             *
             * if true and solving CNF
             *   learn a clause that negates lastReduced()
             *   delete all terms
             * if false and solving CNF
             *   add fresh def X := and(univ win move)
             *   augment every original clause with (... OR X)
             *     meaning that repeating (any of) the previous moves leads to instant loss for univ
             *   delete all learned clauses (alternative: add ... OR X to every learned clause)
             *   can keep all terms
             */
          //bool isFullyDefined = true;
          if (constraint_type != variable_data_store->varType(1)) {
            if (ext_prop && !ext_prop->checkSolution()) {
              continue;
            }
            ++num_solutions;
            if (!options.trace) {
              cout << "sol " << num_solutions << std::endl;
              cout << "v " << learning_engine->reducedLast() << std::endl;
            }
            if (enumerate) {
              // for solution enumeration, block the solution here
              vector<Literal> blocking_constraint = blockingConstraint();
              addConstraintDuringSearch(blocking_constraint, (ConstraintType) (1-constraint_type), num_solutions);
              continue;
            }
          }
          t_solve_end = clock();
          if (!options.trace) {
            if (enumerate) {
              std::cout << "Found " << num_solutions << " solution" << (num_solutions == 1 ? "" : "s") << std::endl;
            }
            if (ext_prop) {
              ext_prop->printStats();
            }
          }
          result = lbool(constraint_type);
          return result;
        } else {
          CRef learned_constraint_reference = constraint_database->addConstraint(literal_vector, constraint_type, true, result_is_tainted);
          auto& learned_constraint = constraint_database->getConstraint(learned_constraint_reference, constraint_type);
          if (options.trace) {
            tracer->traceConstraint(learned_constraint, constraint_type, premises);
          }
          decision_heuristic->notifyLearned(learned_constraint, constraint_type, conflict_side_literals);
          backtrackBefore(decision_level_backtrack_before);
          if (is_learned_constraint_unit) {
            assert(debug_helper->isUnit(learned_constraint, constraint_type));
            enqueue(unit_literal ^ constraint_type, learned_constraint_reference);
            solver_statistics.learned_asserting[constraint_type]++;
          } else {
            assert(!debug_helper->isUnit(learned_constraint, constraint_type));
          }
          propagator->addConstraint(learned_constraint_reference, constraint_type);
          restart_scheduler->notifyLearned(learned_constraint);
          assert(is_learned_constraint_unit || !dependency_manager->isEligibleOOO(var(unit_literal)));
        }
      } else {
        solver_statistics.backtracks_dep++;
        Variable unit_variable = var(unit_literal);
        dependency_manager->learnDependencies(unit_variable, literal_vector);
        auto decision_level_backtrack_before = variable_data_store->varDecisionLevel(unit_variable);
        backtrackBefore(decision_level_backtrack_before);
      }
      constraint_database->notifyConflict(constraint_type);
      restart_scheduler->notifyConflict(constraint_type);
      if (restart_scheduler->restart()) {
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
    //LOG(trace) << "Enqueue literal" << (reason == CRef_Undef ? "(decision)": "") << ": " << (sign(l) ? "" : "-") << var(l) << std::endl;
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
  LOG(trace) << "Backtracking before decision level: " << target_decision_level << std::endl;
  // WARNING: for out of order decisions to work properly, the following two notifications must be performed in this order
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
