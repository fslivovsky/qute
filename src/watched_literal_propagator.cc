#include "watched_literal_propagator.hh"

namespace Qute {

WatchedLiteralPropagator::WatchedLiteralPropagator(QCDCL_solver& solver): solver(solver), constraints_watched_by{vector<vector<WatchedRecord>>(2), vector<vector<WatchedRecord>>(2)} {}

CRef WatchedLiteralPropagator::propagate(ConstraintType& constraint_type) {
  if (solver.variable_data_store->decisionLevel() == 0) {
    for (ConstraintType _constraint_type: constraint_types) {
      vector<CRef>::iterator i, j;
      for (i = j = constraints_without_two_watchers[_constraint_type].begin(); i != constraints_without_two_watchers[_constraint_type].end(); ++i) {
        bool watchers_found = false;
        if (!propagateUnwatched(*i, _constraint_type, watchers_found)) {
          CRef empty_constraint_reference = *i;
           // Constraint is empty: clean up, return constraint_reference.
          for (; i != constraints_without_two_watchers[_constraint_type].end(); i++, j++) {
            *j = *i;
          }
          constraints_without_two_watchers[_constraint_type].resize(j - constraints_without_two_watchers[_constraint_type].begin(), CRef_Undef);
          constraint_type = _constraint_type;
          return empty_constraint_reference;
        } else if (!watchers_found) {
          *j++ = *i;
        }
      }
      constraints_without_two_watchers[_constraint_type].resize(j - constraints_without_two_watchers[_constraint_type].begin(), CRef_Undef);
    }
  }
  while (!propagation_queue.empty()) {
    Literal to_propagate = propagation_queue.back();
    propagation_queue.pop_back();
    //BOOST_LOG_TRIVIAL(trace) << "Propagating literal: " << (sign(to_propagate) ? "" : "-") << var(to_propagate);
    for (ConstraintType _constraint_type: constraint_types) {
      Literal watcher = ~(to_propagate ^ _constraint_type);
      vector<WatchedRecord>& record_vector = constraints_watched_by[_constraint_type][toInt(watcher)];
      vector<WatchedRecord>::iterator i, j;
      for (i = j = record_vector.begin(); i != record_vector.end(); ++i) {
        WatchedRecord& record = *i;
        CRef constraint_reference = record.constraint_reference;
        Literal blocker = record.blocker;
        bool watcher_changed = false;
        if (!disablesConstraint(blocker, _constraint_type)) {
          Constraint& constraint = solver.constraint_database->getConstraint(constraint_reference, _constraint_type);
          if (constraintIsWatchedByLiteral(constraint, watcher)) { // if we want to allow for removal, add "&& !constraint.isMarked()"
            if (!updateWatchedLiterals(constraint, constraint_reference, _constraint_type, watcher_changed)) {
              // Constraint is empty: clean up, return constraint_reference.
              for (; i != record_vector.end(); i++, j++) {
                *j = *i;
              }
              record_vector.resize(j - record_vector.begin(), WatchedRecord(CRef_Undef, Literal_Undef));
              constraint_type = _constraint_type;
              return constraint_reference;
            }
          } else {
            watcher_changed = true;
          }
        }
        if (!watcher_changed) {
          *j++ = record;
        }
      }
      record_vector.resize(j - record_vector.begin(), WatchedRecord(CRef_Undef, Literal_Undef));
    }
  }
  assert(propagationCorrect());
  if (solver.variable_data_store->allAssigned()) { 
  /* Every variable is assigned but no conflict/solution is detected.
     Use the model generation rule to obtain an initial term. */
    vector<Literal> initial_term;
    vector<bool> characteristic_function(solver.variable_data_store->lastVariable() + solver.variable_data_store->lastVariable() + 2);
    fill(characteristic_function.begin(), characteristic_function.end(), false);
    for (vector<CRef>::const_iterator it = solver.constraint_database->constraintReferencesBegin(ConstraintType::clauses, false); 
         it != solver.constraint_database->constraintReferencesEnd(ConstraintType::clauses, false); 
         ++it) {
      CRef constraint_reference = *it;
      Constraint& input_clause = solver.constraint_database->getConstraint(constraint_reference, ConstraintType::clauses);
      for (Literal l: input_clause) {
        if (disablesConstraint(l, ConstraintType::clauses)) {
          characteristic_function[toInt(l)] = true;
          break;
        }
      }
    }
    for (unsigned i = 0; i < characteristic_function.size(); i++) {
      if (characteristic_function[i]) {
        initial_term.push_back(toLiteral(i));
      }
    }
    CRef initial_term_reference = solver.constraint_database->addConstraint(initial_term, ConstraintType::terms, true);
    solver.constraint_database->getConstraint(initial_term_reference, ConstraintType::terms).mark(); // Immediately mark for removal upon constraint cleaning.
    assert(isEmpty(solver.constraint_database->getConstraint(initial_term_reference, ConstraintType::terms), ConstraintType::terms));
    constraint_type = ConstraintType::terms;
    return initial_term_reference;
  } else {
    return CRef_Undef;
  }
}

void WatchedLiteralPropagator::addConstraint(CRef constraint_reference, ConstraintType constraint_type) {
  Constraint& constraint = solver.constraint_database->getConstraint(constraint_reference, constraint_type);
  uint32_t first_watcher_index = findFirstWatcher(constraint, constraint_type);
  if (first_watcher_index < constraint.size) {
    std::swap(constraint[0], constraint[first_watcher_index]);
  } else {
    constraints_without_two_watchers[constraint_type].push_back(constraint_reference);
    return;
  }
  uint32_t second_watcher_index = findSecondWatcher(constraint, constraint_type);
  if (second_watcher_index < constraint.size) {
    std::swap(constraint[1], constraint[second_watcher_index]);
  } else {
    constraints_without_two_watchers[constraint_type].push_back(constraint_reference);
    return;
  }
  constraints_watched_by[constraint_type][toInt(constraint[0])].emplace_back(constraint_reference, constraint[1]);
  constraints_watched_by[constraint_type][toInt(constraint[1])].emplace_back(constraint_reference, constraint[0]);
}

void WatchedLiteralPropagator::relocConstraintReferences(ConstraintType constraint_type) {
  for (unsigned literal_int = Min_Literal_Int; literal_int < constraints_watched_by[constraint_type].size(); literal_int++) {
    vector<WatchedRecord>& watched_records = constraints_watched_by[constraint_type][literal_int];
    vector<WatchedRecord>::iterator i, j;
    for (i = j = watched_records.begin(); i != watched_records.end(); ++i) {
      WatchedRecord& record = *i;
      Constraint& constraint = solver.constraint_database->getConstraint(record.constraint_reference, constraint_type);
      if (!constraint.isMarked()) {
        solver.constraint_database->relocate(record.constraint_reference, constraint_type);
        *j++ = *i;
      }
    }
    watched_records.resize(j - watched_records.begin(), WatchedRecord(CRef_Undef, Literal_Undef));
  }
  vector<CRef>::iterator i, j;
  for (i = j = constraints_without_two_watchers[constraint_type].begin(); i != constraints_without_two_watchers[constraint_type].end(); ++i) {
    Constraint& constraint = solver.constraint_database->getConstraint(*i, constraint_type); 
    if (!constraint.isMarked()) {
      solver.constraint_database->relocate(*i, constraint_type);
      *j++ = *i;
    }
  }
  constraints_without_two_watchers[constraint_type].resize(j - constraints_without_two_watchers[constraint_type].begin());
}

bool WatchedLiteralPropagator::propagateUnwatched(CRef constraint_reference, ConstraintType constraint_type, bool& watchers_found) {
  Constraint& constraint = solver.constraint_database->getConstraint(constraint_reference, constraint_type);
  if ((constraint.size == 0 || solver.variable_data_store->varType(var(constraint[0])) != constraint_type) && !isDisabled(constraint, constraint_type)) {
    //BOOST_LOG_TRIVIAL(trace) << (constraint_type ? "Term " : "Clause ") << "empty: " << solver.variable_data_store->constraintToString(constraint);
    assert(isEmpty(constraint, constraint_type));
    return false;
  } else if (!isDisabled(constraint, constraint_type)) { // First watcher is a primary literal and constraint is not disabled. 
    uint32_t second_watcher_index = findSecondWatcher(constraint, constraint_type);
    if (second_watcher_index < constraint.size) {
      std::swap(constraint[1], constraint[second_watcher_index]);
      watchers_found = true;
      constraints_watched_by[constraint_type][toInt(constraint[0])].emplace_back(constraint_reference, constraint[1]);
      constraints_watched_by[constraint_type][toInt(constraint[1])].emplace_back(constraint_reference, constraint[0]);
      return true;
    } else {
      assert(isEmpty(constraint, constraint_type) || isUnit(constraint, constraint_type));
      //BOOST_LOG_TRIVIAL(trace) << (constraint_type ? "Term " : "Clause ") << (isEmpty(constraint, constraint_type) ? "empty" : "unit") << ": " << solver.variable_data_store->constraintToString(constraint);
      return solver.enqueue(constraint[0] ^ constraint_type, constraint_reference);
    }
  }
  return true;
}

bool WatchedLiteralPropagator::updateWatchedLiterals(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type, bool& watcher_changed) {
  watcher_changed = false;
  if (isDisabled(constraint, constraint_type)) {
    return true;
  }
  /* If both watchers must be updated, it can happen that the first watcher can be updated, but not the second.
     In such a case we may end up with a primary in constraint[0] (which is set to the disabling polarity by propagation)
     and a secondary in constraint[1] that constraint[0] does not depend on. This would not constitute a valid pair
     of watchers after backtracking. To catch this case, we keep track of the index i of the old first watcher, in case
     it is updated. If we only fail to update the second watcher (constraint[1]), i == 1 and swapping constraint[i] with
     constraint[1] has no effect. */
  unsigned int i = 1; 
  if (solver.variable_data_store->isAssigned(var(constraint[0]))) {
    /* First watcher (constraint[0]) must be updated.
       If the second watcher (constraint[1]) is a primary that is assigned or a secondary,
       we _must_ find a new unassigned primary or a disabling literal unless the constraint is empty. */
    if (solver.variable_data_store->varType(var(constraint[1])) != constraint_type || solver.variable_data_store->isAssigned(var(constraint[1]))) {
      for (i = 2; i < constraint.size; i++) {
        if (isUnassignedPrimary(constraint[i], constraint_type)) {
          std::swap(constraint[0], constraint[i]);
          constraints_watched_by[constraint_type][toInt(constraint[0])].emplace_back(constraint_reference, constraint[1]);
          watcher_changed = true;
          break;
        }
      }
      if (!watcher_changed) {
        // Constraint is empty, see above.
        //BOOST_LOG_TRIVIAL(trace) << (constraint_type ? "Term " : "Clause ") << "empty: " << solver.variable_data_store->constraintToString(constraint);
        assert(isEmpty(constraint, constraint_type));
        return false;
      }
    } else {
      // If the second watcher is an unassigned primary literal, we swap the first and second watcher.
      std::swap(constraint[0], constraint[1]);
    }
  }
  // The second watcher (constraint[1]) must be updated, and the first watcher is an unassigned primary.
  for (unsigned i = 1; i < constraint.size; i++) {
    if (isUnassignedPrimary(constraint[i], constraint_type)) {
      assert(!isEmpty(constraint, constraint_type) && !isUnit(constraint, constraint_type));
      std::swap(constraint[1], constraint[i]);
      constraints_watched_by[constraint_type][toInt(constraint[1])].emplace_back(constraint_reference, constraint[0]);
      watcher_changed = true;
      return true;
    } else if (isBlockedSecondary(constraint[i], constraint_type, constraint[0])) {
      assert(!isEmpty(constraint, constraint_type) && !isUnit(constraint, constraint_type));
      // Make constraint[i] the second watcher.
      std::swap(constraint[1], constraint[i]);
      constraints_watched_by[constraint_type][toInt(constraint[1])].emplace_back(constraint_reference, constraint[0]);
      watcher_changed = true;
      return true;
    }
  }
  // No new watcher has been found. Try to propagate the first watcher.
  //BOOST_LOG_TRIVIAL(trace) << (constraint_type ? "Term " : "Clause ") << (isEmpty(constraint, constraint_type) ? "empty" : "unit") << ": " << solver.variable_data_store->constraintToString(constraint);
  assert(isEmpty(constraint, constraint_type) || isUnit(constraint, constraint_type));
  std::swap(constraint[1], constraint[i]); // See the comment towards the top.
  watcher_changed = false;
  return solver.enqueue(constraint[0] ^ constraint_type, constraint_reference);
}

bool WatchedLiteralPropagator::isUnit(Constraint& constraint, ConstraintType constraint_type) {
  Literal unassigned_primary = Literal_Undef;
  for (Literal l: constraint) {
    if (disablesConstraint(l, constraint_type)) {
      return false;
    }
    else if (isUnassignedPrimary(l, constraint_type)) {
      if (unassigned_primary == Literal_Undef) { 
        unassigned_primary = l;
      } else {
        return false;
      }
    }
  }
  if (unassigned_primary == Literal_Undef) {
    return false;
  } else {
    for (Literal l: constraint) {
      if (solver.variable_data_store->varType(var(l)) != constraint_type && !solver.variable_data_store->isAssigned(var(l)) && solver.dependency_manager->dependsOn(var(unassigned_primary), var(l))) {
        return false;
      }
    }
    return true;
  }
}

bool WatchedLiteralPropagator::isEmpty(Constraint& constraint, ConstraintType constraint_type) {
  for (Literal l: constraint) {
    if (disablesConstraint(l, constraint_type) || isUnassignedPrimary(l, constraint_type)) {
      return false;
    }
  }
  return true;
}

inline uint32_t WatchedLiteralPropagator::findFirstWatcher(Constraint& constraint, ConstraintType constraint_type) {
  uint32_t i;
  for (i = 0; i < constraint.size; i++) {
    if (isUnassignedOrDisablingPrimary(constraint[i], constraint_type)) {
      break;
    }
  }
  return i;
}

inline uint32_t WatchedLiteralPropagator::findSecondWatcher(Constraint& constraint, ConstraintType constraint_type) {
  uint32_t i;
  for (i = 1; i < constraint.size; i++) {
    if (isUnassignedOrDisablingPrimary(constraint[i], constraint_type) || isBlockedOrDisablingSecondary(constraint[i], constraint_type, constraint[0])) {
      break;
    }
  }
  if (i < constraint.size) {
    return i;
  } else {
    /* No other unassigned or disabling primary or blocked or disabling secondary was found.
       If there are any other assigned primaries or assigned secondaries the first watcher depends on,
       take the one with maximum decision level. */
    uint32_t index_with_highest_decision_level = constraint.size;
    for (i = 1; i < constraint.size; i++) {
      if ((solver.variable_data_store->varType(var(constraint[i])) == constraint_type ||
           solver.dependency_manager->dependsOn(var(constraint[0]), var(constraint[i]))) &&
          solver.variable_data_store->isAssigned(var(constraint[i])) &&
          (index_with_highest_decision_level == constraint.size ||
           solver.variable_data_store->varDecisionLevel(var(constraint[i])) > solver.variable_data_store->varDecisionLevel(var(constraint[index_with_highest_decision_level])))) {
        index_with_highest_decision_level = i;
      }
    }
    return index_with_highest_decision_level; 
  }
}

bool WatchedLiteralPropagator::isUnassignedOrDisablingPrimary(Literal literal, ConstraintType constraint_type) {
  return (solver.variable_data_store->varType(var(literal)) == constraint_type &&
          (!solver.variable_data_store->isAssigned(var(literal)) ||
           (solver.variable_data_store->assignment(var(literal)) == sign(literal)) == disablingPolarity(constraint_type)));
}

bool WatchedLiteralPropagator::isBlockedOrDisablingSecondary(Literal literal, ConstraintType constraint_type, Literal primary) {
  return ((solver.variable_data_store->varType(var(literal)) != constraint_type && solver.dependency_manager->dependsOn(var(primary), var(literal))) &&
          (!solver.variable_data_store->isAssigned(var(literal)) ||
           (solver.variable_data_store->assignment(var(literal)) == sign(literal)) == disablingPolarity(constraint_type) ||
           ((solver.variable_data_store->assignment(var(primary)) == sign(literal)) == disablingPolarity(constraint_type) &&
            solver.variable_data_store->varDecisionLevel(var(primary)) <= solver.variable_data_store->varDecisionLevel(var(literal)))));
}

bool WatchedLiteralPropagator::constraintIsWatchedByLiteral(Constraint& constraint, Literal l) {
  return (l == constraint[0]) || (l == constraint[1]);
}

bool WatchedLiteralPropagator::disablesConstraint(Literal literal, ConstraintType constraint_type) {
  return solver.variable_data_store->isAssigned(var(literal)) && (solver.variable_data_store->assignment(var(literal)) == sign(literal)) == disablingPolarity(constraint_type);
}

bool WatchedLiteralPropagator::isDisabled(Constraint& constraint, ConstraintType constraint_type) {
  for (unsigned i = 0; i < constraint.size; i++) {
    if (disablesConstraint(constraint[i], constraint_type)) {
      return true;
    }
  }
  return false;
}

bool WatchedLiteralPropagator::vanishes(Literal literal, ConstraintType constraint_type) {
  return solver.variable_data_store->isAssigned(var(literal)) && (solver.variable_data_store->assignment(var(literal)) == sign(literal)) != disablingPolarity(constraint_type);
}

bool WatchedLiteralPropagator::isUnassignedPrimary(Literal literal, ConstraintType constraint_type) {
  return solver.variable_data_store->varType(var(literal)) == constraint_type && !solver.variable_data_store->isAssigned(var(literal));
}

bool WatchedLiteralPropagator::isBlockedSecondary(Literal literal, ConstraintType constraint_type, Literal primary) {
  return !solver.variable_data_store->isAssigned(var(literal)) && solver.dependency_manager->dependsOn(var(primary), var(literal));
}

bool WatchedLiteralPropagator::propagationCorrect() {
  for (ConstraintType constraint_type: constraint_types) {
    for (unsigned learnt = 0; learnt <= 1; learnt++) {
      for (vector<CRef>::const_iterator constraint_reference_it = solver.constraint_database->constraintReferencesBegin(constraint_type, static_cast<bool>(learnt));
           constraint_reference_it != solver.constraint_database->constraintReferencesEnd(constraint_type, static_cast<bool>(learnt));
           ++constraint_reference_it) {
        Constraint& constraint = solver.constraint_database->getConstraint(*constraint_reference_it, constraint_type);
        if (!constraint.isMarked() && (isUnit(constraint, constraint_type) || isEmpty(constraint, constraint_type))) {
          BOOST_LOG_TRIVIAL(error) << (learnt ? "Learnt ": "Input ") << (constraint_type ? "term " : "clause ") << (isEmpty(constraint, constraint_type) ? "empty" : "unit") << ": " << solver.variable_data_store->constraintToString(constraint);
          return false;
        }
      }
    }
  }
  return true;
}

}