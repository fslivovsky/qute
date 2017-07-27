#ifndef qcdcl_hh
#define qcdcl_hh

#include <vector>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <algorithm>

#include "pcnf_container.hh"
#include "solver_types.hh"
#include "variable_data.hh"
#include "constraint_DB.hh"
#include "watched_literal_propagator.hh"
#include "decision_heuristic.hh"
#include "dependency_manager_watched.hh"
#include "restart_scheduler.hh"
#include "standard_learning_engine.hh"

using std::vector;
using std::cout;
using std::sort;
using std::unique;

namespace Qute {

class DecisionHeuristic;
class DependencyManagerWatched;
class WatchedLiteralPropagator;
class StandardLearningEngine;
class VariableDataStore;
class ConstraintDB;

class QCDCL_solver: public PCNFContainer {

public:
  QCDCL_solver();
  virtual ~QCDCL_solver();
  // Methods required by PCNFContainer.
  virtual void addVariable(string original_name, char variable_type, bool auxiliary);
  virtual void addConstraint(vector<Literal>& literals, ConstraintType constraint_type);
  virtual void addDependency(Variable of, Variable on);

  lbool solve();
  void interrupt();
  bool enqueue(Literal l, CRef reason);
  void printStatistics();

  // Subsystems.
  VariableDataStore* variable_data_store;
  ConstraintDB* constraint_database;
  WatchedLiteralPropagator* propagator;
  DecisionHeuristic* decision_heuristic;
  DependencyManagerWatched* dependency_manager;
  RestartScheduler* restart_scheduler;
  StandardLearningEngine* learning_engine;

  struct SolverStats
  {
    uint32_t backtracks_total = 0;
    uint32_t backtracks_dep = 0;
    uint32_t nr_decisions = 0;
    uint32_t nr_assignments = 0;
    uint32_t learned_total[2] = {0, 0};
    uint32_t learned_tautological[2] = {0, 0};
    uint32_t nr_dependencies = 0;
    //uint64_t learned_total_length[2] = {0, 0};
  } solver_statistics;

protected:
  void undoLast();
  void backtrackBefore(uint32_t target_decision_level);
  void restart();
  uint64_t computeNrTrivial();

  bool interrupt_flag;

};

// Implementation of inline methods.

inline void QCDCL_solver::interrupt() {
  interrupt_flag = true;
}

inline void QCDCL_solver::printStatistics() {
  cout << "Number of learned clauses: " << solver_statistics.learned_total[false] <<  "\n";
  cout << "Number of learned tautological clauses: " << solver_statistics.learned_tautological[false] <<  "\n";
  cout << "Number of learned terms: " << solver_statistics.learned_total[true] << "\n";
  cout << "Number of learned contradictory terms: " << solver_statistics.learned_tautological[true] << "\n";
  if (solver_statistics.nr_decisions) {
    cout << "Fraction of decisions among assignments: " << double(solver_statistics.nr_decisions) / double(solver_statistics.nr_assignments) << "\n";
  }
  cout << "Number of backtracks: " << solver_statistics.backtracks_total << "\n";
  cout << "Number of backtracks caused by dependency learning: " << solver_statistics.backtracks_dep << "\n";
  if (computeNrTrivial()) {
      cout << "Learned dependencies as a fraction of trivial: " << double(solver_statistics.nr_dependencies) / double(computeNrTrivial()) << "\n";
  }
}

}

#endif