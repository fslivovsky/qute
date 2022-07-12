#ifndef qcdcl_hh
#define qcdcl_hh

#include "pcnf_container.hh"
#include <vector>
#include <iostream>
#include <iomanip>

using std::cout;

namespace Qute {

class Tracer;
class DecisionHeuristic;
class DependencyManagerWatched;
class Propagator;
class StandardLearningEngine;
class VariableDataStore;
class ConstraintDB;
class DebugHelper;
class ModelGenerator;
class RestartScheduler;

class QCDCL_solver: public PCNFContainer {

public:
  QCDCL_solver(double time_limit);
  virtual ~QCDCL_solver();
  // Methods required by PCNFContainer.
  virtual void addVariable(string original_name, char variable_type, bool auxiliary);
  virtual void addConstraint(std::vector<Literal>& literals, ConstraintType constraint_type);
  virtual void addDependency(Variable of, Variable on);
  virtual void notifyMaxVarDeclaration(Variable max_var);
  virtual void notifyNumClausesDeclaration(uint32_t num_clauses);

  lbool solve();
  void interrupt();
  bool enqueue(Literal l, CRef reason);
  void printStatistics();
  void machineReadableSummary();

  // Subsystems.
  VariableDataStore* variable_data_store;
  ConstraintDB* constraint_database;
  Propagator* propagator;
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
    uint32_t learned_asserting[2] = {0, 0};
    uint32_t nr_dependencies = 0;
    uint32_t nr_independencies = 0;
    uint32_t nr_RRS_reduced_lits = 0;
    clock_t  time_spent_computing_RRS = 0;
    clock_t  time_spent_reducing_by_RRS = 0;
    uint32_t initial_terms_generated = 0;
    double   average_initial_term_size = 0;
    uint64_t watched_list_accesses = 0;
    uint64_t spurious_watch_events = 0;
  } solver_statistics;

  struct SolverOptions
  {
    bool trace = true;
    bool print_stats = false;
  } options;

  double time_limit; // in seconds; limitless solving is simulated by a high time limit (default 1e52)
  clock_t t_birth;
  clock_t t_solve_begin;
  clock_t t_solve_end;
  lbool result;
  string filename;
  static const string string_result[3];

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
  cout << "Number of learned    asserting clauses: " << solver_statistics.learned_asserting[false] <<  "\n";
  cout << "Number of learned terms: " << solver_statistics.learned_total[true] << "\n";
  cout << "Number of learned contradictory terms: " << solver_statistics.learned_tautological[true] << "\n";
  cout << "Number of learned     asserting terms: " << solver_statistics.learned_asserting[true] <<  "\n";
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

inline void QCDCL_solver::machineReadableSummary() {

  double solve_time = (double)(t_solve_end - t_solve_begin) / CLOCKS_PER_SEC;
  double total_time = (double)(clock() - t_birth) / CLOCKS_PER_SEC;

  /*double bps = solver_statistics.backtracks_total / solve_time;
  double wps = solver_statistics.watched_list_accesses / solve_time;
  double swe = solver_statistics.spurious_watch_events / (double)solver_statistics.watched_list_accesses * 100;*/
  double ass_lrn_frac[2];
  double ass_lrn_mod[2] = {static_cast<double>(result == l_False), static_cast<double>(result == l_True)}; // make the empty constraint count as asserting
  for (ConstraintType ct : constraint_types) {
    if (solver_statistics.learned_total[ct] > 0) {
      ass_lrn_frac[ct] = (solver_statistics.learned_asserting[ct] + ass_lrn_mod[ct]) / solver_statistics.learned_total[ct];
    } else {
      ass_lrn_frac[ct] = -1;
    }
  }

  std::cout << "QUTE_ANS,"
    << filename << ","
    << string_result[result] << ","
    << std::setprecision(4)
    << solve_time << ","
    << total_time << ","
    << ass_lrn_frac[ConstraintType::clauses] << ","
    << ass_lrn_frac[ConstraintType::terms] << ","
    << solver_statistics.learned_total[ConstraintType::clauses] << ","
    << solver_statistics.learned_total[ConstraintType::terms] << ","
    << solver_statistics.learned_asserting[ConstraintType::clauses] << ","
    << solver_statistics.learned_asserting[ConstraintType::terms]
    << std::endl;

}

}

#endif
