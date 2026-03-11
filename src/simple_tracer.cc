#include "simple_tracer.hh"
#include "constraint_DB.hh"
#include "variable_data.hh"
#include "qcdcl.hh"

namespace Qute {

SimpleTracer::SimpleTracer(QCDCL_solver& solver, string filename):
	filename(filename),
	solver(solver),
	file_stream(filename),
	running_constraint_id(0) {
	}

bool SimpleTracer::notifyStart() {
  //out = unique_ptr<ostream>(new ostream(std::cout.rdbuf()));
  if (!file_stream.is_open())
	  return false;
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
  return true;
}

void SimpleTracer::notifyEnd() {
	file_stream.flush();
	file_stream.close();
}

template <class LiteralContainer> inline void SimpleTracer::printConstraint(LiteralContainer& container, ConstraintType constraint_type) {
  file_stream << ++running_constraint_id << " ";
  file_stream << (constraint_type == ConstraintType::clauses ? 0 : 1) << " ";
  for (auto& l: container) {
    file_stream <<  (sign(l) ? "": "-") << solver.variable_data_store->originalName(var(l)) << " ";
  }
  file_stream << "0 ";
}

inline void SimpleTracer::printPremises(const vector<uint32_t>& premise_ids) {
  for (auto& id: premise_ids) {
    file_stream << id << " ";
  }
}

void SimpleTracer::traceConstraint(vector<Literal>& literals, ConstraintType constraint_type, const vector<uint32_t>& premise_ids) {
  printConstraint(literals, constraint_type);
  printPremises(premise_ids);
  file_stream << "0\n";
}

void SimpleTracer::traceConstraint(Constraint& c, ConstraintType constraint_type, const vector<uint32_t>& premise_ids) {
  printConstraint(c, constraint_type);
  c.id() = running_constraint_id;
  printPremises(premise_ids);
  file_stream << "0\n";
}

void SimpleTracer::traceConstraint(Constraint& c, ConstraintType constraint_type) {
  printConstraint(c, constraint_type);
  c.id() = running_constraint_id;
  file_stream << "0\n";
}

void SimpleTracer::printPrefix() {
  //for (Variable v = 1; v <= solver.variable_data_store->lastVariable(); v++) {
  for (Variable v : solver.variable_data_store->ordinary) {
    bool v_type = solver.variable_data_store->varType(v);
    if (v == 1 || v_type != solver.variable_data_store->varType(v-1)) {
      file_stream << (v_type ? "a": "e") << " ";
    }
    file_stream << solver.variable_data_store->originalName(v) << " ";
    if (v == solver.variable_data_store->lastVariable() || v_type != solver.variable_data_store->varType(v+1)) {
      file_stream << "0 \n";
    }
  }
}

}
