#include "three_watched_literal_propagator.hh"
#include "decision_heuristic.hh"
#include "logging.hh"
#include "debug_helper.hh"
#include "variable_data.hh"
#include "model_generator.hh"
#include "constraint_DB.hh"
#include "tracer.hh"
#include "dependency_manager_watched.hh"
#include "qcdcl.hh"

namespace Qute {

inline bool ThreeWatchedLiteralPropagator::isPrimary(Literal literal, ConstraintType constraint_type) {
	return solver.variable_data_store->varType(var(literal)) == constraint_type;
}

inline bool ThreeWatchedLiteralPropagator::isAssigned(Literal literal) {
   return solver.variable_data_store->isAssigned(var(literal));
}

inline bool ThreeWatchedLiteralPropagator::dependsOn(Literal who_depends_on_other, Literal who_is_depended_on) {
	return solver.dependency_manager->dependsOn(var(who_depends_on_other), var(who_is_depended_on));
}

inline uint32_t ThreeWatchedLiteralPropagator::decLevel(Literal l) {
  return solver.variable_data_store->varDecisionLevel(var(l));
}

inline bool ThreeWatchedLiteralPropagator::phaseAdvice(Variable v) {
  bool var_type = solver.variable_data_store->varType(v);
  if (!var_type) {
    return constraints_watched_by[ConstraintType::clauses][toInt(mkLiteral(v, false))].size() <
           constraints_watched_by[ConstraintType::clauses][toInt(mkLiteral(v, true))].size();
  } else {
    return constraints_watched_by[ConstraintType::terms][toInt(mkLiteral(v, true))].size() <
           constraints_watched_by[ConstraintType::terms][toInt(mkLiteral(v, false))].size();
  }
}

ThreeWatchedLiteralPropagator::ThreeWatchedLiteralPropagator(QCDCL_solver& solver):
  solver(solver),
  constraints_watched_by{
    vector<vector<WatchedRecord>>(2),
    vector<vector<WatchedRecord>>(2)
  } {}

/*
 * rearranges the constraint so that primary literals appear before secondary literals
 */

void ThreeWatchedLiteralPropagator::primariesToFront(Constraint& constraint, ConstraintType constraint_type) {
  int lo = 0;
  int hi = constraint.size()-1;
  do {
    while (lo < hi &&  isPrimary(constraint[lo], constraint_type)) ++lo;
    while (lo < hi && !isPrimary(constraint[hi], constraint_type)) --hi;
    if (lo < hi) {
      constraint.swapLits(lo, hi);
      ++lo;
      --hi;
    }
  } while (lo < hi) ;
}
CRef ThreeWatchedLiteralPropagator::propagate(ConstraintType& constraint_type) {
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
  // WARNING: the propagation_queue actually operates as a stack!
  while (!propagation_queue.empty()) {
    Literal to_propagate = propagation_queue.back();
    propagation_queue.pop_back();
    //LOG(trace) << "Propagating literal: " << (sign(to_propagate) ? "" : "-") << var(to_propagate) << std::endl;
    for (ConstraintType _constraint_type: constraint_types) {
      Literal watcher = ~(to_propagate ^ _constraint_type);
      vector<WatchedRecord>& record_vector = constraints_watched_by[_constraint_type][toInt(watcher)];
      vector<WatchedRecord>::iterator i, j;
      for (i = j = record_vector.begin(); i != record_vector.end(); ++i) {
        ++solver.solver_statistics.watched_list_accesses;
        WatchedRecord& record = *i;
        CRef constraint_reference = record.constraint_reference;
        Literal blocker = record.blocker;
        bool watcher_changed = false;
        if (!disablesConstraint(blocker, _constraint_type)) {
          Constraint& constraint = solver.constraint_database->getConstraint(constraint_reference, _constraint_type);
          if (constraintIsWatchedByLiteral(constraint, _constraint_type, watcher)) { // if we want to allow for removal, add "&& !constraint.isMarked()"
            assert(!isPrimary(watcher, _constraint_type) || constraint[0] == watcher || constraint[1] == watcher);
            assert( isPrimary(watcher, _constraint_type) || constraint.last() == watcher);
            if (!enforceWatcherConsistency(constraint, constraint_reference, _constraint_type, watcher, watcher_changed)) {
              // Constraint is empty: clean up, return constraint_reference.
              // TODO: this performs record_vector.end()-i moves
              // but this can be done in min(record_vector.end()-i, i-j) steps by taking elements from the back until
              // either exhausted or the gap is closed (at the expense of shuffling the order, but that should not matter)
              /*auto t = record_vector.end();
              while (j < i && i < t) {
                *j++ = *--t;
              }
              if (j < i) {
                t = j;
              }
              record_vector.resize(t - record_vector.begin(), WatchedRecord(CRef_Undef, Literal_Undef));*/
              for (; i != record_vector.end(); i++, j++) {
                *j = *i;
              }
              record_vector.resize(j - record_vector.begin(), WatchedRecord(CRef_Undef, Literal_Undef));
              constraint_type = _constraint_type;
              return constraint_reference;
            }/* else if (watcher_changed && isPrimary(watcher, _constraint_type) && constraint[1] == watcher) {
              //TODO what's up with this?
              watcher_changed = false;
            }*/
          } else {
            ++solver.solver_statistics.spurious_watch_events;
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
    vector<Literal> initial_term = solver.model_generator->generateModel();
    CRef initial_term_reference = solver.constraint_database->addConstraint(initial_term, ConstraintType::terms, true);
    auto& term = solver.constraint_database->getConstraint(initial_term_reference, ConstraintType::terms);
    term.mark(); // Immediately mark for removal upon constraint cleaning.
    if (solver.options.trace) {
      solver.tracer->traceConstraint(term, ConstraintType::terms);
    }
    assert(solver.debug_helper->isEmpty(term, ConstraintType::terms));
    constraint_type = ConstraintType::terms;
    return initial_term_reference;
  } else {
    return CRef_Undef;
  }
}

void ThreeWatchedLiteralPropagator::addConstraint(CRef constraint_reference, ConstraintType constraint_type) {
  Constraint& constraint = solver.constraint_database->getConstraint(constraint_reference, constraint_type);
  primariesToFront(constraint, constraint_type);
  int num_watchers = initWatchers(constraint, constraint_reference, constraint_type);
  if (num_watchers < 2) {
    constraints_without_two_watchers[constraint_type].push_back(constraint_reference);
  }
}

void ThreeWatchedLiteralPropagator::relocConstraintReferences(ConstraintType constraint_type) {
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

// TODO: this will probably need to be updated as well. Can probably be lumped together with initWatchers for addConstraint if that is necessary
bool ThreeWatchedLiteralPropagator::propagateUnwatched(CRef constraint_reference, ConstraintType constraint_type, bool& watchers_found) {
  Constraint& constraint = solver.constraint_database->getConstraint(constraint_reference, constraint_type);
  if (constraint.size() == 0 || !isPrimary(constraint[0], constraint_type)){
    /* even if the constraint is disabled, it can be reduced to an empty constraint
     * fingers crossed that this doesn't blow up somewhere else
     */
    return false;
  }
  // constraints with more than one primary literal shouldn't ever appear here
  assert(constraint.size() == 1 || !isPrimary(constraint[1], constraint_type));
  size_t watcher = constraint.size();
  uint32_t max_declevel = 0;
  for (size_t i = constraint.size()-1; i > 0; --i) {
    if (dependsOn(constraint[0], constraint[i])) {
      if (isUnassignedOrDisablesConstraint(constraint[i], constraint_type)) {
        watcher = i;
        break;
      } else {
        if (decLevel(constraint[i]) >= max_declevel) {
          watcher = i; 
          max_declevel = decLevel(constraint[i]);
        }
      } 
    }
  }
  if (watcher < constraint.size()) {
    // secondary watcher found
    constraint.swapToEnd(watcher);
    watchConstraint(constraint_type, constraint_reference, constraint[0], constraint.last());
    watchConstraint(constraint_type, constraint_reference, constraint.last(), constraint[0]);
    watchers_found = true;
    return true;
  } else {
    return solver.enqueue(disablingLiteral(constraint[0], constraint_type), constraint_reference);
  }
}

size_t ThreeWatchedLiteralPropagator::findPrimaryWatcher(Constraint& constraint, ConstraintType constraint_type, size_t begin) {
	while (begin < constraint.size() && isPrimary(constraint[begin], constraint_type)) {
		if (!isAssigned(constraint[begin])) {
			break;
		}
    ++begin;
	}
  assert (
      begin == constraint.size() ||
      !isPrimary(constraint[begin], constraint_type) ||
      !isAssigned(constraint[begin])
      );
	return begin;
}

/*
 * searches for a dependency of constraint[0] backwards
 * the reason to go backwards is that we expect a previously found watcher dependency to already be at the end (TODO re-evaluate whether this is so)
 */
size_t ThreeWatchedLiteralPropagator::findSecondaryWatcher(Constraint& constraint, ConstraintType constraint_type, size_t first_secondary_index) {
	// WARNING first_secondary_index MUST BE POSITIVE, otherwise this loop is infinite
	// it is assumed from previous computation that we know that the literals between first_secondary_index and end are all secondaries
	size_t watcher = constraint.size();
	Literal primary_watcher = constraint[0];
	for (size_t i = constraint.size()-1; i >= first_secondary_index; --i) {
		//if (!isAssigned(constraint[i]) && dependsOn(constraint[0], constraint[i])) {
		if (isBlockedSecondary(constraint[i], constraint_type, primary_watcher)) {
      watcher = i;
			break;
		}
	}
	return watcher;
}

/*
 * initializes watchers for constraint so that
 *   if possible, two primaries watch the constraint
 *   else the only primary and a blocking secondary watch, the blocking secondary goes last
 *   else the constraint is unit or empty
 * returns true if watchers are found successfully (in which case the watched lists are taken care of)
 *   otherwise false
 */
int ThreeWatchedLiteralPropagator::initWatchers(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type) {
  // first attempt to collect primary watchers
  size_t i = 0;
  size_t j = 0;
  while (i < 2 && j < constraint.size() && isPrimary(constraint[j], constraint_type)) {
    if (disablesConstraint(constraint[j], constraint_type) || !isAssigned(constraint[j])) {
      constraint.swapLits(i, j);
      ++i;
    }
    ++j;
  }

  if (i == 0) {
    // no un assigned or disabling primaries, this probably cannot really happen unless we have a fully universal clause in the input
    return 0;
  }

  if (i == 2) {
    // constraint is stable, push onto watched lists
    watchConstraint(constraint_type, constraint_reference, constraint[0], constraint[1]);
    watchConstraint(constraint_type, constraint_reference, constraint[1], constraint[0]);
    return 2;
  }

  assert (i == 1);

  /* constraint is unstable
   *   we must find the primary assigned last, because that will be the second primary watcher
   *   then we must also find a secondary watcher, or a previous secondary watcher
   *
   *   also, var(constraint[0]) should not be a decision variable starting at the next highest
   *   primary decision level in the constraint, or at 0 if there is no second primary
   */

  uint32_t highest_primary_decision_level = 0;
  size_t highest_primary_index = 0;
  for (; i < j; ++i) {
    if (
        highest_primary_decision_level == 0 || 
        decLevel(constraint[i]) > highest_primary_decision_level
       ) {
      highest_primary_decision_level = decLevel(constraint[i]);
      highest_primary_index = i;
    }
  }

  /* now comes the mind-boggling part.
   * If
   *   constraint[0] is the only primary or
   *                 is unassigned       or
   *                 is assigned at a higher decision level than constraint[1]
   * then a secondary watcher might be needed. We need a secondary with the properties
   *   constraint[0] depends on it and
   *
   *   if constraint[0] is the only primary
   *     and is unassigned, then
   *       if an unassigned or disabling secondary blocker exists, pick it, else
   *       if an assigned secondary blocker exists, pick the one with max decision level
   *         (should be equal to the current decision level, no idea what happens if not)
   *          THIS IS A UNIT CASE
   *       else the constraint doesn't have two watchers
   *     else if it is assigned (disabling)
   *       if an unassigned or disabling secondary blocker exists, pick it, else
   *       if an assigned secondary blocker exists, pick the one with max decision level
   *         (should be equal to the decision level of constraint[0], no idea what happens if not)
   *          THIS IS A UNIT CASE
   *       else the constraint doesn't have two watchers
   *    else
   *      if unassigned, then
   *        if an unassigned or disabling secondary blocker exists, pick it, else
   *        if an assigned secondary blocker exists, pick the one with max decision level
   *          IF THIS DECISION LEVEL IS HIGHER THAN THAT OF constraint[1]
   *          (should be equal to the current decision level, no idea what happens if not)
   *          THIS IS A UNIT CASE
   *        else the constraint is good to go with two primary watchers
   *      else
   *        if an unassigned or disabling secondary blocker exists, pick it, else
   *        if an assigned secondary blocker exists, pick the one with max decision level
   *          IF THIS DECISION LEVEL IS HIGHER THAN THAT OF constraint[1]
   *          (should be equal to the current decision level, no idea what happens if not)
   *          THIS IS A UNIT CASE
   *        else the constraint is good to go with two primary watchers
   *        
   * the idea is that if there is a decision level at which the clause might become pseudo-unit, then
   * we should find a blocker that will be unassigned at that level. If this is not possible, then
   * the clause must be unit at decision level 0
   *   
   */

  // this looks for an unassigned dependency of constraint[0] (always preferable)
  // TODO: merge the code that looks for an unassigned literal with the subsequent one
  size_t sec_watcher = findSecondaryWatcher(constraint, constraint_type, j);

  uint32_t highest_secondary_index = sec_watcher;
  size_t highest_secondary_decision_level = 0xFFFFFFFF; // unassigned == infinite decision level

  if (sec_watcher == constraint.size()) {
    for (; i < constraint.size(); ++i) {
      if (dependsOn(constraint[0], constraint[i])) {
        if (
            highest_secondary_index == constraint.size() || 
            decLevel(constraint[i]) > highest_secondary_decision_level
           ) {
          highest_secondary_decision_level = decLevel(constraint[i]);
          highest_secondary_index = i;
        }
      }
    }
  }

  if (highest_primary_index > 0) {
    // the constraint has two primaries
    constraint.swapLits(1, highest_primary_index);
    watchConstraint(constraint_type, constraint_reference, constraint[0], constraint[1]);
    watchConstraint(constraint_type, constraint_reference, constraint[1], constraint[0]);
    // var(constraint[0]) is unassignable since the time the last other primary was assigned
    solver.dependency_manager->setVariableAEL(var(constraint[0]), highest_primary_decision_level);
    if ((
         !isAssigned(constraint[0]) ||
         decLevel(constraint[0]) > decLevel(constraint[1])
         ) &&
        highest_secondary_decision_level > decLevel(constraint[1]))
      {
      constraint.swapToEnd(highest_secondary_index);
      watchConstraint(constraint_type, constraint_reference, constraint.last(), constraint[0]);
      return 3;
    } else {
      // the constraint is good to go with the two primary watchers
      return 2;
    }
  } else {
    // the constraint only has one primary
    solver.dependency_manager->markVarPermanentlyUnassignable(var(constraint[0]));
    // now let's find a secondary watcher
    if (highest_secondary_index < constraint.size()) {
      // the clause has a secondary watcher, it's pseudo-unit
      constraint.swapToEnd(highest_secondary_index);
      watchConstraint(constraint_type, constraint_reference, constraint[0], constraint.last());
      watchConstraint(constraint_type, constraint_reference, constraint.last(), constraint[0]);
      return 2;
    } else {
      // constraint does not have two watchers
      return 1;
    }
  }
}

/*
 * enforces the invariant that constraint has the correct watchers:
 *   if constraint is disabled do nothing (assumes that upon any backtracking that enables the constraint the invariant will be restored automatically)
 *     else
 * 	     if constraint has >= 2 unassigned primaries, then the first two literals are unassigned primaries and the constraint is watched by them
 * 	     	the constraint is "stable"
 * 	     if constraint has == 1 unassigned primary, then it goes to position 0 and the last literal is an unassigned blocking secondary
 * 	     	if possible, the constraint is "pseudo-unit"
 * 	        else the constraint is "unit" and propagates constraint[0]
 * 	     if constraint has == 0 unassigned primaries it is "empty" (falsified clause / satisfied cube)
 */
bool ThreeWatchedLiteralPropagator::enforceWatcherConsistency(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type, Literal violated_watcher, bool& watcher_changed) {
	watcher_changed = false;

	// TODO: try to avoid going through the whole clause unconditionally. Instead check for disablingness individually
  // BUT BE CAREFUL NOT TO PROPAGATE IF THE CONSTRAINT IS DISABLED
	if (isDisabled(constraint, constraint_type)) {
		return true;
	}

  /* two different things happen based on the type of event
   *
   * if last_assigned_literal is secondary
   *   if constraint is stable or not watched by lal, then
   *     nothing happens, drop the watch
   *   else
   *     update the secondary watch (resulting in a unit or pseudo-unit constraint)
   *
   * else
   *   if no unassigned primaries left
   *     return false
   *   swap an unassigned primary to position 0
   *   if no other unassigned primaries left
   *     find a secondary watch
   *     if not possible, then
   *       constraint is unit
   *     else
   *       mark var(constraint[0]) as non-assignable (for out-of-order QCDCL)
   *
   */

	size_t watcher_search_begin = 1;

	// constraint[0] is always a primary
	assert(isPrimary(constraint[0], constraint_type));

	if (isAssigned(constraint[0])) {
		watcher_search_begin = findPrimaryWatcher(constraint, constraint_type, watcher_search_begin);
		if (watcher_search_begin == constraint.size() ||
        !isPrimary(constraint[watcher_search_begin], constraint_type)) {
			// no unassigned primaries, constraint empty
			assert(solver.debug_helper->isEmpty(constraint, constraint_type));
#ifndef NDEBUG
      // assert that there is a primary violated by propagation on the highest decision level
      bool has_recently_propagated_primary = false;
      for (size_t i = 0; i < watcher_search_begin; ++i) {
        Variable v = var(constraint[i]);
        if (solver.variable_data_store->varReason(v) != CRef_Undef && solver.variable_data_store->varDecisionLevel(v) == solver.variable_data_store->decisionLevel()) {
          has_recently_propagated_primary = true;
          break;
        }
      }
      assert(has_recently_propagated_primary);
#endif
			return false;
		} else {
      constraint.swapLits(0, watcher_search_begin);
			if (watcher_search_begin > 1) {
        if (violated_watcher == constraint[1]) {
          /* if both primary watchers are violated and a new one is coming in, it must go to position 0,
           * but we are processing the watcher at position 1, so we need to make sure that we swap out 1
           * (if only one will get swapped out in the end). We swapped out 0, so now let's swap it back
           * to position 1
           */
          constraint.swapLits(1, watcher_search_begin);
        }
				// we swapped a non-watcher primary in, so push onto the watched list
        watchConstraint(constraint_type, constraint_reference, constraint[0], constraint[1]);
				watcher_changed = true;
			} else {
				// we just swapped positions of the two primary watchers, that's non-action on the high level
			}
		}
	}

	assert(!isAssigned(constraint[0]));
	assert(isPrimary(constraint[0], constraint_type));

  // attempt to find another watcher
  watcher_search_begin = findPrimaryWatcher(constraint, constraint_type, watcher_search_begin);

  assert(constraint.size() > 1); // size-1 constraints are always handled in initWatchers
	if (watcher_search_begin < constraint.size() && isPrimary(constraint[watcher_search_begin], constraint_type)) {
    /* we cannot have watcher_search_begin == 1 because
     * that means both constraint[0] and constraint[1] were unassigned primaries to begin with, and
     * that can only happen in a spurious secondary watch event, and
     * that should have been caught by isConstraintWatchedByLiteral()
     */
    assert(watcher_search_begin > 1);
    // constraint can be re-stabilized
    constraint.swapLits(1, watcher_search_begin);
    watchConstraint(constraint_type, constraint_reference, constraint[1], constraint[0]);
    watcher_changed = true;
    assert(isUnassignedPrimary(constraint[0], constraint_type));
    assert(isUnassignedPrimary(constraint[1], constraint_type));
  } else {
    // we're out of unassigned primaries, constraint is pseudo-unit or unit
    // determine which
    size_t secondary_watcher = findSecondaryWatcher(constraint, constraint_type, watcher_search_begin);
    solver.dependency_manager->markVarUnassignable(var(constraint[0]));
    if (secondary_watcher == constraint.size()) {
      // constraint is unit, propagate the primary at 0 and return
      return solver.enqueue(disablingLiteral(constraint[0], constraint_type), constraint_reference);
    } else {
      // constraint is pseudo-unit 
      if (violated_watcher == constraint.last()) {
        watcher_changed = true;
      } else {
      }
      constraint.swapToEnd(secondary_watcher);
      watchConstraint(constraint_type, constraint_reference, constraint.last(), constraint[0]);
      /* TODO at this point var(constraint[0]) becomes in-elligible for assignment
       * if it wasn't already, mark it so, and record that information on the parallel trail */
    }
  }

  return true;
}

bool ThreeWatchedLiteralPropagator::isUnassignedOrDisablingPrimary(Literal literal, ConstraintType constraint_type) {
  return isPrimary(literal, constraint_type) && (!isAssigned(literal) || disablesConstraint(literal, constraint_type));
}

inline bool ThreeWatchedLiteralPropagator::constraintIsWatchedByLiteral(Constraint& constraint, ConstraintType constraint_type, Literal watcher) {
  return constraint[0] == watcher || (constraint[1] == watcher && isPrimary(watcher, constraint_type)) || (
     constraint.last() == watcher &&
     dependsOn(constraint[0], constraint.last()) && (
        !isPrimary(constraint[1], constraint_type) ||
        isAssigned(constraint[1])
        )
     );
}

bool ThreeWatchedLiteralPropagator::disablesConstraint(Literal literal, ConstraintType constraint_type) {
  return isAssigned(literal) && (solver.variable_data_store->assignment(var(literal)) == sign(literal)) == disablingPolarity(constraint_type);
}

inline bool ThreeWatchedLiteralPropagator::isUnassignedOrDisablesConstraint(Literal literal, ConstraintType constraint_type) {
  return !isAssigned(literal) || (solver.variable_data_store->assignment(var(literal)) == sign(literal)) == disablingPolarity(constraint_type);
}

bool ThreeWatchedLiteralPropagator::isDisabled(Constraint& constraint, ConstraintType constraint_type) {
  for (unsigned i = 0; i < constraint.size(); i++) {
    if (disablesConstraint(constraint[i], constraint_type)) {
      return true;
    }
  }
  return false;
}

bool ThreeWatchedLiteralPropagator::isUnassignedPrimary(Literal literal, ConstraintType constraint_type) {
  return solver.variable_data_store->varType(var(literal)) == constraint_type && !solver.variable_data_store->isAssigned(var(literal));
}

bool ThreeWatchedLiteralPropagator::isBlockedSecondary(Literal literal, ConstraintType constraint_type, Literal primary) {
  return !solver.variable_data_store->isAssigned(var(literal)) && solver.dependency_manager->dependsOn(var(primary), var(literal));
}

bool ThreeWatchedLiteralPropagator::propagationCorrect() {
  for (ConstraintType constraint_type: constraint_types) {
    for (unsigned learnt = 0; learnt <= 1; learnt++) {
      for (vector<CRef>::const_iterator constraint_reference_it = solver.constraint_database->constraintReferencesBegin(constraint_type, static_cast<bool>(learnt));
           constraint_reference_it != solver.constraint_database->constraintReferencesEnd(constraint_type, static_cast<bool>(learnt));
           ++constraint_reference_it) {
        Constraint& constraint = solver.constraint_database->getConstraint(*constraint_reference_it, constraint_type);
        if (!constraint.isMarked() && (solver.debug_helper->isUnit(constraint, constraint_type) || solver.debug_helper->isEmpty(constraint, constraint_type))) {
          LOG(error) << (learnt ? "Learnt ": "Input ") << (constraint_type ? "term " : "clause ") << (solver.debug_helper->isEmpty(constraint, constraint_type) ? "empty" : "unit") << ": " << solver.variable_data_store->constraintToString(constraint) << std::endl;
          return false;
        }
      }
    }
  }
  return true;
}

inline void ThreeWatchedLiteralPropagator::watchConstraint(ConstraintType constraint_type, CRef constraint_reference, Literal watcher, Literal cowatcher) {
  assert(!isPrimary(watcher, constraint_type) ||
      watcher == solver.constraint_database->getConstraint(constraint_reference, constraint_type)[0] ||
      watcher == solver.constraint_database->getConstraint(constraint_reference, constraint_type)[1]
      );
  constraints_watched_by[constraint_type][toInt(watcher)].emplace_back(constraint_reference, cowatcher);
}

inline void ThreeWatchedLiteralPropagator::notifyBacktrack(uint32_t decision_level_before) {
  propagation_queue.clear();
  solver.dependency_manager->backtrackAET(decision_level_before);
}

}
