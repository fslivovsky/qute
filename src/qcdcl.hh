#ifndef qcdcl_hh
#define qcdcl_hh

#include "pcnf_container.hh"
#include <vector>
#include <iostream>

using std::cout;

namespace Qute {

class Tracer;
class DecisionHeuristic;
class DependencyManagerWatched;
class WatchedLiteralPropagator;
class StandardLearningEngine;
class VariableDataStore;
class ConstraintDB;
class DebugHelper;
class ModelGenerator;
class RestartScheduler;

class QCDCL_solver: public PCNFContainer {

public:
  QCDCL_solver();
  virtual ~QCDCL_solver();
  // Methods required by PCNFContainer.
  virtual void addVariable(string original_name, char variable_type, bool auxiliary);
  virtual void addConstraint(std::vector<Literal>& literals, ConstraintType constraint_type);
  virtual void addDependency(Variable of, Variable on);

  lbool solve();
  void interrupt();
  bool enqueue(Literal l, CRef reason);
  void printStatistics();

  // Subsystems.
  VariableDataStore* variable_data_store;
  ConstraintDB* constraint_database;
  WatchedLiteralPropagator* propagator;
  ModelGenerator* model_generator;
  DecisionHeuristic* decision_heuristic;
  DependencyManagerWatched* dependency_manager;
  RestartScheduler* restart_scheduler;
  StandardLearningEngine* learning_engine;
  DebugHelper* debug_helper;
  Tracer* tracer;

  struct SolverStats
  {
    uint32_t backtracks_total = 0;
    uint32_t backtracks_dep = 0;
    uint32_t dep_conflicts_resolved = 0;
    uint32_t nr_decisions = 0;
    uint32_t nr_assignments = 0;
    uint32_t learned_total[2] = {0, 0};
    uint32_t learned_tautological[2] = {0, 0};
    uint32_t nr_dependencies = 0;
    uint32_t nr_independencies = 0;
    uint32_t nr_RRS_reduced_lits = 0;
    clock_t  time_spent_computing_RRS = 0;
    clock_t  time_spent_reducing_by_RRS = 0;
    uint32_t initial_terms_generated = 0;
    double   average_initial_term_size = 0;
  } solver_statistics;

  struct SolverOptions
  {
    bool trace = true;
    bool print_stats = false;
  } options;

  clock_t birth_time;

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
  if (options.print_stats) {
    printStatistics();
    options.print_stats = false;
  }
}

inline void QCDCL_solver::printStatistics() {
  cout << "Number of learned clauses: " << solver_statistics.learned_total[false] <<  "\n";
  cout << "Number of learned tautological clauses: " << solver_statistics.learned_tautological[false] <<  "\n";
  cout << "Number of learned terms: " << solver_statistics.learned_total[true] << "\n";
  cout << "Number of learned contradictory terms: " << solver_statistics.learned_tautological[true] << "\n";
  if (solver_statistics.nr_assignments) {
    cout << "Fraction of decisions among assignments: " << double(solver_statistics.nr_decisions) / double(solver_statistics.nr_assignments) << "\n";
  }
  cout << "Number of backtracks: " << solver_statistics.backtracks_total << "\n";
  cout << "Number of backtracks caused by dependency learning: " << solver_statistics.backtracks_dep << "\n";
  cout << "Number of dependency conflicts resolved by RRS: " << solver_statistics.dep_conflicts_resolved << "\n";
  cout << "Number of proven independencies: " << solver_statistics.nr_independencies << std::endl;
  cout << "Number of literals reduced thanks to RRS: " << solver_statistics.nr_RRS_reduced_lits << std::endl;
  cout << "Amount of time spent computing RRS deps (s): " << double(solver_statistics.time_spent_computing_RRS) / CLOCKS_PER_SEC << std::endl;
  cout << "Amount of time spent on generalized forall reduction (s): " << double(solver_statistics.time_spent_reducing_by_RRS) / CLOCKS_PER_SEC << std::endl;
  if (computeNrTrivial()) {
    cout << "Learned dependencies as a fraction of trivial: " << double(solver_statistics.nr_dependencies) / double(computeNrTrivial()) << std::endl;
  }
  cout << "Number of initial terms generated: " << solver_statistics.initial_terms_generated << std::endl;
  cout << "Average initial term size: " << solver_statistics.average_initial_term_size << std::endl;
}

}

#endif
