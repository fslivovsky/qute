#include "decision_heuristic.hh"

namespace Qute {

DecisionHeuristic::DecisionHeuristic(QCDCL_solver& solver): solver(solver), phase_heuristic(PhaseHeuristicOption::INVJW) {
  distribution = bernoulli_distribution(0.5);
}

bool DecisionHeuristic::phaseHeuristic(Variable v) {
  switch (phase_heuristic)
  {
    case PhaseHeuristicOption::INVJW:
      return invJeroslowWang(v);
    case PhaseHeuristicOption::QTYPE:
      return qtypeDecHeur(v);
    case PhaseHeuristicOption::WATCHER:
      return watcherPhaseHeuristic(v);
    case PhaseHeuristicOption::RANDOM:
      return randomPhase();
    case PhaseHeuristicOption::PHFALSE:
      return false;
    case PhaseHeuristicOption::PHTRUE:
      return true;
  }
  return false;
}

bool DecisionHeuristic::invJeroslowWang(Variable v) {
  /* We want to "guess" a polarity so that we do not have to undo this
     assignment later. For a universal variable, that means we want to get to
     a conflict and falsify every clause. */
  if (solver.variable_data_store->varType(v)) {
    // Universal variable.
    return (invJeroslowWangScore(mkLiteral(v, false), ConstraintType::clauses) >
            invJeroslowWangScore(mkLiteral(v, true), ConstraintType::clauses));
  }
  else {
    return (invJeroslowWangScore(mkLiteral(v, true), ConstraintType::terms) >
            invJeroslowWangScore(mkLiteral(v, false), ConstraintType::terms));
  }
}

bool DecisionHeuristic::watcherPhaseHeuristic(Variable v) {
  bool var_type = solver.variable_data_store->varType(v);
  if (!var_type) {
    return solver.propagator->constraints_watched_by[ConstraintType::clauses][toInt(mkLiteral(v, false))].size() <
           solver.propagator->constraints_watched_by[ConstraintType::clauses][toInt(mkLiteral(v, true))].size();
  } else {
    return solver.propagator->constraints_watched_by[ConstraintType::terms][toInt(mkLiteral(v, true))].size() <
           solver.propagator->constraints_watched_by[ConstraintType::terms][toInt(mkLiteral(v, false))].size();
  }
}

double DecisionHeuristic::invJeroslowWangScore(Literal l, ConstraintType constraint_type) {
  double score = 0;
  for (vector<CRef>::const_iterator it = solver.constraint_database->literalOccurrencesBegin(l, constraint_type);
       it != solver.constraint_database->literalOccurrencesEnd(l, constraint_type);
       ++it) {
    Constraint& constraint = solver.constraint_database->getConstraint(*it, constraint_type);
    score += (1 << constraint.size);
  }
  return score;
}

bool DecisionHeuristic::qtypeDecHeur(Variable v) {
  /* We want to "guess" a polarity so that we do not have to undo this
     assignment later. For a universal variable, that means we want to get to
     a conflict and falsify every clause. */
  if (solver.variable_data_store->varType(v)) {
    // Universal variable.
    return (nrLiteralOccurrences(mkLiteral(v, false), ConstraintType::clauses) >
            nrLiteralOccurrences(mkLiteral(v, true), ConstraintType::clauses));
  }
  else {
    return (nrLiteralOccurrences(mkLiteral(v, true), ConstraintType::terms) >
            nrLiteralOccurrences(mkLiteral(v, false), ConstraintType::terms));
  }
}

int DecisionHeuristic::nrLiteralOccurrences(Literal l, ConstraintType constraint_type) {
  return solver.constraint_database->literalOccurrencesEnd(l, constraint_type) - solver.constraint_database->literalOccurrencesBegin(l, constraint_type);
}

}