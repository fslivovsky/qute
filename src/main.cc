#include <limits>
#include <csignal>
#include <iostream>
#include <string>

#include "main.hh"
#include "logging.hh"
#include "simple_tracer.hh"
#include "debug_helper.hh"
#include "qcdcl.hh"
#include "parser.hh"
#include "solver_types.hh"
#include "constraint_DB.hh"
#include "decision_heuristic_VMTF_deplearn.hh"
#include "decision_heuristic_VMTF_prefix.hh"
#include "decision_heuristic_VSIDS_deplearn.hh"
#include "decision_heuristic_SGDB.hh"
#include "dependency_manager_rrs.hh"
#include "model_generator_simple.hh"
#include "model_generator_weighted.hh"
#include "restart_scheduler_none.hh"
#include "restart_scheduler_inner_outer.hh"
#include "restart_scheduler_ema.hh"
#include "restart_scheduler_luby.hh"
#include "standard_learning_engine.hh"
#include "variable_data.hh"
#include "three_watched_literal_propagator.hh"
#include "watched_literal_propagator.hh"
#ifdef USE_SMS
#include "sms_propagator.hh"
#endif

using namespace Qute;
using namespace std::placeholders;
using std::cerr;
using std::cout;
using std::ifstream;
using std::string;

static unique_ptr<QCDCL_solver> solver;

void signal_handler(int signal)
{
  solver->interrupt();
}

static const char USAGE[] =
R"(Usage: qute [options] [<path>]

General Options:
  --initial-clause-DB-size <int>        initial learnt clause DB size [default: 4000]
  --initial-term-DB-size <int>          initial learnt term DB size [default: 500]
  --clause-DB-increment <int>           clause database size increment [default: 4000]
  --term-DB-increment <int>             term database size increment [default: 500]
  --clause-removal-ratio <double>       fraction of clauses removed while cleaning [default: 0.5]
  --term-removal-ratio <double>         fraction of terms removed while cleaning [default: 0.5]
  --use-activity-threshold              remove all constraints with activities below threshold
  --LBD-threshold <int>                 only remove constraints with LBD larger than this [default: 2]
  --constraint-activity-inc <double>    constraint activity increment [default: 1]
  --constraint-activity-decay <double>  constraint activity decay [default: 0.999]
  --decision-heuristic arg              variable decision heuristic [default: VMTF]
                                        (VSIDS | VMTF | SGDB)
  --restarts arg                        restart strategy [default: inner-outer]
                                        (off | luby | inner-outer | EMA)
  --model-generation arg                model generation strategy for initial terms [default: depqbf]
                                        (off | depqbf | weighted)
  --dependency-learning arg             dependency learning strategy
                                        (off | outermost | fewest | all) [default: all]
  --watched-literals <int>              watched-literals scheme (out-of-order decisions require 3)
                                        (2 | 3) [default: 2]
  --out-of-order-decisions arg          lift the constraint that decisions should respect dependencies [default: off]
                                        (off, existential, universal, all)
  --rrs arg                             toggle the use of the resolution-path dependency scheme
                                        (off | clauses | both) [default: off]
  --no-phase-saving                     deactivate phase saving
  --phase-heuristic arg                 phase selection heuristic [default: watcher]
                                        (invJW, qtype, watcher, random, false, true) 
  --partial-certificate                 output assignment to outermost block
  -v --verbose                          output information during solver run
  --print-stats                         print statistics on termination
  --machine-readable                    print a machine-readable CSV line with comprehensive information instead of just the answer
  --trace                               output solver trace for certificate generation
  -t --time-limit <double>              tell the solver to give up after this much time (in seconds) [default: 1e52]

Weighted Model Generation Options:
  --exponent <double>                   exponent skewing the distribution of weights [default: 1]
  --scaling-factor <double>             scaling factor for variable weights [default: 1]
  --universal-penalty <double>          additive penalty for universal variables [default: 0]

VSIDS Options:
  --tiebreak arg                        tiebreaking strategy for equally active variables [default: arbitrary]
                                        (arbitrary, more-primary, fewer-primary, more-secondary, fewer-secondary)
  --var-activity-inc <double>           variable activity increment [default: 1]
  --var-activity-decay <double>         variable activity decay [default: 0.95]

SGDB Options:
  --initial-learning-rate <double>      Initial learning rate [default: 0.8]
  --learning-rate-decay <double>        Learning rate additive decay [default: 2e-6]
  --learning-rate-minimum <double>      Minimum learning rate [default: 0.12]
  --lambda-factor <double>              Regularization parameter [default: 0.1]

Luby Restart Options:
  --luby-restart-multiplier <int>       Multiplier for restart intervals [default: 50]

