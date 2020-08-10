#ifndef decision_heuristic_hh
#define decision_heuristic_hh

#include <random>
#include <vector>
#include <map>
#include <algorithm>
//#include <dlib/svm.h>
#include "solver_types.hh"
#include "constraint.hh"

using std::random_device;
using std::bernoulli_distribution;
using std::map;
using std::fill;
using std::min;
using std::max;

namespace Qute {

class QCDCL_solver;

class DecisionHeuristic {

//typedef map<unsigned long,double> sample_type;
//typedef dlib::sparse_linear_kernel<sample_type> kernel_type;
// typedef dlib::sparse_radial_basis_kernel<sample_type> kernel_type;
//typedef dlib::svm_pegasos<kernel_type> trainer_type;

public:
  DecisionHeuristic(QCDCL_solver& solver);
  virtual ~DecisionHeuristic() {}
  virtual void addVariable(bool auxiliary) = 0;
  virtual void notifyStart() = 0;
  virtual void notifyAssigned(Literal l) = 0;
  virtual void notifyUnassigned(Literal l) = 0;
  virtual void notifyEligible(Variable v) = 0;
  virtual void notifyLearned(Constraint& c, ConstraintType constraint_type, vector<Literal>& conflict_side_literals) = 0;
  virtual void notifyBacktrack(uint32_t decision_level_before) = 0;
  virtual Literal getDecisionLiteral() = 0;
  virtual void notifyConflict(ConstraintType constraint_type);

  enum class PhaseHeuristicOption: int8_t {INVJW, QTYPE, WATCHER, RANDOM, PHFALSE, PHTRUE};

  void setPhaseHeuristic(PhaseHeuristicOption heuristic);
  bool phaseHeuristic(Variable v);

protected:
  //bool svmPhase(Variable v);
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
  vector<lbool> saved_phase;
  /*vector<map<Variable,bool>> assignment_cache;
  vector<bool> label_cache;
  map<Variable,trainer_type> trainer_map;
  map<Variable,uint32_t> last_update;*/
  uint32_t conflict_counter;

};

inline void DecisionHeuristic::setPhaseHeuristic(PhaseHeuristicOption heuristic) {
  phase_heuristic = heuristic;
}

inline bool DecisionHeuristic::randomPhase() {
  return distribution(generator);
}

}

#endif
