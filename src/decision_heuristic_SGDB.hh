#ifndef decision_heuristic_SGDB_hh
#define decision_heuristic_SGDB_hh

#include <stdlib.h>
#include <algorithm>
#include <math.h> 
#include <cmath>
#include "decision_heuristic.hh"
#include "variable_data.hh"
#include "dependency_manager_watched.hh"
#include "qcdcl.hh"

#include "minisat/mtl/Heap.h"
#include "minisat/mtl/IntMap.h"

using std::find;
using Minisat::Heap;
using Minisat::IntMap;

namespace Qute {

class DecisionHeuristicSGDB: public DecisionHeuristic {

public:
  DecisionHeuristicSGDB(QCDCL_solver& solver, bool no_phase_saving, double initial_learning_rate, double learning_rate_decay, double minimum_learning_rate, double lambda_factor);
  virtual void addVariable(bool auxiliary);
  virtual void notifyStart();
  virtual void notifyAssigned(Literal l);
  virtual void notifyUnassigned(Literal l);
  virtual void notifyEligible(Variable v);
  virtual void notifyLearned(Constraint& c, ConstraintType constraint_type, vector<Literal>& conflict_side_literals);
  virtual void notifyBacktrack(uint32_t decision_level_before);
  virtual Literal getDecisionLiteral();
  virtual void notifyConflict(ConstraintType constraint_type);

protected:
  Variable candidateFromVariableQueue(bool type);
  double sigmoid(double x);
  void updateParameters();
  void lazyRegularize(Variable v);
  bool assignmentPredictedConflict();
  bool lessThan(bool variable_type, double first_coefficient, double second_coefficient);

  bool no_phase_saving;

  double bias;
  double learning_rate;
  double learning_rate_decay;
  double minimum_learning_rate;
  double lambda_factor;
  double lambda;
  uint32_t backtrack_decision_level_before;
  vector<short> assigned_conflict_characteristic;
  // uint32_t output_last = 0;

  struct VariableData {
    bool is_auxiliary;
    uint32_t regularized_last;
    VariableData(bool is_auxiliary): is_auxiliary(is_auxiliary), regularized_last(0) {}
  };

  IntMap<Variable, double> coefficient;

  struct CompareVariables
  {
    CompareVariables(bool variable_type, const IntMap<Variable, double>&  coefficient): variable_type(variable_type), coefficient(coefficient) {};
    bool operator()(const Variable first, const Variable second) const {
      return (variable_type && (coefficient[first] > coefficient[second])) || (!variable_type && (coefficient[first] < coefficient[second]));
      /* For universal variables (variable_type == true), this amounts to the standard order < on coefficients.
         For existential variables (variable_type == false), this computes the order, variables are "larger" if
         they have smaller coefficients (which means their assignment is predicted to be more likely to lead to
         a solution). */
    }
    bool variable_type;
    const IntMap<Variable, double>&  coefficient;
  };

  vector<VariableData> variable_data;
  Heap<Variable,CompareVariables> universal_queue, existential_queue;

  //TrailIterator current_trail_position;
  double current_activation;

  // For debugging
  double maxCoeff(ConstraintType constraint_type);

};

inline void DecisionHeuristicSGDB::addVariable(bool auxiliary) {
  variable_data.emplace_back(auxiliary);
  saved_phase.push_back(l_Undef);
  coefficient.insert(solver.variable_data_store->lastVariable(), 0);
}

inline void DecisionHeuristicSGDB::notifyStart() {
  assigned_conflict_characteristic.resize(solver.variable_data_store->lastVariable());
  fill(assigned_conflict_characteristic.begin(), assigned_conflict_characteristic.end(), 0);
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    auto& variable_record = variable_data[v-1];
    if (!variable_record.is_auxiliary && solver.dependency_manager->isDecisionCandidate(v)) {
      auto variable_type = solver.variable_data_store->varType(v);
      if (variable_type) {
        universal_queue.insert(v);
      } else {
        existential_queue.insert(v);
      }
    }
  }
  //current_trail_position = solver.variable_data_store->trailBegin();
  //double current_activation = bias;
}

inline void DecisionHeuristicSGDB::notifyAssigned(Literal l) {
  current_activation += coefficient[var(l)];
  if (!no_phase_saving) {
    saved_phase[var(l) - 1] = sign(l);
  }
}

inline void DecisionHeuristicSGDB::notifyEligible(Variable v) {
  auto variable_type = solver.variable_data_store->varType(v);
  if (!variable_data[v-1].is_auxiliary) {
    if (variable_type) {
      universal_queue.update(v);
    } else {
      existential_queue.update(v);
    }
  }
}

inline void DecisionHeuristicSGDB::notifyBacktrack(uint32_t decision_level_before) {
  backtrack_decision_level_before = decision_level_before;
  //current_trail_position = solver.variable_data_store->trailBegin();
  //current_activation = bias;
}

inline Variable DecisionHeuristicSGDB::candidateFromVariableQueue(bool type) {
  Heap<Variable,CompareVariables>* queue;
  if (type) {
    queue = &universal_queue;
  } else {
    queue = &existential_queue;
  }
  Variable v = 0;
  while (!queue->empty() && !solver.dependency_manager->isDecisionCandidate((*queue)[0])) {
    queue->removeMin();
  }
  if (!queue->empty()) {
    v = queue->removeMin();
  }
  return v;
}

inline double DecisionHeuristicSGDB::sigmoid(double x) {
  return 1 / (1 + exp(-x));
}

inline void DecisionHeuristicSGDB::updateParameters() {
  if (learning_rate > minimum_learning_rate) {
    learning_rate = learning_rate - learning_rate_decay;
    lambda = learning_rate * lambda_factor;
  }
}

inline void DecisionHeuristicSGDB::lazyRegularize(Variable v) {
  auto& variable_record = variable_data[v-1];
  if (conflict_counter > variable_record.regularized_last) {
    coefficient[v] *= pow(1-learning_rate * lambda/2, conflict_counter - variable_record.regularized_last);
    auto variable_type = solver.variable_data_store->varType(v);
    if (!variable_data[v-1].is_auxiliary) {
      if (variable_type) {
        universal_queue.update(v);
      } else {
        existential_queue.update(v);
      }
    }
  }
  variable_record.regularized_last = conflict_counter;
}

inline double DecisionHeuristicSGDB::maxCoeff(ConstraintType constraint_type) {
  Variable max_variable = 0;
  double max_coeff;
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    auto& variable_record = variable_data[v-1];
    auto variable_type = solver.variable_data_store->varType(v);
    if (constraint_type == constraint_types[variable_type] && !variable_record.is_auxiliary && solver.dependency_manager->isDecisionCandidate(v) && 
        (max_variable == 0 || lessThan(variable_type, max_coeff, coefficient[v]))) {
      max_variable = v;
      max_coeff = coefficient[v];
    }
  }
  return max_coeff;
}

inline bool DecisionHeuristicSGDB::assignmentPredictedConflict() {
  /*double activation = bias;
  for (TrailIterator it = solver.variable_data_store->trailBegin(); it != solver.variable_data_store->trailEnd(); ++it) {
    auto v = var(*it);
    activation += coefficient[v];
  }*/
  auto prediction = sigmoid(current_activation);
  return prediction > 0.5;
}

inline bool DecisionHeuristicSGDB::lessThan(bool variable_type, double first_coefficient, double second_coefficient) {
  return (variable_type && (first_coefficient < second_coefficient)) || (!variable_type && (first_coefficient > second_coefficient));
}

}

#endif
