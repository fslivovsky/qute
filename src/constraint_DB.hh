#ifndef constraint_DB_hh
#define constraint_DB_hh

#include <boost/log/trivial.hpp>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include "solver_types.hh"
#include "constraint.hh"
#include "qcdcl.hh"

using std::vector;
using std::unordered_map;
using std::sort;

namespace Qute {

class QCDCL_solver;

class ConstraintDB {

public:
  ConstraintDB(QCDCL_solver& solver, bool print_trace, double constraint_activity_decay, uint32_t max_learnt_clauses, uint32_t max_learnt_terms, uint32_t learnt_clauses_increment, uint32_t learnt_terms_increment, double clause_removal_ratio, double term_removal_ratio, bool use_activity_threshold, double constraint_increment);
  CRef addConstraint(vector<Literal>& literals, ConstraintType constraint_type, bool learnt);
  Constraint& getConstraint(CRef constraint_reference, ConstraintType constraint_type);
  vector<CRef>::const_iterator constraintReferencesBegin(ConstraintType constraint_type, bool learnt);
  vector<CRef>::const_iterator constraintReferencesEnd(ConstraintType constraint_type, bool learnt);
  vector<CRef>::const_iterator literalOccurrencesBegin(Literal l, ConstraintType constraint_type);
  vector<CRef>::const_iterator literalOccurrencesEnd(Literal l, ConstraintType constraint_type);
  void bumpConstraintActivity(Constraint& constraint, ConstraintType constraint_type);
  virtual void notifyStart();
  virtual void notifyConflict(ConstraintType constraint_type);
  virtual void notifyRestart();
  void updateLBD(Constraint& constraint);
  void relocate(CRef& constraint_reference, ConstraintType constraint_type);
  
protected:
  void decayConstraintActivity(ConstraintType constraint_type);
  void rescaleConstraintActivity(ConstraintType constraint_type);
  void relocConstraintReferences(ConstraintType constraint_type);
  void relocAll(ConstraintType constraint_type);
  void cleanConstraints(ConstraintType constraint_type);
  bool isLocked(Constraint& constraint, CRef constraint_reference, ConstraintType constraint_type);

  struct ConstraintCompare {
    ConstraintAllocator& ca;
    ConstraintCompare(ConstraintAllocator& ca): ca(ca) {}
    bool operator() (CRef first_constraint_reference, CRef second_constraint_reference) {
      return (ca[first_constraint_reference].LBD() > ca[second_constraint_reference].LBD() ||
              (ca[first_constraint_reference].LBD() == ca[second_constraint_reference].LBD() &&
               ca[first_constraint_reference].activity() < ca[second_constraint_reference].activity()));
    }
  };

  double removal_ratio[2];
  QCDCL_solver& solver;
  bool print_trace;
  ConstraintAllocator constraints[2];
  vector<CRef> input_constraint_references[2];
  vector<CRef> learnt_constraint_references[2];
  unordered_map<Literal, vector<CRef>> literal_occurrences[2];
  double constraint_inc[2];
  double constraint_activity_decay;
  uint32_t learnts_max[2];
  uint32_t learnts_increment[2];
  ConstraintAllocator* ca_to;
  bool use_activity_threshold;
};

// Implementation of inline methods.

inline Constraint& ConstraintDB::getConstraint(CRef constraint_reference, ConstraintType constraint_type) {
  return constraints[constraint_type][constraint_reference];
}

inline vector<CRef>::const_iterator ConstraintDB::constraintReferencesBegin(ConstraintType constraint_type, bool learnt) {
  return learnt ? learnt_constraint_references[constraint_type].cbegin(): input_constraint_references[constraint_type].cbegin();
}

inline vector<CRef>::const_iterator ConstraintDB::constraintReferencesEnd(ConstraintType constraint_type, bool learnt) {
  return learnt ? learnt_constraint_references[constraint_type].cend(): input_constraint_references[constraint_type].cend();
}

inline vector<CRef>::const_iterator ConstraintDB::literalOccurrencesBegin(Literal l, ConstraintType constraint_type) {
  return literal_occurrences[constraint_type][l].cbegin();
}

inline vector<CRef>::const_iterator ConstraintDB::literalOccurrencesEnd(Literal l, ConstraintType constraint_type) {
  return literal_occurrences[constraint_type][l].cend();
}

inline void ConstraintDB::bumpConstraintActivity(Constraint& constraint, ConstraintType constraint_type) {
  constraint.activity() += constraint_inc[constraint_type];
  if (constraint.activity() > 1e60) {
    rescaleConstraintActivity(constraint_type);
  }
}

inline void ConstraintDB::notifyStart() {}

inline void ConstraintDB::notifyConflict(ConstraintType constraint_type) {
  decayConstraintActivity(constraint_type);
  if (learnt_constraint_references[constraint_type].size() >= learnts_max[constraint_type]) {
    BOOST_LOG_TRIVIAL(info) << "Reached learnt " << (constraint_type ? "term ": "clause ") << "limit of " << learnts_max[constraint_type] << ".";
    learnts_max[constraint_type] += learnts_increment[constraint_type];
    cleanConstraints(constraint_type);
  }
}

inline void ConstraintDB::notifyRestart() {
}

inline void ConstraintDB::relocate(CRef& constraint_reference, ConstraintType constraint_type) {
  assert(ca_to != nullptr);
  constraints[constraint_type].reloc(constraint_reference, *ca_to);
}

inline void ConstraintDB::decayConstraintActivity(ConstraintType constraint_type) {
  constraint_inc[constraint_type] *= (1 / constraint_activity_decay);
}

inline void ConstraintDB::rescaleConstraintActivity(ConstraintType constraint_type) {
  for (CRef constraint_reference: learnt_constraint_references[constraint_type]) {
    Constraint& constraint = getConstraint(constraint_reference, constraint_type);
    constraint.activity() *= 1e-60;
  }
  constraint_inc[constraint_type] *= 1e-60;
}

}

#endif