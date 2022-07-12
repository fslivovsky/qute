#ifndef learning_engine_hh
#define learning_engine_hh

#include <vector>
#include "solver_types.hh"

using std::vector;

namespace Qute {

class LearningEngine {

public:
  virtual bool analyzeConflict(CRef conflict_constraint_reference, ConstraintType constraint_type, vector<Literal>& literal_vector, uint32_t& decision_level_backtrack_before, Literal& unit_literal, bool& constraint_learned, vector<Literal>& conflict_side_literals, vector<uint32_t>& premises) = 0;

};

}

#endif
