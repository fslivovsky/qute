#include "dependency_manager_rrs.hh"
#include "qcdcl.hh"
#include "logging.hh"
#include "variable_data.hh"
#include "constraint_DB.hh"
#include <queue>
#include <unordered_map>
#include <algorithm>

namespace Qute {

using std::priority_queue;
using std::unordered_map;

DependencyManagerRRS::DependencyManagerRRS(QCDCL_solver& solver, string dependency_learning_strategy, string out_of_order_decisions): DependencyManagerWatched(solver, dependency_learning_strategy, out_of_order_decisions) {
}

void DependencyManagerRRS::filterIndependentVariables(Variable unit_variable, vector<Literal>& literal_vector) {
  solver.solver_statistics.backtracks_dep++;
  if (!variable_dependencies[unit_variable - 1].independencies_known) {
    // only compute more independencies if there is time
    /*
	clock_t n = 3, c = 30;
	clock_t now = clock();
    if ((now - solver.birth_time) + n * c * CLOCKS_PER_SEC < n * solver.solver_statistics.time_spent_computing_RRS) {
      return;
    }*/
    getDepsRRS(unit_variable);
  }
  size_t j = 0;
  for (size_t i = 0; i < literal_vector.size(); ++i) {
    if (!notDependsOn(unit_variable, var(literal_vector[i]))) {
      literal_vector[j++] = literal_vector[i];
    }
  }
  if (j == 0) {
    ++solver.solver_statistics.dep_conflicts_resolved;
  }
  literal_vector.resize(j);
}

void DependencyManagerRRS::reduceWithRRS(vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type) {
    clock_t t = clock();
    vector<Variable> blockers;
    int bound = Min_Literal_Int;
    for (int i = toInt(rightmost_primary); i >= bound; i--) {
      if (characteristic_function[i]) {
        Variable v = i >> 1;
        if (solver.variable_data_store->varType(v) == constraint_type) {
          if (independenciesKnown(v) && numIndependencies(v) > 0) {
            blockers.push_back(v);
            int new_bound = leftmostIndependent(v) * 2;
            bound = bound < new_bound ? new_bound : bound;
          } else {
            break;
          }
        } else {
          bool can_be_reduced = true;
          for (Variable blocker : blockers) {
            // TODO: queries for a given blocker are in sorted order, can be optimized
            if (!notDependsOn(blocker, v)) {
              can_be_reduced = false;
              break;
            }
          }
          characteristic_function[i] = !can_be_reduced;
          solver.solver_statistics.nr_RRS_reduced_lits += can_be_reduced;
          if (i % 2 == 1) {
            --i;
            solver.solver_statistics.nr_RRS_reduced_lits += characteristic_function[i] && can_be_reduced;
            characteristic_function[i] = characteristic_function[i] && !can_be_reduced;
          }
        }
      }
    }
    solver.solver_statistics.time_spent_reducing_by_RRS += clock()-t;
}

void DependencyManagerRRS::getDepsRRS(Variable v) {
  clock_t t = clock();
  bool vqtype = solver.variable_data_store->varType(v);

  vector<bool> reachable_true = getReachable(mkLiteral(v, true));
  vector<bool> reachable_false = getReachable(mkLiteral(v, false));

  // TODO iterate over !vqtype variables only (using a next-of-type query?)
  for(int x = 2; x < v*2; x += 2) {
    Variable xvar = x / 2;
    if (solver.variable_data_store->varType(xvar) != vqtype) {
      if ((reachable_true[x] && reachable_false[x^1]) || (reachable_true[x^1] && reachable_false[x])) {
        
      }
      else {
        // independence detected
        addNonDependency(v, xvar);
      }
    }
  }
  variable_dependencies[v - 1].independencies_known = true;
  solver.solver_statistics.time_spent_computing_RRS += clock()-t;
}

vector<bool> DependencyManagerRRS::getReachable(Literal l) {
  /* TODO optimizations?
   *
   * * (?) short-circuit once it is found that everything in the 
   *   dependency conflict is a dependency
   * * (?) possible variant of the above: never run the second 'getReachable'
   *   if nothing from the dependency conflict is reachable the first time
   *   around (or in general, consider the results of the first run before
   *   the second one)
   */

  uint32_t num_vars = solver.variable_data_store->lastVariable();
  uint32_t num_lits = num_vars * 2;

  vector<bool> reachable(num_lits);
  reachable.assign(num_lits, false);

  Variable lvar = var(l);
  bool lqtype = solver.variable_data_store->varType(var(l));
  bool target_qtype = 1 - lqtype;

  uint32_t max_connections = 2*solver.variable_data_store->countVarsOfTypeLeftOf(!lqtype, lvar);

  /* max_min_path_depth:
   *
   * the minimum path depth over a currently explored resolution-path is
   * the leftmost connecting variable on that path (and infinity ~ num_vars+1
   * if the path has no connecting literals).
   * max_min_path_depth[x] stores the maximum minimum path depth over all
   * paths already visited --- it is always sufficient to only
   * explore a literal over it's "deepest" path, because the polarity jump
   * always lands in the same set of clauses, but allows a larger set of terminals.
   * a literal is never pushed onto the stack if it's already there, instead the
   * max_min_path_depth[...] is updated
   *
   * max_min_path_depth is initialized to minus infinity ~ 0
   */

  priority_queue<Literal> landing_literals({}, {l});

  vector<Variable> max_min_path_depth(num_lits+2, 0);
  max_min_path_depth[toInt(l)] = lvar;
  unordered_map<CRef, Literal> first_entry_literal;
  //first_entry_literal.reserve(solver.constraint_database->constraintReferencesBegin(ConstraintType::clauses, false) - solver.constraint_database->constraintReferencesBegin(ConstraintType::clauses, false));

  auto exploreLit = [this, &reachable, &max_min_path_depth, &landing_literals, target_qtype](Literal landing_lit, Literal next_lit) -> uint32_t {
    Variable next_litvar = var(next_lit);
    bool litvarqtype = solver.variable_data_store->varType(next_litvar);
    if (litvarqtype == 0) {
      int landing_idx = toInt(landing_lit);
      // the minimum depth of the current path if we extend it by lit
      Variable lit_candidate_depth = std::min(max_min_path_depth[landing_idx], next_litvar);
      // the number of new targets we can reach compared to the so far deepest path through lit
      int num_new_targets = solver.variable_data_store->countVarsOfTypeBetween(target_qtype, max_min_path_depth[toInt(~next_lit)], lit_candidate_depth);
      if (num_new_targets > 0) {
        if (max_min_path_depth[toInt(~next_lit)] == 0) {
          landing_literals.push(~next_lit);
        }
        max_min_path_depth[toInt(~next_lit)] = lit_candidate_depth;
      }
    }
    if (litvarqtype == target_qtype && next_litvar < max_min_path_depth[toInt(landing_lit)]) {
      /* lit is validly reached by the current path */
      if (!reachable[toInt(next_lit)]) {
        reachable[toInt(next_lit)] = true;
        return 1;
        /*
        ++connections_found;
        if (connections_found == max_connections) {
          return;
        }
        */
      }
    }
    return 0;
  };

  uint32_t connections_found = 0;
  while (!landing_literals.empty()) {
    Literal current_lit = landing_literals.top();
    landing_literals.pop();

    for (auto occit = solver.constraint_database->literalOccurrencesBegin(current_lit, ConstraintType::clauses);
          occit != solver.constraint_database->literalOccurrencesEnd(current_lit, ConstraintType::clauses);
          occit++) {
      auto fel_ref = first_entry_literal.find(*occit);
      if (fel_ref != first_entry_literal.end()) {
        // only check the first entry literal, clause was already visited
        // and it is guaranteed that previous entry path was deeper (proof?)
        connections_found += exploreLit(current_lit, fel_ref->second);
      }
      else {
        Constraint& clause = solver.constraint_database->getConstraint(*occit, ConstraintType::clauses);
        if (clause.size() > 8) {
          first_entry_literal[*occit] = current_lit;
        }
        for (Literal lit : clause) {
          if (lit != current_lit) {
            connections_found += exploreLit(current_lit, lit);
          }
        }
      }
      if (connections_found == max_connections) {
        // clear the priority queue and break out, to ensure that there is a
        // single return statement and we get RVO
        landing_literals = priority_queue<Literal>();
        break;
      }
    }
  }

  return reachable;
}

void DependencyManagerRRS::addNonDependency(Variable of, Variable on) {
  solver.solver_statistics.nr_independencies++;
  variable_dependencies[of - 1].independent_of.push_back(on);
}

}
