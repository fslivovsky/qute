#include <boost/log/trivial.hpp>
#include "standard_learning_engine.hh"

namespace Qute {

StandardLearningEngine::StandardLearningEngine(QCDCL_solver& solver): solver(solver) {}

void StandardLearningEngine::analyzeConflict(CRef conflict_constraint_reference, ConstraintType constraint_type, vector<Literal>& literal_vector, uint32_t& decision_level_backtrack_before, Literal& unit_literal, bool& constraint_learned) {
  Constraint& constraint = solver.constraint_database->getConstraint(conflict_constraint_reference, constraint_type);
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
      return;
    }
    CRef reason_reference = solver.variable_data_store->varReason(var(primary_assigned_last));
    assert(reason_reference != CRef_Undef);
    Constraint& reason = solver.constraint_database->getConstraint(reason_reference, constraint_type);
    if (reason.learnt) {
      solver.constraint_database->updateLBD(reason);
      solver.constraint_database->bumpConstraintActivity(reason, constraint_type);
    }
    // Update "characteristic_function" to represent to resolvent (reduced). Also update "primary_literal_decision_level_counts".
    //BOOST_LOG_TRIVIAL(trace) << "Resolving: " << cfToString(characteristic_function, rightmost_primary) << " and " << solver.variable_data_store->constraintToString(reason) << " on " << (sign(primary_assigned_last) ? "" : "-") << var(primary_assigned_last);
    resolveAndReduce(characteristic_function, reason, constraint_type, primary_assigned_last, rightmost_primary, primary_literal_decision_level_counts, literal_vector);
    assert(getPrimaryLiteralDecisionLevelCounts(characteristic_function, rightmost_primary, constraint_type) == primary_literal_decision_level_counts);
    if (!literal_vector.empty()) {
      // Illegal merge. Set return values and exit.
      unit_literal = primary_assigned_last;
      constraint_learned = false;
      return;
    }
  }
  // The constraint represented by "characteristic_function" is empty, return.
  constraint_learned = true;
}

vector<bool> StandardLearningEngine::constraintToCf(Constraint& constraint, ConstraintType constraint_type, Literal& rightmost_primary) {
  vector<bool> characteristic_function(solver.variable_data_store->lastVariable() + solver.variable_data_store->lastVariable() + 2);
  fill(characteristic_function.begin(), characteristic_function.end(), false);
  for (Literal l: constraint) {
    if (solver.variable_data_store->varType(var(l)) == constraint_type && rightmost_primary < l) {
      rightmost_primary = l;
    }
  }
  for (Literal l: constraint) {
    characteristic_function[toInt(l)] = l <= rightmost_primary;
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
  primary_literal_decision_level_counts[solver.variable_data_store->varDecisionLevel(var(pivot))]--;
  vector<Literal> secondary_literals_reason;
  for (Literal l: reason) {
    if (var(l) == var(pivot)) {
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
      if (characteristic_function[toInt(~l)] && l < pivot) {
        // Illegal merge. Add to vector of dependencies to be learned.
        literal_vector.push_back(l);
      } else {
        secondary_literals_reason.push_back(l);
      }
    }
  }
  if (literal_vector.empty()) {
    // No illegal merges occurred.
    if (rightmost_primary == pivot) {
      reduced_last.clear();
      // Look for new rightmost primary literal, reduce secondaries along the way.
      rightmost_primary = Literal_Undef;
      for (unsigned i = toInt(pivot); i >= Min_Literal_Int; i--) {
        Variable v = i >> 1;
        if (characteristic_function[i] && solver.variable_data_store->varType(v) == constraint_type) {
          rightmost_primary = toLiteral(i);
          break;
        } else if (characteristic_function[i]) {
          characteristic_function[i] = false;
          reduced_last.push_back(toLiteral(i));
        }
      }
    }
    for (Literal l: secondary_literals_reason) {
      characteristic_function[toInt(l)] = l < rightmost_primary;
    }
  }
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
    Variable v;
    bool first_type =  solver.variable_data_store->varType(1);
    for (v = 1; v <= solver.variable_data_store->lastVariable() && solver.variable_data_store->varType(v) == first_type; v++);
    for (Literal l: reduced_last) {
      if (var(l) < v) {
        out_string += (sign(l) ? "" : "-");
        out_string += solver.variable_data_store->originalName(var(l));
        out_string += " ";
      }
    }
  }
  return out_string;
}

}