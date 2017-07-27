#ifndef pcnf_container_hh
#define pcnf_container_hh

#include <vector>
#include <string>
#include "solver_types.hh"

using std::string;
using std::vector;

namespace Qute {

class PCNFContainer {

public:
  virtual void addVariable(string original_name, char variable_type, bool auxiliary) = 0;
  virtual void addConstraint(vector<Literal>& literals, ConstraintType constraint_type) = 0;
  virtual void addDependency(Variable of, Variable on) = 0;
};
}

#endif
