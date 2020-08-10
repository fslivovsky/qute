#ifndef model_generator_hh
#define model_generator_hh

#include <vector>

namespace Qute {

class QCDCL_solver;
class Literal;

class ModelGenerator {
  public:
    ModelGenerator(QCDCL_solver& solver) : solver(solver) {}
    virtual ~ModelGenerator() {}
    virtual std::vector<Literal> generateModel() = 0;
    void update_solver_statistics(const std::vector<Literal>& model) const;
  protected:
    QCDCL_solver& solver;
};

}

#endif
