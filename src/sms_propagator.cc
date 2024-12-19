#include "sms_propagator.hh"
#include "standard_learning_engine.hh"
#include "variable_data.hh"

namespace Qute {

std::vector<std::vector<int>> makeDefaultOrderingVector(int vertices) {
  std::vector<std::vector<int>> initialVertexOrderings = {std::vector<int>(vertices)};
  for (int i = 0; i < vertices; i++) {
    initialVertexOrderings.back()[i] = i;
  }
  return initialVertexOrderings;
}

SMSPropagator::SMSPropagator(QCDCL_solver* solver, int vertices, int cutoff) :
  solver(solver),
  config(vertices, cutoff),
  checker(30, config.initialPartition, makeDefaultOrderingVector(vertices), config.cutoff, NULL)
{}

bool SMSPropagator::checkSolution() {
  try {
    // TODO handle isFullyDefined properly
    checker.check(getAdjMatrix(true), true);
  } catch (forbidden_graph_t fg) {
    vector<Literal> clause = blockingClause(fg);
    solver->addConstraintDuringSearch(clause, ConstraintType::clauses, checker.counter);
    return false;
  }
  return true;
}

bool SMSPropagator::checkAssignment() {
  try {
    // TODO handle isFullyDefined properly
    checker.check(getAdjMatrix(false), true);
  } catch (forbidden_graph_t fg) {
    vector<Literal> clause = blockingClause(fg);
    solver->addConstraintDuringSearch(clause, ConstraintType::clauses, checker.counter);
    return false;
  }
  return true;
}

vector<Literal> SMSPropagator::blockingClause(const forbidden_graph_t& fg) {
  vector<Literal> clause;
  for (auto signedEdge : fg) {
    auto edge = signedEdge.second;
    Literal el = mkLiteral(config.edges[edge.first][edge.second], signedEdge.first != truth_value_true);
    clause.push_back(el);
  }
  return clause;
}

adjacency_matrix_t SMSPropagator::getAdjMatrix(bool from_solution) {
  adjacency_matrix_t matrix(config.vertices, vector<truth_value_t>(config.vertices, truth_value_unknown));
  if (from_solution) {
    for (int i = 0; i < config.vertices; i++) {
      //matrix[i][i] = truth_value_false;
      for (int j = i + 1; j < config.vertices; j++) {
        Variable eij = config.edges[i][j];
        matrix[i][j] = matrix[j][i] = solver->learning_engine->reducedLast(eij) ? truth_value_true : truth_value_false;
      }
    }
    return matrix;
  } else {
    //bool isFullyDefined = true;
    for (int i = 0; i < config.vertices; i++) {
      for (int j = i + 1; j < config.vertices; j++) {
        Variable eij = config.edges[i][j];
        if (solver->variable_data_store->isAssigned(eij)) {
          matrix[i][j] = matrix[j][i] = solver->variable_data_store->assignment(eij) ? truth_value_true : truth_value_false;
        } else {
          //isFullyDefined = false;
          matrix[i][j] = matrix[j][i] = truth_value_unknown;
        }
      }
    }
  }
  return matrix;
}


}

