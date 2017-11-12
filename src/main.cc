#include <functional>
#include <csignal>
#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "qcdcl.hh"
#include "parser.hh"
#include "solver_types.hh"
#include "constraint_DB.hh"
#include "decision_heuristic_VMTF_deplearn.hh"
#include "decision_heuristic_VMTF_prefix.hh"
#include "decision_heuristic_VSIDS_deplearn.hh"
#include "dependency_manager_watched.hh"
#include "restart_scheduler_none.hh"
#include "restart_scheduler_inner_outer.hh"
#include "standard_learning_engine.hh"
#include "variable_data.hh"
#include "watched_literal_propagator.hh"

using namespace Qute;
using namespace boost;
using namespace std::placeholders;
using std::cerr;
using std::cout;
using std::ifstream;
using std::to_string;
using std::string;

namespace logging = boost::log;

static QCDCL_solver* solver;

void signal_handler(int signal)
{
  solver->interrupt();
}

void checkPhaseHeuristic(string value, string option_string) {
  if (!(value == "invJW") && !(value == "qtype") && !(value == "watcher") && !(value == "random") && !(value == "true") && !(value == "false")) {
    throw program_options::error(option_string + " must be one of 'invJW', 'qtype', 'watcher', random', 'true', or 'false'");
  }
}

void checkIntervalDouble(double value, string option_string, double lower = 0, double upper = 1, bool excluding_lower = false, bool excluding_upper = false) {
  if ((value < lower || (excluding_lower && value == lower)) || (value > upper || (excluding_upper && value == upper))) {
    string lower_bracket = excluding_lower ? "(" : "[";
    string upper_bracket = excluding_upper ? ")" : "]";
    throw boost::program_options::error(option_string + " must be in the interval " + lower_bracket + to_string(lower) + ", " + to_string(upper) + upper_bracket);
  }
}

void checkIntervalInt(int value, string option_string, int lower = 0, int upper = 1, bool excluding_lower = false, bool excluding_upper = false) {
  if ((value < lower || (excluding_lower && value == lower)) || (value > upper || (excluding_upper && value == upper))) {
    string lower_bracket = excluding_lower ? "(" : "[";
    string upper_bracket = excluding_upper ? ")" : "]";
    throw boost::program_options::error(option_string + " must be an integer in the interval " + lower_bracket + to_string(lower) + ", " + to_string(upper) + upper_bracket);
  }
}

void checkVariableDecay(double value, string option_string, program_options::variables_map* vm) {
  if (!(*vm)["var-activity-decay"].defaulted() && (*vm)["decision-heuristic"].as<int>() == 0) {
    throw program_options::error(option_string + " can only be used with VSIDS decision heuristic");
  }
  checkIntervalDouble(value, "var-activity-decay", 0, 1, true, false);
}

void checkVariableIncrement(double value, string option_string, program_options::variables_map* vm) {
  if (!(*vm)["var-activity-inc"].defaulted() && (*vm)["decision-heuristic"].as<int>() == 0) {
    throw program_options::error(option_string + " can only be used with VSIDS decision heuristic");
  }
  checkIntervalDouble(value, "var-activity-inc", 0, 10, true, false);
}

void checkGreaterZero(int32_t value, string option_string, bool equal) {
  if (value < 0 || (!equal && value == 0)) {
    string or_equal = equal ? "or equal " : "";
    throw boost::program_options::error(option_string + " must be greater than " + or_equal + "0");
  }
}

void checkRestartParameters(bool value, program_options::variables_map* vm) {
  if (value && (!(*vm)["inner-restart-distance"].defaulted() || !(*vm)["outer-restart-distance"].defaulted() || !(*vm)["restart-multiplier"].defaulted())) {
    throw boost::program_options::error("cannot use options \"inner-restart-distance\", \"outer-restart-distance\", or \"restart-multiplier\" together with \"no-restarts\"");
  }
}

