#include "model_generator_weighted.hh"
#include "solver_types.hh"
#include "qcdcl.hh"
#include "variable_data.hh"
#include "constraint_DB.hh"
#include "watched_literal_propagator.hh"
#include "minisat/mtl/Heap.h"
#include <cmath>

using std::vector;

namespace Qute {

ModelGeneratorWeighted::ModelGeneratorWeighted(QCDCL_solver& solver, double exponent, double scaling_factor, double universal_penalty) : ModelGenerator(solver), exponent(exponent), scaling_factor(scaling_factor), universal_penalty(universal_penalty), variable_weights(solver.variable_data_store->lastUniversal()+1) {
  /* Weights are always only assigned up to the last universal, because
   * the final existential block, if there is any, has weight 0.
   */
  variable_weights[0] = 1.0; // dummy value at index 0 in order to be able to index by variable name
  /* In weighted mode, the following model is used to distribute weights.
   * By w(v) we denote the weight of a variable v, by c(v), we denote
   * an auxiliary quantity called the cost. Furthermore, for an existential v,
   * let Q(v) be the total number of universal variables to the right of v,
   * and for a universal v, let Q(v) be the total number of existential variables
   * to the left of v, and let E be the total number of existential variables, and
   * U be the total number of universal variables.
   * Let the real parameters to this function be denoted by e, s, and p
   * respectively.
   *
   * c(v) = Q(v)/E if v is universal, and Q(v)/U if v is existential
   * w(v) = 1 + (c(v)^e)*s + p if v is universal
   * w(v) = 1 + (c(v)^e)*s     if v is existential
   *
   * In order to set all weights to 1 (and obtain what was previously called
   * 'heuristic' mode), set s=0 and p=0, i.e. run with
   *   --scaling-factor=0
   *   --universal-penalty=0
   */
  uint32_t variables_seen[2] = {1, 0};
  /* As we iterate over the variables, we will keep track
   * how many of each type we have seen to determine the
   * costs. As the cost of an existential is based on the
   * number of universals to the right of it, we will push
   * negative numbers to the cost, and at the end add the
   * total number of universals to the cost of every existential.
   *
   * We offset the number of existential variables seen by 1,
   * because if there is only one universal block (except for the
   * final existential one), we would have division by 0.
   */
  vector<uint32_t> costs;
  costs.push_back(1);
  /* TODO: looks like this code counts the number of variables of
   * opposite quantifier type to the left for every variables,
   * this may be computed elsewhere (variable_data_store, dep_man?) already */
  for (Variable v = 1; v <= solver.variable_data_store->lastUniversal(); v++) {
    uint32_t qtype = solver.variable_data_store->varType(v);
    uint32_t multiplier = qtype*2 - 1;
    costs.push_back(variables_seen[1-qtype]*multiplier);
    variables_seen[qtype]++;
  }
  for (Variable v = 1; v <= solver.variable_data_store->lastUniversal(); v++) {
    uint32_t qtype = solver.variable_data_store->varType(v);
    if(qtype == 0)
      costs[v] += variables_seen[1];
  }
  for (Variable v = 1; v <= solver.variable_data_store->lastUniversal(); v++) {
    bool qtype = solver.variable_data_store->varType(v);
    double cost = ((double) costs[v]) / variables_seen[1-qtype];
    double penalty = (qtype == 1) ? universal_penalty : 0.0;
    variable_weights[v] = scaling_factor*std::pow(cost, exponent) + 1 + penalty;
  }
}

vector<Literal> ModelGeneratorWeighted::generateModel() {
  /* Create an initial term from a satisfying assignment
   * using the greedy hitting-set approximation algorithm,
   * which keeps taking the literal that satisfies the
   * most unsatisfied clauses at every step. This
   * algorithm is guaranteed to produce a hitting set
   * at most lg(n) times larger than the optimal one.
   *
   * Furter possible optimization in the case of QBF is automatically
   * adding innermost existential variables to the initial term.
   * These variables can be reduced from any term, so they are never
   * actually added to the term, but clauses covered by them are
   * considered as already hit. Also, if a clause is satisfied by a
   * single literal, that literal is immediately added to the model
   * and clauses satisfied by it are removed.
   */
  vector<Literal> model;

  // build a vector of sets of clauses such that occurences[var] is
  // the set that contains all clauses in which the literal of var
  // which is currently set to true occurs. The inner data structure
  // must be a set, because we need fast iteration and removal by value.
  vector<std::unordered_set<CRef>> occurrences(solver.variable_data_store->lastUniversal()+1);

  vector<bool> is_in_model(solver.variable_data_store->lastVariable() + 1);
  fill(is_in_model.begin(), is_in_model.end(), false);
  vector<Variable> temp_true_variables_of_clause;
  uint32_t num_clauses = 0;
  for (vector<CRef>::const_iterator it = solver.constraint_database->constraintReferencesBegin(ConstraintType::clauses, false);
      it != solver.constraint_database->constraintReferencesEnd(ConstraintType::clauses, false);
      it++) {
    Constraint& clause = solver.constraint_database->getConstraint(*it, ConstraintType::clauses);
    num_clauses++;
    bool already_covered = false;
    for (Literal lit : clause) {
      Variable litvar = var(lit);
      if (solver.propagator->disablesConstraint(lit, ConstraintType::clauses)) {
        if (is_in_model[litvar]) {
          already_covered = true;
          break;
        } else if (litvar > solver.variable_data_store->lastUniversal()) {
          is_in_model[litvar] = true;
          model.push_back(mkLiteral(litvar, solver.variable_data_store->assignment(litvar)));
          already_covered = true;
          break;
        }
        temp_true_variables_of_clause.push_back(litvar);
      }
    }

    // if a clause isn't satisfied by an innermost existential literal and
    // it contains only one literal set to true, that literal
    // will have to be in the model anyway
    if (!already_covered && temp_true_variables_of_clause.size() == 1) {
      is_in_model[temp_true_variables_of_clause[0]] = true;
      model.push_back(mkLiteral(temp_true_variables_of_clause[0], solver.variable_data_store->assignment(temp_true_variables_of_clause[0])));
      already_covered = true;
    }

    if (!already_covered) {
      for (Variable current_var : temp_true_variables_of_clause)
        occurrences[current_var].insert(*it);
    }

    temp_true_variables_of_clause.clear();
  }

  // we already possibly added some literals to the model, so we need to
  // delete all clauses that are covered by them
  for (Literal mlit : model) {
    Variable mvar = var(mlit);
    // we only have occurrences for non-innermost-existentials
    if (mvar > solver.variable_data_store->lastUniversal()) {
      continue;
    }
    for (CRef cref : occurrences[mvar]) {
      Constraint& clause = solver.constraint_database->getConstraint(cref, ConstraintType::clauses);
      for (Literal lit : clause) {
        Variable litvar = var(lit);
        if (litvar != mvar && litvar <= solver.variable_data_store->lastUniversal() && solver.propagator->disablesConstraint(lit, ConstraintType::clauses)) {
          occurrences[litvar].erase(cref);
        }
      }
    }
    occurrences[mvar].clear();
  }

  Minisat::Heap<Variable, CompVarsByOccAndWeight> h(CompVarsByOccAndWeight(occurrences, variable_weights));
  for (Variable current_var = 1; current_var <= solver.variable_data_store->lastUniversal(); current_var++) {
    if (occurrences[current_var].size() > 0) {
      h.insert(current_var);
    }
  }

  while (!h.empty()) {
    Variable current_var = h.removeMin();
    if (occurrences[current_var].size() == 0) {
      // h now contains only variables whose every containing clause has been covered, hence we're done
      break;
    }
    model.push_back(mkLiteral(current_var, solver.variable_data_store->assignment(current_var)));

    for (CRef cref : occurrences[current_var]) {
      Constraint& clause = solver.constraint_database->getConstraint(cref, ConstraintType::clauses);
      for (Literal lit : clause) {
        Variable litvar = var(lit);
        if (litvar != current_var && solver.propagator->disablesConstraint(lit, ConstraintType::clauses)) {
          occurrences[litvar].erase(cref);
          // h is natively a min-heap, so increase means
          // 'get further away from the optimum'; in our
          // case the more occurrences, the better, so
          // deleting occurrences is increasing in h.
          h.increase(litvar);
        }
      }
    }
  }

  update_solver_statistics(model);

  return model;

  // TODO: consider further improving the model by randomly removing literals and redoing the calculation
  //
  //    or by restricting to models whose rightmost universal literal is at most the rightmost universal
  //       literal in the current model. In other words, add all existentials to the current model, so
  //       that the existential reduction still derives the same thing, and then see what can be removed
  //       to obtain a possibly better model.
  
}

}
