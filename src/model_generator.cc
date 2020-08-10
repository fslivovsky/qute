#include "model_generator.hh"
#include "qcdcl.hh"

namespace Qute {

void ModelGenerator::update_solver_statistics(const vector<Literal>& model) const {
      ++solver.solver_statistics.initial_terms_generated;
      solver.solver_statistics.average_initial_term_size = solver.solver_statistics.average_initial_term_size + (model.size() - solver.solver_statistics.average_initial_term_size) / solver.solver_statistics.initial_terms_generated;
}

}
