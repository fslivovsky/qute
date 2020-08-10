#include "debug_helper.hh"
#include "variable_data.hh"
#include "dependency_manager_watched.hh"
#include "qcdcl.hh"

namespace Qute {

DebugHelper::DebugHelper(QCDCL_solver& solver): solver(solver) {}

bool DebugHelper::isUnit(Constraint& constraint, ConstraintType constraint_type) {
  Literal unassigned_primary = Literal_Undef;
  for (Literal l: constraint) {
    if (disablesConstraint(l, constraint_type)) {
      return false;
    }
    else if (isUnassignedPrimary(l, constraint_type)) {
      if (unassigned_primary == Literal_Undef) { 
        unassigned_primary = l;
      } else {
        return false;
      }
    }
  }
  if (unassigned_primary == Literal_Undef) {
    return false;
  } else {
    for (Literal l: constraint) {
      if (solver.variable_data_store->varType(var(l)) != constraint_type && !solver.variable_data_store->isAssigned(var(l)) && solver.dependency_manager->dependsOn(var(unassigned_primary), var(l))) {
        return false;
      }
    }
    return true;
  }
}

bool DebugHelper::isEmpty(Constraint& constraint, ConstraintType constraint_type) {
  for (Literal l: constraint) {
    if (disablesConstraint(l, constraint_type) || isUnassignedPrimary(l, constraint_type)) {
      return false;
    }
  }
  return true;
}

bool DebugHelper::isUnassignedPrimary(Literal literal, ConstraintType constraint_type) {
  return solver.variable_data_store->varType(var(literal)) == constraint_type && !solver.variable_data_store->isAssigned(var(literal));
}

bool DebugHelper::disablesConstraint(Literal literal, ConstraintType constraint_type) {
  return solver.variable_data_store->isAssigned(var(literal)) && (solver.variable_data_store->assignment(var(literal)) == sign(literal)) == disablingPolarity(constraint_type);
}

}
