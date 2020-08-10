#ifndef model_generator_simple_hh
#define model_generator_simple_hh

#include "model_generator.hh"

namespace Qute {

class ModelGeneratorSimple : public ModelGenerator {

public:
  ModelGeneratorSimple(QCDCL_solver& solver) : ModelGenerator(solver) {}
  virtual std::vector<Literal> generateModel();

};

}

#endif
