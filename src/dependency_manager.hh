#ifndef dependency_manager_hh
#define dependency_manager_hh

#include "solver_types.hh"

namespace Qute {
  
class DependencyManager {

public:
  virtual ~DependencyManager() {}
  virtual void addVariable(bool auxiliary) = 0;
  virtual void addDependency(Variable of, Variable on) = 0;
  virtual void notifyStart() = 0;
  virtual void notifyAssigned(Variable v) = 0;
  virtual void notifyUnassigned(Variable v) = 0;
  virtual bool isDecisionCandidate(Variable v) const = 0;
  virtual bool dependsOn(Variable of, Variable on) const = 0;

};

}

#endif