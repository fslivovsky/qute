#ifndef simple_tracer_hh
#define simple_tracer_hh

#include <fstream>
#include <streambuf>
#include <memory>
#include "tracer.hh"

using std::string;
using std::ofstream;
using std::ostream;
using std::streambuf;
using std::unique_ptr;

namespace Qute {

class QCDCL_solver;

class SimpleTracer: public Tracer {

public:
  SimpleTracer(QCDCL_solver& solver);
  virtual void notifyStart();
  virtual void traceConstraint(vector<Literal>& literals, ConstraintType constraint_type, vector<uint32_t>& premise_ids);
  virtual void traceConstraint(Constraint& constraint, ConstraintType constraint_type, vector<uint32_t>& premise_ids);
  virtual void traceConstraint(Constraint& c, ConstraintType constraint_type);

protected:
  void printPrefix();
  template <class LiteralContainer> void printConstraint(LiteralContainer& container, ConstraintType constraint_type);
  void printPremises(vector<uint32_t>& premise_ids);

  QCDCL_solver& solver;
  ofstream file_stream;
  unique_ptr<ostream> out;
  streambuf* buf;

  uint32_t running_constraint_id;

};

}

#endif
