#include "model_generator_simple.hh"
#include "solver_types.hh"
#include "constraint.hh"
#include "qcdcl.hh"
#include "watched_literal_propagator.hh"
#include "constraint_DB.hh"
#include "variable_data.hh"

using std::vector;

namespace Qute {

vector<Literal> ModelGeneratorSimple::generateModel() {
  vector<Literal> model;
  vector<bool> is_in_model(solver.variable_data_store->lastVariable() + solver.variable_data_store->lastVariable() + 2);
  fill(is_in_model.begin(), is_in_model.end(), false);
  for (vector<CRef>::const_iterator it = solver.constraint_database->constraintReferencesBegin(ConstraintType::clauses, false); 
       it != solver.constraint_database->constraintReferencesEnd(ConstraintType::clauses, false); 
       ++it) {
    CRef constraint_reference = *it;
    Constraint& input_clause = solver.constraint_database->getConstraint(constraint_reference, ConstraintType::clauses);
    Literal disabling = Literal_Undef;
    bool already_covered = false;
    for (Literal lit : input_clause) {
      if (solver.propagator->disablesConstraint(lit, ConstraintType::clauses)) {
        if (is_in_model[toInt(lit)]) {
          already_covered = true;
          break;
        }
        disabling = lit;
        if (solver.variable_data_store->varType(var(lit)) == false) {
          break;
        }
      }
    }
    if (!already_covered) {
      is_in_model[toInt(disabling)] = true;
    }
  }
  for (unsigned int i = Min_Literal_Int; i < is_in_model.size() ; i++) {
    if (is_in_model[i]) {
      model.push_back(toLiteral(i));
    }
  }

  update_solver_statistics(model);

  return model;
}

}
