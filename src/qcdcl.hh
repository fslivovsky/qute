#ifndef qcdcl_hh
#define qcdcl_hh

#include "pcnf_container.hh"
#include <vector>

namespace Qute {

class Tracer;
class DecisionHeuristic;
class DependencyManagerWatched;
class Propagator;
class StandardLearningEngine;
class VariableDataStore;
struct Constraint;
class ConstraintDB;
class DebugHelper;
class ModelGenerator;
class RestartScheduler;
class ExternalPropagator;

class QCDCL_solver: public PCNFContainer {

public:
  QCDCL_solver(double time_limit);
  virtual ~QCDCL_solver();
  // Methods required by PCNFContainer.
  virtual void addVariable(string original_name, char variable_type, bool auxiliary);
  virtual CRef addConstraint(std::vector<Literal>& literals, ConstraintType constraint_type);
  virtual void addDependency(Variable of, Variable on);
  virtual void notifyMaxVarDeclaration(Variable max_var);
  virtual void notifyNumClausesDeclaration(uint32_t num_clauses);

  void addConstraintDuringSearch(std::vector<Literal>& literals, ConstraintType constraint_type, int id);
  Literal getUnitLiteralAfterBacktrack(std::vector<Literal>& clause);

  lbool solve();
  void interrupt();
  bool enqueue(Literal l, CRef reason);
  void printStatistics();
  static void machineReadableHeader();
  void machineReadableSummary();
  vector<Literal> blockingConstraint();

  std::string externalize (const Constraint &lits) const;
  std::string externalize (const vector<Literal> &lits) const;
  std::string externalize (Literal lit) const;

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

  // External Propagator (for SMS and others)
  ExternalPropagator* ext_prop = NULL;

  struct SolverStats
  {
    uint64_t backtracks_total = 0;
    uint64_t backtracks_dep = 0;
    uint64_t dep_conflicts_resolved = 0;
    uint64_t nr_decisions = 0;
    uint64_t nr_assignments = 0;
    uint64_t learned_total[2] = {0, 0};
    uint64_t learned_tautological[2] = {0, 0};
    uint64_t learned_asserting[2] = {0, 0};
    uint64_t nr_dependencies = 0; // learned by dependency learning
    uint64_t nr_independencies = 0; // proved by dependency scheme
    uint64_t nr_depscheme_reduced_lits = 0;
    clock_t  time_spent_computing_depscheme = 0;
    clock_t  time_spent_reducing_by_depscheme = 0;
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
  bool enumerate;
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

}

#endif
