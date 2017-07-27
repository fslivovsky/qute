#ifndef decision_heuristic_hh
#define decision_heuristic_hh

#include <random>
#include "qcdcl.hh"
#include "solver_types.hh"
#include "constraint.hh"

using std::random_device;
using std::bernoulli_distribution;

namespace Qute {

class DecisionHeuristic {

public:
  DecisionHeuristic(QCDCL_solver& solver);
  virtual ~DecisionHeuristic() {}
  virtual void addVariable(bool auxiliary) = 0;
  virtual void notifyStart() = 0;
  virtual void notifyAssigned(Literal l) = 0;
  virtual void notifyUnassigned(Literal l) = 0;
  virtual void notifyEligible(Variable v) = 0;
  virtual void notifyLearned(Constraint& c) = 0;
  virtual void notifyBacktrack(uint32_t decision_level_before) = 0;
  virtual Literal getDecisionLiteral() = 0;

  enum class DecisionHeuristicOption: int8_t {VMTF = 0, VSIDS = 1, VSIDS_TIEBREAK_MORE_PRIMARY_OCCS = 2, VSIDS_TIEBREAK_FEWER_PRIMARY_OCCS = 3, VSIDS_TIEBREAK_MORE_SECONDARY_OCCS = 4, VSIDS_TIEBREAK_FEWER_SECONDARY_OCCS = 5};
  enum class PhaseHeuristicOption: int8_t {INVJW, QTYPE, WATCHER, RANDOM, PHFALSE, PHTRUE};

  void setPhaseHeuristic(PhaseHeuristicOption heuristic);
  bool phaseHeuristic(Variable v);

protected:
  bool randomPhase();
  bool invJeroslowWang(Variable v);
  bool watcherPhaseHeuristic(Variable v);
  double invJeroslowWangScore(Literal l, ConstraintType constraint_type);
  bool qtypeDecHeur(Variable v);
  int nrLiteralOccurrences(Literal l, ConstraintType constraint_type);

  QCDCL_solver& solver;
  random_device generator;
  bernoulli_distribution distribution;
  PhaseHeuristicOption phase_heuristic;

};

inline void DecisionHeuristic::setPhaseHeuristic(PhaseHeuristicOption heuristic) {
  phase_heuristic = heuristic;
}

inline bool DecisionHeuristic::randomPhase() {
  return distribution(generator);
}

}

#endif