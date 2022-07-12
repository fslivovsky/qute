#ifndef three_watched_literal_propagator_hh
#define three_watched_literal_propagator_hh

#include <vector>
#include <algorithm>
#include <unordered_set>
#include "propagator.hh"
#include "solver_types.hh"
#include "constraint.hh"

using std::vector;

namespace Qute {

class QCDCL_solver;

class ThreeWatchedLiteralPropagator: public Propagator {
  friend class DecisionHeuristic;
  friend class ModelGeneratorSimple;
  friend class ModelGeneratorWeighted;

public:
  ThreeWatchedLiteralPropagator(QCDCL_solver& solver);
  virtual void addVariable();
  virtual CRef propagate(ConstraintType& constraint_type);
  virtual void addConstraint(CRef constraint_reference, ConstraintType constraint_type);
  virtual void notifyAssigned(Literal l);
  virtual void notifyBacktrack(uint32_t decision_level_before);
  virtual void relocConstraintReferences(ConstraintType constraint_type);
  virtual bool phaseAdvice(Variable v);
  virtual bool disablesConstraint(Literal literal, ConstraintType constraint_type);

protected:
  uint32_t findFirstWatcher(Constraint& constraint, ConstraintType constraint_type);
  uint32_t findSecondWatcher(Constraint& constraint, ConstraintType constraint_type);
  bool isUnassignedOrDisablingPrimary(Literal literal, ConstraintType constraint_type);
  bool isUnassignedOrDisablesConstraint(Literal literal, ConstraintType constraint_type);
  bool isBlockedOrDisablingSecondary(Literal literal, ConstraintType constraint_type, Literal primary);
  inline bool constraintIsWatchedByLiteral(Constraint& constraint, ConstraintType _constraint_type, Literal watcher);
  bool propagateUnwatched(CRef constraint_reference, ConstraintType constraint_type, bool& watchers_found);
  bool isDisabled(Constraint& constraint, ConstraintType constraint_type);
  bool isUnassignedPrimary(Literal literal, ConstraintType constraint_type);
  bool isBlockedSecondary(Literal literal, ConstraintType constraint_type, Literal primary);
  bool updateWatchedLiterals(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type, bool& watcher_changed);
  bool propagationCorrect();

  size_t findPrimaryWatcher(Constraint& constraint, ConstraintType constraint_type, size_t begin);
  size_t findSecondaryWatcher(Constraint& constraint, ConstraintType constraint_type, size_t first_secondary_index);
  int initWatchers(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type);
  bool enforceWatcherConsistency(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type, Literal last_assigned_literal, bool& watcher_changed);

  inline bool isPrimary(Literal literal, ConstraintType constraint_type);
  inline bool isAssigned(Literal literal);
  inline bool dependsOn(Literal who_depends_on_other, Literal who_is_depended_on);
  inline uint32_t decLevel(Literal l);

  void primariesToFront(Constraint& constraint, ConstraintType constraint_type);

  struct WatchedRecord
  {
    CRef constraint_reference;
    Literal blocker;

    WatchedRecord(CRef constraint_reference, Literal blocker): constraint_reference(constraint_reference), blocker(blocker) {}

    WatchedRecord& operator=(const WatchedRecord& other) {
      constraint_reference = other.constraint_reference;
      blocker = other.blocker;
      return *this;
    }
  };

  QCDCL_solver& solver;

  vector<Literal> propagation_queue;

  // we need to (?) distinguish between whether the watcher is primary or not
  vector<vector<WatchedRecord>> constraints_watched_by[2]; // constraints with at least 2 existential (presence of universals is irrelevant)
  vector<CRef> constraints_without_two_watchers[2]; // size-1 constraints can go here if necessary

  inline void watchConstraint(ConstraintType constraint_type, CRef constraint_reference, Literal watcher, Literal cowatcher);
};

// Implementation of inline methods.
inline void ThreeWatchedLiteralPropagator::addVariable() {
  for (ConstraintType constraint_type: constraint_types) {
    // Add entries for both literals.
    constraints_watched_by[constraint_type].emplace_back();
    constraints_watched_by[constraint_type].emplace_back();
  }
}

// inline void WatchedLiteralPropagator::removeConstraint(CRef constraint_reference, ConstraintType constraint_type) {
//   Constraint& constraint = solver.constraint_database->getConstraint(constraint_reference, constraint_type);
//   constraint.mark();
// }

inline void ThreeWatchedLiteralPropagator::notifyAssigned(Literal l) {
  propagation_queue.push_back(l);
}

}

#endif