EMA Restart Options:
  --alpha <double>                      Weight of new constraint LBD [default: 2e-5]
  --minimum-distance <int>              Minimum restart distance [default: 20]
  --threshold-factor <double>           Restart if short term LBD is this much larger than long term LBD [default: 1.4]

Outer-Inner Restart Options:
  --inner-restart-distance <int>        initial number of conflicts until inner restart [default: 100]
  --outer-restart-distance <int>        initial number of conflicts until outer restart [default: 100]
  --restart-multiplier <double>         restart limit multiplier [default: 1.1]

SMS Options:
  --sms-vertices <int>                  search for graphs with this many vertices [default: 2]
  --sms-cutoff <int>                    cutoff limit for SMS's minimality check [default: 0]
  -E --enumerate                        enumerate all solutions to the outermost block

)";

int main(int argc, const char** argv)
{
  std::map<std::string, docopt::value> args = docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "Qute v.1.1");

  /*for (auto arg: args) { // For debugging only.
    std::cout << arg.first << " " << arg.second << "\n";
  }*/

  // BEGIN Command Line Parameter Validation

  vector<unique_ptr<ArgumentConstraint>> argument_constraints;
  regex non_neg_int("[[:digit:]]+");
  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--initial-clause-DB-size", "unsigned int"));
  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--initial-term-DB-size", "unsigned int"));
  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--clause-DB-increment", "unsigned int"));
  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--term-DB-increment", "unsigned int"));

  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--clause-removal-ratio"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--term-removal-ratio"));

  argument_constraints.push_back(make_unique<DoubleConstraint>("--constraint-activity-inc"));
  // argument_constraints.push_back(make_unique<DoubleConstraint>("--activity-threshold"));
  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--LBD-threshold", "unsigned int"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--constraint-activity-decay"));

  vector<string> decision_heuristics = {"VSIDS", "VMTF", "SGDB"};
  argument_constraints.push_back(make_unique<ListConstraint>(decision_heuristics, "--decision-heuristic"));
  
  vector<string> restart_strategies = {"off", "luby", "inner-outer", "EMA"};
  argument_constraints.push_back(make_unique<ListConstraint>(restart_strategies, "--restarts"));

  vector<string> model_generation_strategies = {"off", "depqbf", "weighted"};
  argument_constraints.push_back(make_unique<ListConstraint>(model_generation_strategies, "--model-generation"));

  vector<string> watched_literal_schemes = {"2", "3"};
  argument_constraints.push_back(make_unique<ListConstraint>(watched_literal_schemes, "--watched-literals"));

  vector<string> out_of_order_decision_modes = {"off", "existential", "universal", "all"};
  argument_constraints.push_back(make_unique<ListConstraint>(out_of_order_decision_modes, "--out-of-order-decisions"));

  vector<string> dependency_learning_strategies = {"off", "outermost", "fewest", "all"};
  argument_constraints.push_back(make_unique<ListConstraint>(dependency_learning_strategies, "--dependency-learning"));

  vector<string> rrs_mode = {"off", "clauses", "both"};
  argument_constraints.push_back(make_unique<ListConstraint>(rrs_mode, "--rrs"));

  vector<string> phase_heuristics = {"invJW", "qtype", "watcher", "random", "false", "true"};
  argument_constraints.push_back(make_unique<ListConstraint>(phase_heuristics, "--phase-heuristic"));

  vector<string> VSIDS_tiebreak_strategies = {"arbitrary", "more-primary", "fewer-primary", "more-secondary", "fewer-secondary"};
  argument_constraints.push_back(make_unique<ListConstraint>(VSIDS_tiebreak_strategies, "--tiebreak"));

  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0.5, 2, "--exponent"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--scaling-factor"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--universal-penalty"));

  argument_constraints.push_back(make_unique<DoubleConstraint>("--var-activity-inc"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--var-activity-decay"));

  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--initial-learning-rate"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--learning-rate-decay"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--learning-rate-minimum"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--lambda-factor"));

  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(1, std::numeric_limits<double>::infinity(), "--luby-restart-multiplier", false, true));

  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, 1, "--alpha"));
  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--minimum-distance", "unsigned int"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(0, std::numeric_limits<double>::infinity(), "--threshold-factor", false, true));

  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--inner-restart-distance", "unsigned int"));
  argument_constraints.push_back(make_unique<RegexArgumentConstraint>(non_neg_int, "--outer-restart-distance", "unsigned int"));
  argument_constraints.push_back(make_unique<DoubleRangeConstraint>(1, std::numeric_limits<double>::infinity(), "--restart-multiplier", false, true));

  argument_constraints.push_back(make_unique<IfThenConstraint>("--dependency-learning", "off", "--decision-heuristic", "VMTF",
    "decision heuristic must be VMTF if dependency learning is deactivated"));

  // TODO validate SMS args

  for (auto& constraint_ptr: argument_constraints) {
    if (!constraint_ptr->check(args)) {
      std::cout << constraint_ptr->message() << "\n\n";
      std::cout << USAGE;
      return 0;
    }
  }

  // END Command Line Parameter Validation
  solver = make_unique<QCDCL_solver>(std::stod(args["--time-limit"].asString()));

  solver->enumerate = args["--enumerate"].asBool();

