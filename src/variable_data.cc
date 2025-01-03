#include "variable_data.hh"
#include "qcdcl.hh"
#include "constraint_DB.hh"

namespace Qute {

VariableDataStore::VariableDataStore(QCDCL_solver& solver): solver(solver), last_variable(0), last_universal(0) {}

Variable extract_integer_name(std::string name) {
  Variable result = 0;
  if (name[0] == '0')
    return 0;
  for (char c : name) {
    if ('0' <= c && c <= '9') {
      result *= 10;
      result += c - '0';
    } else {
      return 0;
    }
  }
  return result;
}

void VariableDataStore::addVariable(string original_name, bool type) {
  if (original_name == "") {
    original_name = std::to_string(next_orig_id++);
  } else {
    Variable name_id = extract_integer_name(original_name);
    if (name_id >= next_orig_id) {
      next_orig_id = name_id + 1;
    }
  }
  variable_name.push_back(original_name);
  variable_data.emplace_back();
  assignment_data.emplace_back(type);
  last_variable++;
  if (type == true)
    last_universal = last_variable;
  vars_of_type_until[type].push_back(vars_of_type_until[type].back() + 1);
  vars_of_type_until[1-type].push_back(vars_of_type_until[1-type].back());
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
