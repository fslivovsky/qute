#include "logging.hh"
#include "standard_learning_engine.hh"
#include "constraint_DB.hh"
#include "variable_data.hh"
#include "dependency_manager_watched.hh"
#include "qcdcl.hh"

namespace Qute {

StandardLearningEngine::StandardLearningEngine(QCDCL_solver& solver, string rrs_mode): solver(solver) {
  use_rrs_for_qtype[0] = (rrs_mode != "off");
  use_rrs_for_qtype[1] = (rrs_mode == "both");
}

/* with the possibility of out-of-order decisions, it is not guaranteed that we learn a unit constraint
 * we might also learn a "pseudo-unit" constraint: one whose "primary sub-constraint" (think existential sub-clause)
 * is unit propositionally, but which is unconstrained on secondaries (other than not being disabled)
 */
bool StandardLearningEngine::analyzeConflict(CRef conflict_constraint_reference, ConstraintType constraint_type, vector<Literal>& literal_vector, uint32_t& decision_level_backtrack_before, Literal& unit_literal, bool& constraint_learned, vector<Literal>& conflict_side_literals, vector<uint32_t>& premises) {
  Constraint& constraint = solver.constraint_database->getConstraint(conflict_constraint_reference, constraint_type);
  if (solver.options.trace) {
    premises.push_back(constraint.id());
  }
  if (constraint.learnt) {
    solver.constraint_database->updateLBD(constraint);
    solver.constraint_database->bumpConstraintActivity(constraint, constraint_type);
  }
  Literal rightmost_primary = Literal_Undef;
  vector<bool> characteristic_function = constraintToCf(constraint, constraint_type, rightmost_primary);

  vector<uint32_t> primary_literal_decision_level_counts = getPrimaryLiteralDecisionLevelCounts(constraint, constraint_type);
  assert(getPrimaryLiteralDecisionLevelCounts(characteristic_function, rightmost_primary, constraint_type) == primary_literal_decision_level_counts);
  vector<Literal> primary_trail;

  for (TrailIterator it = solver.variable_data_store->trailBegin(); it != solver.variable_data_store->trailEnd(); ++it) {
    if (solver.variable_data_store->varType(var(*it)) == constraint_type) {
      primary_trail.push_back(*it);
    }
  }

  while (rightmost_primary != Literal_Undef) { // Proceed as long as the constraint represented by "characteristic_function" is not empty.
    Literal primary_assigned_last;
    do {
      assert(!primary_trail.empty());
      primary_assigned_last = ~(primary_trail.back() ^ constraint_type);
      primary_trail.pop_back();
    } while (!characteristic_function[toInt(primary_assigned_last)]);
    // Check whether the result is asserting w.r.t. primary_assigned_last.
    if (isAsserting(primary_assigned_last, characteristic_function, primary_literal_decision_level_counts, constraint_type)) {
      // Constraint is asserting. Set return values and exit.
      if (cfToLiteralVector(characteristic_function, literal_vector, rightmost_primary)) {
        solver.solver_statistics.learned_tautological[constraint_type]++;
      }
      unit_literal = primary_assigned_last;
      constraint_learned = true;
      decision_level_backtrack_before = computeBackTrackLevel(primary_assigned_last, characteristic_function, rightmost_primary, constraint_type) + 1;
      // return value indicates that the constraint is unit
      return true;
    }
    CRef reason_reference = solver.variable_data_store->varReason(var(primary_assigned_last));
    if (reason_reference == CRef_Undef) {
      /* we cannot resolve further, but we haven't learned an asserting constraint
       * this is due to a decision on primary_assigned_last performed out of dependency order
       * nevertheless, we learn this constraint, which should make primary_assigned_last ineligible
       * for assignment. This kind of constraint is called pseudo-asserting.
       * 
       * In a correct implementation, a pseudo-asserting constraint is not contained in the
       * constraint database yet, so the solver is guaranteed to make progress. Optional TODO:
       * check that the learned constraint is indeed new (check that at least one resolution
       * step was used to derive it, with the caveat that when learning from generated initial
       * terms, no resolutions need necessarily be performed, and so that case has to be
       * distinguished), for both debugging purposes, as well as for the sake of an alternative
       * lazy implementation, which wouldn't keep track of OOO eligibility information, but
       * would instead check here that the decision was sound (and if it wasn't, the solver
       * would have to recover appropriately, taking care to avoid an infinite loop).
       */
      if (cfToLiteralVector(characteristic_function, literal_vector, rightmost_primary)) {
        solver.solver_statistics.learned_tautological[constraint_type]++;
      }
      unit_literal = primary_assigned_last;
      constraint_learned = true;
      decision_level_backtrack_before = computeBackTrackLevelPseudoUnit(primary_assigned_last, characteristic_function, rightmost_primary, constraint_type) + 1;
      // return value indicates that the constraint is *not* unit
      return false;
    }
    Constraint& reason = solver.constraint_database->getConstraint(reason_reference, constraint_type);
    if (reason.learnt) {
      solver.constraint_database->updateLBD(reason);
      solver.constraint_database->bumpConstraintActivity(reason, constraint_type);
    }
    if (solver.options.trace) {
      premises.push_back(reason.id());
    }
    // Update "characteristic_function" to represent to resolvent (reduced). Also update "primary_literal_decision_level_counts".
    //LOG(trace) << "Resolving: " << cfToString(characteristic_function, rightmost_primary) << " and " << solver.variable_data_store->constraintToString(reason) << " on " << (sign(primary_assigned_last) ? "" : "-") << var(primary_assigned_last) << std::endl;
    resolveAndReduce(characteristic_function, reason, constraint_type, primary_assigned_last, rightmost_primary, primary_literal_decision_level_counts, literal_vector);
    assert(getPrimaryLiteralDecisionLevelCounts(characteristic_function, rightmost_primary, constraint_type) == primary_literal_decision_level_counts);
    conflict_side_literals.push_back(primary_assigned_last);
    if (!literal_vector.empty()) {
      // Illegal merge. Set return values and exit.
      unit_literal = primary_assigned_last;
      constraint_learned = false;
      return false;
    }
  }
  // The constraint represented by "characteristic_function" is empty, return.
  constraint_learned = true;
  // return value immaterial here : the constraint is not unit, but we also don't care anymore
  return false;
}

vector<bool> StandardLearningEngine::constraintToCf(Constraint& constraint, ConstraintType constraint_type, Literal& rightmost_primary) {
  vector<bool> characteristic_function(solver.variable_data_store->lastVariable() + solver.variable_data_store->lastVariable() + 2);
  fill(characteristic_function.begin(), characteristic_function.end(), false);
  reduced_last.resize(solver.variable_data_store->lastVariable() + 1);
  for (Literal l: constraint) {
    if (solver.variable_data_store->varType(var(l)) == constraint_type && rightmost_primary < l) {
      rightmost_primary = l;
    }
  }
  for (Literal l: constraint) {
    characteristic_function[toInt(l)] = l <= rightmost_primary;
    reduced_last[var(l)] = sign(l);
  }
  return characteristic_function;
}

vector<uint32_t> StandardLearningEngine::getPrimaryLiteralDecisionLevelCounts(vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type) {
  vector<uint32_t> primary_decision_level_counts(solver.variable_data_store->decisionLevel() + 1);
  fill(primary_decision_level_counts.begin(), primary_decision_level_counts.end(), 0);
  vector<Literal> literal_vector;
  cfToLiteralVector(characteristic_function, literal_vector, rightmost_primary);
  for (Literal l: literal_vector) {
    if (solver.variable_data_store->varType(var(l)) == constraint_type) {
      assert(solver.variable_data_store->isAssigned(var(l)));
      primary_decision_level_counts[solver.variable_data_store->varDecisionLevel(var(l))]++;
    }
  }
  return primary_decision_level_counts;
}

vector<uint32_t> StandardLearningEngine::getPrimaryLiteralDecisionLevelCounts(Constraint& constraint, ConstraintType constraint_type) {
  vector<uint32_t> primary_decision_level_counts(solver.variable_data_store->decisionLevel() + 1);
  fill(primary_decision_level_counts.begin(), primary_decision_level_counts.end(), 0);
  for (Literal l: constraint) {
    if (solver.variable_data_store->varType(var(l)) == constraint_type) {
      assert(solver.variable_data_store->isAssigned(var(l)));
      primary_decision_level_counts[solver.variable_data_store->varDecisionLevel(var(l))]++;
    }
  }
  return primary_decision_level_counts;
}

bool StandardLearningEngine::isPseudoAsserting(Literal last_literal, vector<bool>& characteristic_function, vector<uint32_t>& primary_literal_decision_level_counts, ConstraintType constraint_type) {
  uint32_t decision_level_last_literal = solver.variable_data_store->varDecisionLevel(var(last_literal));
  if (decision_level_last_literal == 0 || primary_literal_decision_level_counts[decision_level_last_literal] > 1 || solver.variable_data_store->decisionLevelType(decision_level_last_literal) != constraint_type) {
    return false;
  }
  return true;
}


bool StandardLearningEngine::isAsserting(Literal last_literal, vector<bool>& characteristic_function, vector<uint32_t>& primary_literal_decision_level_counts, ConstraintType constraint_type) {
  uint32_t decision_level_last_literal = solver.variable_data_store->varDecisionLevel(var(last_literal));
  if (decision_level_last_literal == 0 || primary_literal_decision_level_counts[decision_level_last_literal] > 1 || solver.variable_data_store->decisionLevelType(decision_level_last_literal) != constraint_type) {
    return false;
  }
  for (int i = Min_Literal_Int; i < toInt(last_literal); i++) { // Every secondary var(last_literal) depends is to the left of var(last_literal) in the prefix.
    Variable v = i >> 1;
    if (characteristic_function[i] && solver.variable_data_store->varType(v) != constraint_type && solver.dependency_manager->dependsOn(var(last_literal), v) && 
        (!solver.variable_data_store->isAssigned(v) || solver.variable_data_store->varDecisionLevel(v) >= decision_level_last_literal)) {
      return false;
    }
  }
  return true;
}

void StandardLearningEngine::resolveAndReduce(vector<bool>& characteristic_function, Constraint& reason, ConstraintType constraint_type, Literal pivot, Literal& rightmost_primary, vector<uint32_t>& primary_literal_decision_level_counts, vector<Literal>& literal_vector) {
  characteristic_function[toInt(pivot)] = false;
  Variable pivot_var = var(pivot);
  primary_literal_decision_level_counts[solver.variable_data_store->varDecisionLevel(pivot_var)]--;
  vector<Literal> secondary_literals_reason;
  for (Literal l: reason) {
    if (var(l) == pivot_var) {
      continue;
    }
    if (solver.variable_data_store->varType(var(l)) == constraint_type) {
      // Primary literal.
      if (!characteristic_function[toInt(l)]) {
        characteristic_function[toInt(l)] = true;
        if (rightmost_primary < l) {
          rightmost_primary = l;
        }
        primary_literal_decision_level_counts[solver.variable_data_store->varDecisionLevel(var(l))]++;
      }
    } else {
      // Secondary literal.
      secondary_literals_reason.push_back(l);
      if (characteristic_function[toInt(~l)] && l < pivot) {
        // Illegal merge. Add to vector of dependencies to be learned.
        literal_vector.push_back(l);
      }
	}
  }
  /* filter out provably independent clashing variables using the reflexive
   * resolution-path dependency scheme */
  if (!literal_vector.empty() && use_rrs_for_qtype[solver.variable_data_store->varType(pivot_var)]) {
	  solver.dependency_manager->filterIndependentVariables(pivot_var, literal_vector);
  }
  if (literal_vector.empty()) {
    // No illegal merges occurred.
    if (rightmost_primary == pivot) {
      // Look for new rightmost primary literal, reduce secondaries along the way.
      rightmost_primary = Literal_Undef;
      for (unsigned i = toInt(pivot); i >= Min_Literal_Int; i--) {
        Variable v = i >> 1;
        if (characteristic_function[i] && solver.variable_data_store->varType(v) == constraint_type) {
          rightmost_primary = toLiteral(i);
          break;
        } else if (characteristic_function[i]) {
          characteristic_function[i] = false;
          reduced_last[v] = sign(toLiteral(i));
        }
      }
    }
    for (Literal l: secondary_literals_reason) {
      if (l < rightmost_primary) {
        characteristic_function[toInt(l)] = true;
      } else {
        characteristic_function[toInt(l)] = false;
        reduced_last[var(l)] = sign(l);
      }
    }

    // additionally, apply Drrs reduction
    solver.dependency_manager->reduceWithRRS(characteristic_function, rightmost_primary, constraint_type);
  }
}

/* computes the minimum possible decision level, such that undoing all greater decision levels will still guarantee that
 * literal will be pseudo-unit (the only unassigned primary)
 * the existence of such a decision level is assumed from the calling context
 * (the constraint should not be disabled and it should have a unique primary with highest decision level)
 *
 * TODO: I don't see why this should use the characteristic function insead of the literal vector, which is already available in
 * the calling context
 */
uint32_t StandardLearningEngine::computeBackTrackLevelPseudoUnit(Literal literal, vector<bool>& characteristic_function, Literal rightmost_primary, ConstraintType constraint_type) {
  uint32_t backtrack_level = 0;
  for (int i = Min_Literal_Int; i <= toInt(rightmost_primary); i++) {
    Variable v = i >> 1;
    if (characteristic_function[i] &&
        toLiteral(i) != literal &&
        solver.variable_data_store->isAssigned(v) &&
        solver.variable_data_store->varDecisionLevel(v) > backtrack_level &&
        solver.variable_data_store->varType(v) == constraint_type) {
      backtrack_level = solver.variable_data_store->varDecisionLevel(v);
    }
  }
  return backtrack_level;
}

uint32_t StandardLearningEngine::computeBackTrackLevel(Literal literal, vector<bool>& characteristic_function, Literal rightmost_primary, ConstraintType constraint_type) {
  uint32_t backtrack_level = 0;
  for (int i = Min_Literal_Int; i <= toInt(rightmost_primary); i++) {
    Variable v = i >> 1;
    if (characteristic_function[i] && toLiteral(i) != literal && solver.variable_data_store->isAssigned(v) && solver.variable_data_store->varDecisionLevel(v) > backtrack_level &&
        (solver.variable_data_store->varType(v) == constraint_type || solver.dependency_manager->dependsOn(var(literal), v))) {
      backtrack_level = solver.variable_data_store->varDecisionLevel(v);
    }
  }
  return backtrack_level;
}

string StandardLearningEngine::cfToString(vector<bool>& characteristic_function, Literal rightmost_primary) const {
  vector<Literal> literal_vector;
  cfToLiteralVector(characteristic_function, literal_vector, rightmost_primary);
  return solver.variable_data_store->literalVectorToString(literal_vector);
}

string StandardLearningEngine::reducedLast() {
  string out_string;
  if (solver.variable_data_store->lastVariable() > 0) {
    bool first_type =  solver.variable_data_store->varType(1);
    for (Variable v = 1; v <= solver.variable_data_store->lastVariable() && solver.variable_data_store->varType(v) == first_type; v++) {
      out_string += ((reduced_last[v] != first_type) ? "" : "-");
      out_string += solver.variable_data_store->originalName(v);
      out_string += " ";
    }
  }
  out_string += "0";
  return out_string;
}

}
