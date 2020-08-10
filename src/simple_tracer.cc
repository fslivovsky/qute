#include "simple_tracer.hh"
#include "constraint_DB.hh"
#include "variable_data.hh"
#include "qcdcl.hh"

namespace Qute {

SimpleTracer::SimpleTracer(QCDCL_solver& solver): solver(solver), running_constraint_id(0) {};

void SimpleTracer::notifyStart() {
  out = unique_ptr<ostream>(new ostream(cout.rdbuf()));
  printPrefix();
  for (auto& constraint_type: constraint_types) {
    for (auto it = solver.constraint_database->constraintReferencesBegin(constraint_type, false); 
         it != solver.constraint_database->constraintReferencesEnd(constraint_type, false);
         ++it) {
      auto cref = *it;
      Constraint& c = solver.constraint_database->getConstraint(cref, constraint_type);
      traceConstraint(c, constraint_type);
    }
  }
}

template <class LiteralContainer> inline void SimpleTracer::printConstraint(LiteralContainer& container, ConstraintType constraint_type) {
  *out << ++running_constraint_id << " ";
  *out << (constraint_type == ConstraintType::clauses ? 0 : 1) << " ";
  for (auto& l: container) {
    *out <<  (sign(l) ? "": "-") << solver.variable_data_store->originalName(var(l)) << " ";
  }
  *out << "0 ";
}

inline void SimpleTracer::printPremises(vector<uint32_t>& premise_ids) {
  for (auto& id: premise_ids) {
    *out << id << " ";
  }
}

void SimpleTracer::traceConstraint(vector<Literal>& literals, ConstraintType constraint_type, vector<uint32_t>& premise_ids) {
  printConstraint(literals, constraint_type);
  printPremises(premise_ids);
  *out << "0\n";
}

void SimpleTracer::traceConstraint(Constraint& c, ConstraintType constraint_type, vector<uint32_t>& premise_ids) {
  printConstraint(c, constraint_type);
  c.id() = running_constraint_id;
  printPremises(premise_ids);
  *out << "0\n";
}

void SimpleTracer::traceConstraint(Constraint& c, ConstraintType constraint_type) {
  printConstraint(c, constraint_type);
  c.id() = running_constraint_id;
  *out << "0\n";
}

void SimpleTracer::printPrefix() {
  for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
    bool v_type = solver.variable_data_store->varType(v);
    if (v == 1 || v_type != solver.variable_data_store->varType(v-1)) {
      *out << (v_type ? "a": "e") << " ";
    }
    *out << solver.variable_data_store->originalName(v) << " ";
    if (v == solver.variable_data_store->lastVariable() || v_type != solver.variable_data_store->varType(v+1)) {
      *out << "0 \n";
    }
  }
}

}
