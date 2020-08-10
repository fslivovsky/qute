#ifndef debug_helper_hh
#define debug_helper_hh

#include "solver_types.hh"
#include "constraint.hh"

namespace Qute {

class QCDCL_solver;

class DebugHelper {

public:
  DebugHelper(QCDCL_solver& solver);
  bool isUnit(Constraint& constraint, ConstraintType constraint_type);
  bool isEmpty(Constraint& constraint, ConstraintType constraint_type);

protected:
  bool disablesConstraint(Literal literal, ConstraintType constraint_type);
  bool isUnassignedPrimary(Literal literal, ConstraintType constraint_type);

  QCDCL_solver& solver;

};

}

#endif