#ifdef USE_SMS
  // initialize an external propagator if desired
  unique_ptr<ExternalPropagator> ext_prop;
  // load SMS options into the solver instance
  int vertices = args["--sms-vertices"].asLong();
  int cutoff = args["--sms-cutoff"].asLong();
  if (vertices > 2) {
    ext_prop = make_unique<SMSPropagator>(solver.get(), vertices, cutoff);
  }
  solver->ext_prop = ext_prop.get();
#endif

  solver->options.trace = args["--trace"].asBool();
  unique_ptr<Tracer> tracer;
  if (solver->options.trace) {
    tracer = make_unique<SimpleTracer>(*solver);
    solver->tracer = tracer.get();
  }

  ConstraintDB constraint_database(*solver,
                                    solver->options.trace,
                                    std::stod(args["--constraint-activity-decay"].asString()), 
                                    static_cast<uint32_t>(args["--initial-clause-DB-size"].asLong()),
                                    static_cast<uint32_t>(args["--initial-term-DB-size"].asLong()),
                                    static_cast<uint32_t>(args["--clause-DB-increment"].asLong()),
                                    static_cast<uint32_t>(args["--term-DB-increment"].asLong()),
                                    std::stod(args["--clause-removal-ratio"].asString()),
                                    std::stod(args["--term-removal-ratio"].asString()),
                                    args["--use-activity-threshold"].asBool(),
                                    std::stod(args["--constraint-activity-inc"].asString()),
                                    static_cast<uint32_t>(args["--LBD-threshold"].asLong())
                                    );
  solver->constraint_database = &constraint_database;
  DebugHelper debug_helper(*solver);
  solver->debug_helper = &debug_helper;
  VariableDataStore variable_data_store(*solver);
  solver->variable_data_store = &variable_data_store;

  unique_ptr<DependencyManagerWatched> dependency_manager;
  if (args["--rrs"].asString() == "off") {
    dependency_manager = make_unique<DependencyManagerWatched>(*solver, args["--dependency-learning"].asString(), args["--out-of-order-decisions"].asString());
  } else {
    dependency_manager = make_unique<DependencyManagerRRS>(*solver, args["--dependency-learning"].asString(), args["--out-of-order-decisions"].asString());
  }
  solver->dependency_manager = dependency_manager.get();

  unique_ptr<DecisionHeuristic> decision_heuristic;

