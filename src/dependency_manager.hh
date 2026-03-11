#ifndef dependency_manager_hh
#define dependency_manager_hh

#include "solver_types.hh"

namespace Qute {
  
class DependencyManager {

public:
  virtual ~DependencyManager() {}
  virtual void addVariable(bool auxiliary, bool qtype) = 0;
  virtual void addDependency(Variable of, Variable on) = 0;
  virtual void notifyStart() = 0;
  virtual void notifyAssigned(Variable v) = 0;
  virtual void notifyUnassigned(Variable v) = 0;
  virtual bool isDecisionCandidate(Variable v) const = 0;
  virtual bool dependsOn(Variable of, Variable on) const = 0;
  //virtual bool notDependsOn(Variable of, Variable on) const = 0;
  virtual void learnDependencies(Variable unit_variable, vector<Literal>& literal_vector) = 0;
  virtual void reduceWithDepscheme(std::vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type) = 0;
  virtual void filterIndependentVariables(Variable unit_variable, vector<Literal>& literal_vector) = 0;

	// TODO should change the nomenclature:
	// 	reduceWitHRRS should be reduceWithDepScheme
	//	dependsOn and notDependsOn should clearly be identified with their framework:
	//		dependsOn is dependency learning
	//		notDependsOn is dep scheme
	//	should be made clear that isDecisionCandidate does *not* use a dep scheme
	//	also should be made clear that the learning engine is still looking for a classically asserting clause (is that even the case)?
	//		probably not the case, since it's looking for dep-learning asserting clause, and any dep-scheme independence is guaranteed not to be learned
};

}

#endif
