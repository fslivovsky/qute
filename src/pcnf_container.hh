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
  virtual CRef addConstraint(vector<Literal>& literals, ConstraintType constraint_type) = 0;
  virtual void addDependency(Variable of, Variable on) = 0;
  virtual void notifyMaxVarDeclaration(Variable max_var) = 0;
  virtual void notifyNumClausesDeclaration(uint32_t num_clauses) = 0;
};
}

#endif
