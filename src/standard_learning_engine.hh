#ifndef standard_learning_engine_hh
#define standard_learning_engine_hh

#include <vector>
#include <algorithm>
#include <string>
#include "learning_engine.hh"
#include "solver_types.hh"
#include "constraint.hh"

using std::vector;
using std::string;

namespace Qute {

class QCDCL_solver;

class StandardLearningEngine: public LearningEngine {

public:
  StandardLearningEngine(QCDCL_solver& solver, string rrs_mode);
  virtual bool analyzeConflict(CRef conflict_constraint_reference, ConstraintType constraint_type, 
                               vector<Literal>& literal_vector, uint32_t& decision_level_backtrack_before, 
                               Literal& unit_literal, bool& constraint_learned, vector<Literal>& conflict_side_literals,
                               vector<uint32_t>& premises, bool& result_is_tainted);
  string reducedLast();
  bool reducedLast(Variable v);

protected:
  vector<bool> constraintToCf(Constraint& constraint, ConstraintType constraint_type, Literal& rightmost_primary);
  bool cfToLiteralVector(vector<bool>& characteristic_function, vector<Literal>& literal_vector, Literal& rightmost_primary) const;
  vector<uint32_t> getPrimaryLiteralDecisionLevelCounts(Constraint& constraint, ConstraintType constraint_type);
  vector<uint32_t> getPrimaryLiteralDecisionLevelCounts(vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type);
  bool isAsserting(Literal last_literal, vector<bool>& characteristic_function, vector<uint32_t>& primary_literal_decision_level_counts, ConstraintType constraint_type);
  bool isPseudoAsserting(Literal last_literal, vector<bool>& characteristic_function, vector<uint32_t>& primary_literal_decision_level_counts, ConstraintType constraint_type);
  void resolveAndReduce(vector<bool>& characteristic_function, Constraint& reason, ConstraintType constraint_type, Literal pivot, Literal& rightmost_primary, vector<uint32_t>& primary_literal_decision_level_counts, vector<Literal>& literal_vector);
  uint32_t computeBackTrackLevel(Literal literal, vector<bool>& characteristic_function, Literal rightmost_primary, ConstraintType constraint_type);
  uint32_t computeBackTrackLevelPseudoUnit(Literal literal, vector<bool>& characteristic_function, Literal rightmost_primary, ConstraintType constraint_type);
  string cfToString(vector<bool>& characteristic_function, Literal rightmost_primary) const;

  vector<Literal> cfToVector(vector<bool>& characteristic_function, Literal rightmost_primary);

  QCDCL_solver& solver;
  vector<bool> reduced_last;
  
  bool use_rrs_for_qtype[2];
};

// Implementation of inline methods.

inline bool StandardLearningEngine::cfToLiteralVector(vector<bool>& characteristic_function, vector<Literal>& literal_vector, Literal& rightmost_primary) const {
  bool taut = false;
  for (int i = Min_Literal_Int; i <= toInt(rightmost_primary); i++) {
    if (characteristic_function[i]) {
      literal_vector.push_back(toLiteral(i));
      if (characteristic_function[toInt(~toLiteral(i))]) {
        taut = true;
      }
    }
  }
  return taut;
}

inline vector<Literal> cfToVector(vector<bool>& characteristic_function, Literal rightmost_primary) {
  vector<Literal> result;
  for (int i = Min_Literal_Int; i <= toInt(rightmost_primary); i++) {
    if (characteristic_function[i]) {
      result.push_back(toLiteral(i));
    }
  }
  return result;
}

inline bool StandardLearningEngine::reducedLast(Variable v) {
  return reduced_last[v];
}

}

#endif
