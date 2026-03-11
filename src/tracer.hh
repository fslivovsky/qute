#ifndef tracer_hh
#define tracer_hh

#include "constraint.hh"
#include "solver_types.hh"

namespace Qute {

class Tracer {

public:
  virtual ~Tracer() {}
  virtual bool notifyStart() = 0;
  virtual void notifyEnd() = 0;
  virtual void traceConstraint(vector<Literal>& literals, ConstraintType constraint_type, const vector<uint32_t>& premise_ids) = 0;
  virtual void traceConstraint(Constraint& constraint, ConstraintType constraint_type, const vector<uint32_t>& premise_ids) = 0;
  virtual void traceConstraint(Constraint& constraint, ConstraintType constraint_type) = 0;

  std::string filename;

};

}

#endif