if (args["--dependency-learning"].asString() == "off") {
  decision_heuristic = make_unique<DecisionHeuristicVMTFprefix>(*solver, args["--no-phase-saving"].asBool());
} else if (args["--decision-heuristic"].asString() == "VMTF") {
  decision_heuristic = make_unique<DecisionHeuristicVMTFdeplearn>(*solver, args["--no-phase-saving"].asBool());
} else if (args["--decision-heuristic"].asString() == "VSIDS") {
  bool tiebreak_scores = false;
  bool use_secondary_occurrences = false;
  bool prefer_fewer_occurrences = false;
  if (args["--tiebreak"].asString() == "arbitrary") {
    tiebreak_scores = false;
  } else if (args["--tiebreak"].asString() == "more-primary") {
    tiebreak_scores = true;
    use_secondary_occurrences = false;
    prefer_fewer_occurrences = false;
  } else if (args["--tiebreak"].asString() == "fewer-primary") {
    tiebreak_scores = true;
    use_secondary_occurrences = false;
    prefer_fewer_occurrences = true;
  } else if (args["--tiebreak"].asString() == "more-secondary") {
    tiebreak_scores = true;
    use_secondary_occurrences = true;
    prefer_fewer_occurrences = false;
  } else if (args["--tiebreak"].asString() == "fewer-secondary") {
    tiebreak_scores = true;
    use_secondary_occurrences = true;
    prefer_fewer_occurrences = true;
  } else {
    assert(false);
  }
  decision_heuristic = make_unique<DecisionHeuristicVSIDSdeplearn>(*solver,
                                                          args["--no-phase-saving"].asBool(),
                                                          std::stod(args["--var-activity-decay"].asString()),
                                                          std::stod(args["--var-activity-inc"].asString()),
                                                          tiebreak_scores,
                                                          use_secondary_occurrences,
                                                          prefer_fewer_occurrences);
  } else if (args["--decision-heuristic"].asString() == "SGDB") {
    decision_heuristic = make_unique<DecisionHeuristicSGDB>(*solver,
                                                    args["--no-phase-saving"].asBool(),
                                                    std::stod(args["--initial-learning-rate"].asString()),
                                                    std::stod(args["--learning-rate-decay"].asString()),
                                                    std::stod(args["--learning-rate-minimum"].asString()),
                                                    std::stod(args["--lambda-factor"].asString()));
  } else {
    assert(false);
  }
  solver->decision_heuristic = decision_heuristic.get();

  DecisionHeuristic::PhaseHeuristicOption phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::PHFALSE;
  if (args["--phase-heuristic"].asString() == "qtype") {
    phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::QTYPE;
  } else if (args["--phase-heuristic"].asString() == "watcher") {
    phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::WATCHER;
  } else if (args["--phase-heuristic"].asString() == "random") {
    phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::RANDOM;
  } else if (args["--phase-heuristic"].asString() == "false") {
    phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::PHFALSE;
  } else if (args["--phase-heuristic"].asString() == "true") {
    phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::PHTRUE;
  } else if (args["--phase-heuristic"].asString() == "invJW") {
    phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::INVJW;
  } else {
    assert(false);
  }
  decision_heuristic->setPhaseHeuristic(phase_heuristic);

  unique_ptr<RestartScheduler> restart_scheduler;

  if (args["--restarts"].asString() == "off") {
    restart_scheduler = make_unique<RestartSchedulerNone>();
  } else if (args["--restarts"].asString() == "inner-outer") {
    restart_scheduler = make_unique<RestartSchedulerInnerOuter>(
      static_cast<uint32_t>(args["--inner-restart-distance"].asLong()),
      static_cast<uint32_t>(args["--outer-restart-distance"].asLong()),
      std::stod(args["--restart-multiplier"].asString())
    );
  } else if (args["--restarts"].asString() == "luby") {
    restart_scheduler = make_unique<RestartSchedulerLuby>(static_cast<uint32_t>(args["--luby-restart-multiplier"].asLong()));
  } else if (args["--restarts"].asString() == "EMA") {
    restart_scheduler = make_unique<RestartSchedulerEMA>(
      std::stod(args["--alpha"].asString()),
      static_cast<uint32_t>(args["--minimum-distance"].asLong()),
      std::stod(args["--threshold-factor"].asString())
    );
  } else {
    assert(false);
  }

  solver->restart_scheduler = restart_scheduler.get();

  StandardLearningEngine learning_engine(*solver, args["--rrs"].asString());
  solver->learning_engine = &learning_engine;

  unique_ptr<Propagator> propagator;
  if (args["--watched-literals"].asLong() == 2 && args["--out-of-order-decisions"].asString() == "off") {
    propagator = make_unique<WatchedLiteralPropagator>(*solver);
  } else {
    propagator = make_unique<ThreeWatchedLiteralPropagator>(*solver);
  }
  solver->propagator = propagator.get();

  Parser parser(*solver, args["--model-generation"].asString() != "off");

  // PARSER
  if (args["<path>"]) {
    string filename = args["<path>"].asString();
  	ifstream ifs(filename);
    if (!ifs.is_open()) {
      cerr << "qute: cannot access '" << filename << "': no such file or directory \n";
      return 2;
    } else {
      solver->filename = filename;
      parser.readAUTO(ifs);
      ifs.close();
    }
  }
  else {
    parser.readAUTO();
  }
  
  unique_ptr<ModelGenerator> model_generator;
  if (args["--model-generation"].asString() == "weighted") {
    model_generator = make_unique<ModelGeneratorWeighted>(
          *solver,
          std::stod(args["--exponent"].asString()),
          std::stod(args["--scaling-factor"].asString()),
          std::stod(args["--universal-penalty"].asString())
        );
  } else if(args["--model-generation"].asString() == "depqbf") {
    model_generator = make_unique<ModelGeneratorSimple>(*solver);
  }
  solver->model_generator = model_generator.get();

  // LOGGING
  if (args["--verbose"].asBool()) {
    Logger::get().setOutputLevel(Loglevel::info);
  }

  // Register signal handler
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGINT, signal_handler);

  solver->options.print_stats = args["--print-stats"].asBool();

  lbool result = solver->solve();

  if (solver->options.print_stats) {
    solver->printStatistics();
  }

  if (!solver->options.trace) {
    if (args["--machine-readable"].asBool()) {
      solver->machineReadableSummary();
    } else {
      cout << solver->string_result[result] << std::endl;
    }
  }

  if (args["--partial-certificate"].asBool() && ((result == l_True && !solver->variable_data_store->varType(1)) ||
                                                 (result == l_False && solver->variable_data_store->varType(1)))) {
    cout << "v " << learning_engine.reducedLast() << "\n";
  }

  return (result == l_True ? 10 : result == l_False ? 20 : 0);
}
