#ifndef propagator_hh
#define propagator_hh

#include "solver_types.hh"

namespace Qute {

class Propagator {

public:
  virtual void addVariable() = 0;
  virtual CRef propagate(ConstraintType& constraint_type) = 0;
  virtual void addConstraint(CRef constraint_reference, ConstraintType constraint_type) = 0;
  //virtual void removeConstraint(CRef constraint_reference, ConstraintType constraint_type) = 0;
  virtual void notifyAssigned(Literal l) = 0;
  virtual void notifyBacktrack(uint32_t decision_level_before) = 0;
  virtual void relocConstraintReferences(ConstraintType constraint_type) = 0;

};

}

#endif