#ifndef variable_data_hh
#define variable_data_hh

#include "solver_types.hh"
#include "constraint.hh"

#include <string>
#include <vector>

using std::string;
using std::to_string;
using std::vector;

namespace Qute {

class QCDCL_solver;

class VariableDataStore {

public:
  VariableDataStore(QCDCL_solver& solver);
  void addVariable(string original_name, bool type);
  string originalName(Variable v) const;
  bool varType(Variable v) const;
  bool isAssigned(Variable v) const;
  bool assignment(Variable v) const;
  void appendToTrail(Literal l, CRef reason);
  uint32_t decisionLevel() const;
  uint32_t varDecisionLevel(Variable v) const;
  bool decisionLevelType(uint32_t decision_level);
  CRef varReason(Variable v) const;
  Literal popFromTrail();
  bool trailIsEmpty() const;
  Variable lastVariable() const;
  Variable lastUniversal() const;
  TrailIterator trailBegin() const;
  TrailIterator trailEnd() const;
  void relocConstraintReferences(ConstraintType constraint_type);
  string constraintToString(Constraint& constraint) const;
  string literalVectorToString(vector<Literal>& literal_vector) const;
  bool allAssigned() const;
  inline int countVarsOfTypeUntil(bool type, Variable x) const;
  inline int countVarsOfTypeLeftOf(bool type, Variable x) const;
  inline int countVarsOfTypeRightOf(bool type, Variable x) const;
  inline int countVarsOfTypeBetween(bool type, Variable x, Variable y) const;
  // TODO implement next_of_type queries using the new data structure
  Variable getNextOfType(bool type, Variable x) const;
  Variable getPreviousOfType(bool type, Variable x) const;

protected:
  struct AssignmentData
  {
    bool is_assigned: 1;
    bool assignment: 1;
    bool type: 1;
    AssignmentData(bool type): is_assigned(false), assignment(false), type(type) {}
  };

  struct VariableData
  {
    uint32_t decision_level;
    CRef reason;
    VariableData(): reason(CRef_Undef) {}
  };

  vector<Literal> trail;
  vector<VariableData> variable_data;
  vector<AssignmentData> assignment_data;
  vector<string> variable_name;

  QCDCL_solver& solver;
  Variable last_variable;
  Variable last_universal;
  Variable next_orig_id; // next available original-name-id for a fresh variable (external name)
  vector<Variable> vars_of_type_until[2] = {{0},{0}};
  vector<Variable> decisions;

public:
  vector<Variable> to_enumerate; // enumerate winning moves to this set of variables (should be a subset of the first block)
  vector<Variable> auxiliary; // need to know which variables are auxiliary in QCIR, for renaming
  vector<Variable> ordinary;
  inline void rename_auxiliary_variables() {
    for (Variable v : auxiliary) {
      if (varType(v)) { // universal
        variable_name[v - 1] = std::to_string(next_orig_id++);
      }
    }
  };
};

// Implementation of inline methods.

inline string VariableDataStore::originalName(Variable v) const {
  return variable_name[v - 1];
}

inline bool VariableDataStore::varType(Variable v) const {
  return assignment_data[v - 1].type;
}

inline bool VariableDataStore::isAssigned(Variable v) const {
  return assignment_data[v - 1].is_assigned;
}

inline bool VariableDataStore::assignment(Variable v) const {
  return assignment_data[v - 1].assignment;
}

inline uint32_t VariableDataStore::decisionLevel() const {
  return decisions.size();
}

inline uint32_t VariableDataStore::varDecisionLevel(Variable v) const {
  return variable_data[v - 1].decision_level;
}

inline bool VariableDataStore::decisionLevelType(uint32_t decision_level) {
  return varType(decisions[decision_level - 1]);
}

inline CRef VariableDataStore::varReason(Variable v) const {
  return variable_data[v - 1].reason;
}

inline bool VariableDataStore::trailIsEmpty() const {
  return trail.empty();
}

inline Variable VariableDataStore::lastVariable() const {
  return last_variable;
}

inline Variable VariableDataStore::lastUniversal() const {
  return last_universal;
}

inline TrailIterator VariableDataStore::trailBegin() const {
  return TrailIterator(&trail[0]);
}

inline TrailIterator VariableDataStore::trailEnd() const {
  return TrailIterator(&trail[trail.size()]);
}

inline string VariableDataStore::constraintToString(Constraint& constraint) const {
  vector<Literal> literal_vector;
  for (Literal l: constraint) {
    literal_vector.push_back(l);
  }
  return literalVectorToString(literal_vector);
}

inline string VariableDataStore::literalVectorToString(vector<Literal>& literal_vector) const {
  string out_string = "( ";
  for (Literal l: literal_vector) {
    out_string += (sign(l) ? "" : "-");
    out_string += to_string(var(l));
    out_string += (varType(var(l)) ? "A" : "E");
    out_string += (isAssigned(var(l)) ? (":" + to_string(assignment(var(l)) == sign(l)) + 
    "@" + to_string(varDecisionLevel(var(l)))) : "") + " ";
  }
  out_string += ")";
  return out_string;
}

inline bool VariableDataStore::allAssigned() const {
  return trail.size() == variable_data.size();
}

inline int VariableDataStore::countVarsOfTypeUntil(bool type, Variable x) const {
	return vars_of_type_until[type][x];
}

inline int VariableDataStore::countVarsOfTypeLeftOf(bool type, Variable x) const {
	return vars_of_type_until[type][x-1];
}

inline int VariableDataStore::countVarsOfTypeRightOf(bool type, Variable x) const {
	return vars_of_type_until[type][last_variable] - vars_of_type_until[type][x];
}

inline int VariableDataStore::countVarsOfTypeBetween(bool type, Variable x, Variable y) const {
	/* returns the number of variables to the left of y, but not to the left of x,
	 * i.e. including x and excluding y */
	return (x < y) * (vars_of_type_until[type][y-1] - vars_of_type_until[type][x-1]);
}

}

#endif