int main(int argc, char** argv)
{
  program_options::variables_map vm;
  program_options::options_description generic{"Options"};
  generic.add_options()
    ("help,h", "display this help message")
    ("initial-clause-DB-size", program_options::value<int32_t>()->default_value(4000)->
      notifier(std::bind(checkGreaterZero, _1, "initial-clause-DB-size", true)), "set initial learnt clause DB size")
    ("initial-term-DB-size", program_options::value<int32_t>()->default_value(500)->
      notifier(std::bind(checkGreaterZero, _1, "initial-term-DB-size", true)), "set initial learnt term DB size")
    ("clause-DB-increment", program_options::value<int32_t>()->default_value(4000)->
      notifier(std::bind(checkGreaterZero, _1, "clause-DB-increment", true)), "set clause database size increment")
    ("term-DB-increment", program_options::value<int32_t>()->default_value(500)->
      notifier(std::bind(checkGreaterZero, _1, "term-DB-increment", true)), "set term database size increment")
    ("clause-removal-ratio", program_options::value<double>()->default_value(0.5)->
      notifier(std::bind(checkIntervalDouble, _1, "clause-removal-ratio", 0, 1, false, false)), "set fraction of clauses removed while cleaning")
    ("term-removal-ratio", program_options::value<double>()->default_value(0.5)->
      notifier(std::bind(checkIntervalDouble, _1, "term-removal-ratio", 0, 1, false, false)), "set fraction of terms removed while cleaning")
    ("decision-heuristic", program_options::value<int>()->default_value(0)->
      notifier(std::bind(checkIntervalInt, _1, "decision-heuristic", 0, 5, false, false)),
      "Set variable decision heuristic.\n"
      "0 - VMTF\n"
      "1 - VSIDS\n"
      "2 - VSIDS (if equally active, pick variable with more primary occurrences)\n"
      "3 - VSIDS (if equally active, pick variable with fewer primary occurrences)\n"
      "4 - VSIDS (if equally active, pick variable with more secondary occurrences)\n"
      "5 - VSIDS (if equally active, pick variable with fewer secondary occurrences)")
    ("var-activity-inc", program_options::value<double>()->default_value(1, "1")->
      notifier(std::bind(checkVariableIncrement, _1, "var-activity-inc", &vm)), "set variable activity increment (VSIDS only)")
    ("var-activity-decay", program_options::value<double>()->default_value(0.95, "0.95")->
      notifier(std::bind(checkVariableDecay, _1, "var-activity-decay", &vm)), "set variable activity decay (VSIDS only)")
    ("use-activity-threshold", program_options::bool_switch(), "remove constraints with activities below threshold")
    ("constraint-activity-inc", program_options::value<double>()->default_value(1, "1")->
      notifier(std::bind(checkIntervalDouble, _1, "constraint-activity-inc", -10, 10, false, false)), "set constraint activity decay")
    ("constraint-activity-decay", program_options::value<double>()->default_value(0.999)->
      notifier(std::bind(checkIntervalDouble, _1, "constraint-activity-decay", 0, 1, false, true)), "set constraint activity decay")
    ("no-restarts", program_options::bool_switch()->
      notifier(std::bind(checkRestartParameters, _1, &vm)), "turn restarts off")
    ("model-generation", program_options::bool_switch(), "obtain initial terms by model generation")
    ("inner-restart-distance", program_options::value<int32_t>()->default_value(100)->
      notifier(std::bind(checkGreaterZero, _1, "inner-restart-distance", false)), "set initial number of conflicts until inner restart")
    ("outer-restart-distance", program_options::value<int32_t>()->default_value(100)->
      notifier(std::bind(checkGreaterZero, _1, "outer-restart-distance", false)), "set initial number of conflicts until inner restart")
    ("restart-multiplier", program_options::value<double>()->default_value(1.1, "1.1")->
      notifier(std::bind(checkIntervalDouble, _1, "restart-multiplier", 1, 5, false, false)), "restart limit multiplier")
    ("no-phase-saving", program_options::bool_switch(), "deactivate phase saving (always use phase selection heuristic)")
    ("partial-certificate", program_options::bool_switch(), "output assignment to outermost block")
    ("phase-heuristic", program_options::value<string>()->default_value("invJW")->
      notifier(std::bind(checkPhaseHeuristic, _1, "phase-heuristic")), "phase selection heuristic (invJW, qtype, watcher, random, false, true)")
    ("prefix-mode", program_options::bool_switch(), "run the solver in prefix mode (no dependency learning)")
    ("verbose,v", program_options::bool_switch(), "output information during solver run")
    ("print-stats", program_options::bool_switch(), "print solver statistics");
    //("trace", boost::program_options::bool_switch(), "enable tracing");

  program_options::options_description hidden{"Hidden options"};
  hidden.add_options()
    ("input-file", program_options::value<string>(), "input file");
  //   ("limit", boost::program_options::value<uint32_t>()->default_value(0)->implicit_value(2147483648), "set a limit on the trace size, implicitly 2GB");

  program_options::positional_options_description po_desc;
  po_desc.add("input-file", 1);

  program_options::options_description all_options;
  all_options.add(generic).add(hidden);

  try {
    program_options::store(program_options::command_line_parser(argc, argv).options(all_options).positional(po_desc).run(), vm);
    if (vm.count("help")) {
      cout << "Usage: qute FILENAME [Options] \n\n";
      cout << generic << "\n";
      return 0;
    }
    notify(vm);

    solver = new QCDCL_solver();

    ConstraintDB constraint_database(*solver,
                                     false,
                                     vm["constraint-activity-decay"].as<double>(), 
                                     static_cast<uint32_t>(vm["initial-clause-DB-size"].as<int32_t>()),
                                     static_cast<uint32_t>(vm["initial-term-DB-size"].as<int32_t>()),
                                     static_cast<uint32_t>(vm["clause-DB-increment"].as<int32_t>()),
                                     static_cast<uint32_t>(vm["term-DB-increment"].as<int32_t>()),
                                     vm["clause-removal-ratio"].as<double>(),
                                     vm["term-removal-ratio"].as<double>(),
                                     vm["use-activity-threshold"].as<bool>(),
                                     vm["constraint-activity-inc"].as<double>());
    solver->constraint_database = &constraint_database;
    VariableDataStore variable_data_store(*solver);
    solver->variable_data_store = &variable_data_store;
    DependencyManagerWatched dependency_manager(*solver, vm["prefix-mode"].as<bool>());
    solver->dependency_manager = &dependency_manager;
    DecisionHeuristic* decision_heuristic;
    if (vm["prefix-mode"].as<bool>()) {
      decision_heuristic = new DecisionHeuristicVMTFprefix(*solver, vm["no-phase-saving"].as<bool>());
    } else {
      auto decision_heuristic_option = static_cast<DecisionHeuristic::DecisionHeuristicOption>(vm["decision-heuristic"].as<int>());
      if (decision_heuristic_option == DecisionHeuristic::DecisionHeuristicOption::VMTF) {
        decision_heuristic = new DecisionHeuristicVMTFdeplearn(*solver, vm["no-phase-saving"].as<bool>());
      } else {
        bool tiebreak_scores = false;
        bool use_secondary_occurrences = false;
        bool prefer_fewer_occurrences = false;
        if (decision_heuristic_option == DecisionHeuristic::DecisionHeuristicOption::VSIDS_TIEBREAK_MORE_PRIMARY_OCCS) {
          tiebreak_scores = true;
        } else if (decision_heuristic_option == DecisionHeuristic::DecisionHeuristicOption::VSIDS_TIEBREAK_FEWER_PRIMARY_OCCS) {
          tiebreak_scores = true;
          prefer_fewer_occurrences = true;
        } else if (decision_heuristic_option == DecisionHeuristic::DecisionHeuristicOption::VSIDS_TIEBREAK_MORE_SECONDARY_OCCS) {
          tiebreak_scores = true;
          use_secondary_occurrences = true;
        } else if (decision_heuristic_option == DecisionHeuristic::DecisionHeuristicOption::VSIDS_TIEBREAK_FEWER_SECONDARY_OCCS) {
          tiebreak_scores = true;
          use_secondary_occurrences = true;
          prefer_fewer_occurrences = true;
        }
        decision_heuristic = new DecisionHeuristicVSIDSdeplearn(*solver,
                                                                vm["no-phase-saving"].as<bool>(),
                                                                vm["var-activity-decay"].as<double>(),
                                                                vm["var-activity-inc"].as<double>(),
                                                                tiebreak_scores,
                                                                use_secondary_occurrences,
                                                                prefer_fewer_occurrences);
      }
    }
    solver->decision_heuristic = decision_heuristic;

    if (!vm["phase-heuristic"].defaulted()) {
      string phase_string = vm["phase-heuristic"].as<string>();
      DecisionHeuristic::PhaseHeuristicOption phase_heuristic;
      if (phase_string == "qtype") {
        phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::QTYPE;
      } else if (phase_string == "watcher") {
        phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::WATCHER;
      } else if (phase_string == "random") {
        phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::RANDOM;
      } else if (phase_string == "false") {
        phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::PHFALSE;
      } else if (phase_string == "true") {
        phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::PHTRUE;
      } else {
        phase_heuristic = DecisionHeuristic::PhaseHeuristicOption::INVJW;
      }
      decision_heuristic->setPhaseHeuristic(phase_heuristic);
    }

    RestartScheduler* restart_scheduler;
    if (vm["no-restarts"].as<bool>()) {
      restart_scheduler = new RestartSchedulerNone; // Use dummy Restart Scheduler.
    } else {
      restart_scheduler = new RestartSchedulerInnerOuter(static_cast<uint32_t>(vm["inner-restart-distance"].as<int32_t>()), static_cast<uint32_t>(vm["outer-restart-distance"].as<int32_t>()), vm["restart-multiplier"].as<double>());
    }
    solver->restart_scheduler = restart_scheduler;
    StandardLearningEngine learning_engine(*solver);
    solver->learning_engine = &learning_engine;
    WatchedLiteralPropagator propagator(*solver);
    solver->propagator = &propagator;

    logging::core::get()->set_filter
    (
      logging::trivial::severity >= logging::trivial::error
    );

    Parser parser(*solver, vm["model-generation"].as<bool>());

    logging::add_common_attributes();

    auto fmtTimeStamp = logging::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S");
    auto fmtSeverity = logging::expressions::attr<logging::trivial::severity_level>("Severity");
    logging::formatter logFmt = logging::expressions::format("[%1%] [%2%] %3%") % fmtTimeStamp % fmtSeverity % logging::expressions::smessage;

    /* console sink */
    auto consoleSink = logging::add_console_log(std::clog);
    consoleSink->set_formatter(logFmt);

    if (vm.count("input-file")) {
      string filename = vm["input-file"].as<string>();
    	ifstream ifs(filename);
      if (!ifs.is_open()) {
        cerr << "qute: cannot access '" << filename << "': no such file or directory \n";
        return 2;
      } else {
        parser.readAUTO(ifs);
        ifs.close();
      }
    }
    else {
      parser.readAUTO();
    }

    if (vm["verbose"].as<bool>()) {
      logging::core::get()->set_filter
      (
        logging::trivial::severity >= logging::trivial::info
      );
    }

    // Register signal handler
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    lbool result = solver->solve();

    if (result != l_Undef && vm["partial-certificate"].as<bool>() && !learning_engine.reducedLast().empty()) {
      cout << "v " << learning_engine.reducedLast() << "0\n";
    }

    if (vm["print-stats"].as<bool>()) {
      solver->printStatistics();
    }

    delete solver;
    delete restart_scheduler;
    delete decision_heuristic;

    if (result == l_True) {
      cout << "SAT\n";
      return 10;
    } else if (result == l_False) {
      cout << "UNSAT\n";
      return 20;
    } else {
      cout << "UNDEF\n";
      return 0;
    }
  }
  catch (const program_options::error &ex) {
    cerr << "Error: " << ex.what() << ".\n\n";
    cout << "Usage: qute FILENAME [Options] \n\n";
    cout << generic << "\n";
    return 1;
  }
}
