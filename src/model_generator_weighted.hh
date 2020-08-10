#ifndef model_generator_weighted_hh
#define model_generator_weighted_hh

#include "model_generator.hh"
#include "solver_types.hh"
#include <unordered_set>

namespace Qute {

class ModelGeneratorWeighted : public ModelGenerator {

public:
  ModelGeneratorWeighted(QCDCL_solver& solver, double exponent, double scaling_factor, double universal_penalty);
  virtual std::vector<Literal> generateModel();
private:
  double exponent;
  double scaling_factor;
  double universal_penalty;
  std::vector<double> variable_weights;

  class CompVarsByOccAndWeight {
    public:
      CompVarsByOccAndWeight(const std::vector<std::unordered_set<CRef>>& occurrences, const vector<double>& weights) : occ(occurrences), w(weights) {};
      bool operator()(Variable first, Variable second) {
        return occ[first].size() / w[first] > occ[second].size() / w[second];
      };
    protected:
      const std::vector<std::unordered_set<CRef>>& occ;
      const std::vector<double>& w;
  };

};

}

#endif
