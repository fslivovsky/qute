#include "dependency_manager_watched.hh"
#include "logging.hh"
#include "qcdcl.hh"
#include "variable_data.hh"
#include "decision_heuristic.hh"

namespace Qute {

DependencyManagerWatched::DependencyManagerWatched(QCDCL_solver& solver, string dependency_learning_strategy): learnDependenciesPtr(nullptr), solver(solver), prefix_mode(false) {
  if (dependency_learning_strategy == "off") {
    prefix_mode = true;
  } else if (dependency_learning_strategy == "all") {
    learnDependenciesPtr = &DependencyManagerWatched::learnAllDependencies;
  } else if (dependency_learning_strategy == "outermost") {
    learnDependenciesPtr = &DependencyManagerWatched::learnOutermostDependency;
  } else if (dependency_learning_strategy == "fewest") {
    learnDependenciesPtr = &DependencyManagerWatched::learnDependencyWithFewestDependencies;
  }
}

void DependencyManagerWatched::learnAllDependencies(Variable unit_variable, vector<Literal>& literal_vector) {
  for (Literal l : literal_vector) {
    addDependency(unit_variable, var(l));
  }
}

void DependencyManagerWatched::learnOutermostDependency(Variable unit_variable, vector<Literal>& literal_vector) {
  assert(!literal_vector.empty());
  Variable outermost = solver.variable_data_store->lastVariable() + 1;
  for (Literal l : literal_vector) {
    if (outermost > var(l)) {
      outermost = var(l);
    }
  }
  addDependency(unit_variable, outermost);
}

void DependencyManagerWatched::learnDependencyWithFewestDependencies(Variable unit_variable, vector<Literal>& literal_vector) {
  uint32_t fewest_deps = solver.variable_data_store->lastVariable() + 1;
  Variable variable_with_fewest_deps = 0;
  for (Literal l : literal_vector) {
    Variable current_var = var(l);
    uint32_t current_deps = variable_dependencies[current_var - 1].dependent_on.size();
    if (current_deps < fewest_deps) {
       variable_with_fewest_deps = current_var;
       fewest_deps = current_deps;
    }
  }
  addDependency(unit_variable, variable_with_fewest_deps);
}

void DependencyManagerWatched::notifyAssigned(Variable v) {
  /* After "variable" is assigned, we must find new watchers for variables that
  were watched by "variable". */
  if (!is_auxiliary[v - 1]) {
    vector<Variable>::iterator begin, i, j, end;
    for (begin = i = j = variables_watched_by[v - 1].begin(), end = variables_watched_by[v - 1].end(); i != end; i++) {
      Variable watched = *i;
      assert(!is_auxiliary[watched - 1]);
      if (!findWatchedDependency(watched, false)) {
        /* If we cannot find a new variable to watch "watched", we keep
        "watched" on the list of variables watched by "variable". */
        variable_dependencies[watched - 1].watcher_index = j - begin;
        *j++ = watched;
          // Here, we may have to change the state of the VMTF decision heuristic.
        // Variable "watched" is now a decision candidate (unless it is already assigned).
        if (!solver.variable_data_store->isAssigned(watched)) {
          solver.decision_heuristic->notifyEligible(watched);
        }
      }
    }
    variables_watched_by[v - 1].resize(j - begin, 0);
  }
}

void DependencyManagerWatched::addDependency(Variable of, Variable on) {
  if (!dependsOn(of, on)) {
    LOG(trace) << "Dependency added: (" << of << ", " << on << ")" << std::endl;
    solver.solver_statistics.nr_dependencies++;
    variable_dependencies[of - 1].dependent_on.insert(on);
    variable_dependencies[of - 1].dependent_on_vector.push_back(on);
    /* If the current watched dependency is 0 or a variable that is assigned,
       make the newly added dependency the new watched dependency. */
    Variable current_watcher = variable_dependencies[of - 1].watcher;
    if (!is_auxiliary[of - 1] && (current_watcher == 0 || solver.variable_data_store->isAssigned(current_watcher))) {
      setWatchedDependency(of, on, current_watcher != 0);
    }
    assert(dependsOn(of, on));
  }
}

void DependencyManagerWatched::setWatchedDependency(Variable variable, Variable new_watched, bool remove_from_old) {
  /* If this method is called from "notifyAssigned", we don't want to 
  remove "variable" from the set of dependencies watched its current
  watcher, since the list of variables watched by the current watcher
  is cleared anyway. Hence the flag "remove_from_old". */
  if (variable_dependencies[variable - 1].watcher != 0 && remove_from_old) {
    uint32_t index = variable_dependencies[variable - 1].watcher_index;
    Variable current_watcher = variable_dependencies[variable - 1].watcher;
    // Delete "variable" from the list of variables watched by "current_watcher".
    variables_watched_by[current_watcher - 1][index] = variables_watched_by[current_watcher - 1].back();
    variables_watched_by[current_watcher - 1].pop_back();
    // Update watched list index of the variable that was moved from the back of the list.
    Variable moved_watched = variables_watched_by[current_watcher - 1][index];
    variable_dependencies[moved_watched - 1].watcher_index = index;
  }
  // Make "watched" the new watched variable of "variable".
  variable_dependencies[variable - 1].watcher = new_watched;
  variable_dependencies[variable - 1].watcher_index = variables_watched_by[new_watched - 1].size();
  variables_watched_by[new_watched - 1].push_back(variable);
}

bool DependencyManagerWatched::isDecisionCandidate(Variable v) const {
  if (prefix_mode) { // This is fairly inefficient in prefix mode and only included for purposes of debugging.
    if (solver.variable_data_store->isAssigned(v)) {
      return false;
    } else {
      bool vartype_v = solver.variable_data_store->varType(v);
      for (Variable w = 1; w < v; w++) {
        if (solver.variable_data_store->varType(w) != vartype_v && !solver.variable_data_store->isAssigned(w)) {
          return false;
        }
      }
      return true;
    }
  } else {
    return !solver.variable_data_store->isAssigned(v) && (watcher(v) == 0 || solver.variable_data_store->isAssigned(watcher(v)));
  }
}

bool DependencyManagerWatched::findWatchedDependency(Variable variable, bool remove_from_old) {
  /* Go through the vector of variables "variable" depends on. If the corresponding
  variable is unassigned, make it the new watcher. */
  for (Variable dependency: variable_dependencies[variable - 1].dependent_on_vector) {
    if (!solver.variable_data_store->isAssigned(dependency)) {
      setWatchedDependency(variable, dependency, remove_from_old);
      return true;
    }
  }
  return false;
}

}
