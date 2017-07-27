#ifndef variable_data_hh
#define variable_data_hh

#include <string>
#include <vector>
#include "solver_types.hh"
#include "constraint.hh"
#include "qcdcl.hh"

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
  TrailIterator trailBegin() const;
  TrailIterator trailEnd() const;
  void relocConstraintReferences(ConstraintType constraint_type);
  string constraintToString(Constraint& constraint) const;
  string literalVectorToString(vector<Literal>& literal_vector) const;
  bool allAssigned() const;

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
  vector<Variable> decisions;

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

}

#endif