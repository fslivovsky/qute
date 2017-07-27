#include "variable_data.hh"

namespace Qute {

VariableDataStore::VariableDataStore(QCDCL_solver& solver): solver(solver), last_variable(0) {}

void VariableDataStore::addVariable(string original_name, bool type) {
  variable_name.push_back(original_name);
  variable_data.emplace_back();
  assignment_data.emplace_back(type);
  last_variable++;
}

void VariableDataStore::appendToTrail(Literal l, CRef reason) {
  trail.push_back(l);
  Variable v = var(l);
  assignment_data[v - 1].is_assigned = true;
  assignment_data[v - 1].assignment = sign(l);
  variable_data[v - 1].reason = reason;
  if (reason == CRef_Undef) {
    decisions.push_back(v);
  }
  variable_data[v - 1].decision_level = decisions.size();
}

Literal VariableDataStore::popFromTrail() {
  Literal last_literal = trail.back();
  Variable v = var(last_literal);
  CRef reason = varReason(v);
  if (reason == CRef_Undef) {
    decisions.pop_back();
  }
  trail.pop_back();
  assignment_data[v - 1].is_assigned = false;
  return last_literal;
}

void VariableDataStore::relocConstraintReferences(ConstraintType constraint_type) {
  for (Variable v = 1; v <= lastVariable(); v++) {
    if (varType(v) == constraint_type && varReason(v) != CRef_Undef) {
      solver.constraint_database->relocate(variable_data[v - 1].reason, constraint_type);
    }
  }
}

}
