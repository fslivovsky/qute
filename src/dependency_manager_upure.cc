#include "dependency_manager_upure.hh"
#include "qcdcl.hh"
//#include "logging.hh"
#include "variable_data.hh"
#include "constraint_DB.hh"
#include <stack>

namespace Qute {

	DependencyManagerUPure::DependencyManagerUPure(QCDCL_solver& solver, string dependency_learning_strategy, string out_of_order_decisions): DependencyManagerWatched(solver, dependency_learning_strategy, out_of_order_decisions) {
	}

	void DependencyManagerUPure::filterIndependentVariables(Variable unit_variable, vector<Literal>& literal_vector) {
		if (solver.variable_data_store->varType(unit_variable) == 1) { //ignore universal variables
			return;
		}
		solver.solver_statistics.backtracks_dep++;
		/*
		 * only compute more independencies if there is time
		 clock_t n = 3, c = 30;
		 clock_t now = clock();
		 if ((now - solver.birth_time) + n * c * CLOCKS_PER_SEC < n * solver.solver_statistics.time_spent_computing_UPure) {
		 return;
		 }*/
		for (Literal lit : literal_vector) {
			Variable litvar = var(lit);
			if (!independenciesKnown(litvar))
				getDepsUPure(litvar);
		}
		size_t j = 0;
		for (size_t i = 0; i < literal_vector.size(); ++i) {
			if (!notDependsOn(unit_variable, var(literal_vector[i]))) {
				literal_vector[j++] = literal_vector[i];
			}
		}
		if (j == 0) {
			++solver.solver_statistics.dep_conflicts_resolved;
		}
		literal_vector.resize(j);
	}

	void DependencyManagerUPure::reduceWithDepscheme(vector<bool>& characteristic_function, Literal& rightmost_primary, ConstraintType constraint_type) {
		clock_t t = clock();
		vector<Variable> blockers;
		int bound = Min_Literal_Int;
		for (int i = toInt(rightmost_primary); i >= bound; i--) {
			if (characteristic_function[i]) {
				Variable v = i >> 1;
				if (solver.variable_data_store->varType(v) == constraint_type) {
						blockers.push_back(v);
				} else if (independenciesKnown(v)) {
					bool can_be_reduced = true;
					for (Variable blocker : blockers) {
						// TODO: queries for v are in sorted order, can be optimized
						if (!notDependsOn(blocker, v)) {
							//std::cout << "the blocking variable " << solver.externalize(mkLiteral(blocker, false)) << " is preventing reduction of " << solver.externalize(mkLiteral(v, false)) << std::endl;
							can_be_reduced = false;
							break;
						}
					}
					characteristic_function[i] = !can_be_reduced;
					solver.solver_statistics.nr_depscheme_reduced_lits += can_be_reduced;
					if (i % 2 == 1) { // handle the opposite literal of v
						--i;
						solver.solver_statistics.nr_depscheme_reduced_lits += characteristic_function[i] && can_be_reduced;
						characteristic_function[i] = characteristic_function[i] && !can_be_reduced;
					}
				}
			}
		}
		solver.solver_statistics.time_spent_reducing_by_depscheme += clock()-t;
	}


	/*
	 * computes the variables that depend *ON* v
	 */
	void DependencyManagerUPure::getDepsUPure(Variable v) {
		if (variable_dependencies[v-1].independencies_known)
			return;
		clock_t t = clock();
		bool vqtype = solver.variable_data_store->varType(v);

		vector<bool> reachable_true = getReachable(mkLiteral(v, true));
		vector<bool> reachable_false = getReachable(mkLiteral(v, false));

		// TODO iterate over !vqtype variables only (using a next-of-type query?)
		for(int x = (v+1)*2; x <= solver.variable_data_store->lastVariable()*2; x += 2) {
			Variable xvar = x / 2;
			if (solver.variable_data_store->varType(xvar) != vqtype) {
				if ((reachable_true[x] && reachable_false[x^1]) || (reachable_true[x^1] && reachable_false[x])) {
					//std::cout << "Found that " << solver.externalize(mkLiteral(xvar)) << "  *does*  depend on " << solver.externalize(mkLiteral(v)) << std::endl; 
				}
				else {
					// independence detected
					//std::cout << "Found that " << solver.externalize(mkLiteral(xvar)) << " does not depend on " << solver.externalize(mkLiteral(v)) << std::endl; 
					addNonDependency(xvar, v);
				}
			}
		}
		variable_dependencies[v - 1].independencies_known = true;
		solver.solver_statistics.time_spent_computing_depscheme += clock()-t;
	}

	vector<bool> DependencyManagerUPure::getReachable(Literal l) {
		/* TODO optimizations?
		 *
		 * * (?) short-circuit once it is found that everything in the 
		 *	 dependency conflict is a dependency
		 * * (?) possible variant of the above: never run the second 'getReachable'
		 *	 if nothing from the dependency conflict is reachable the first time
		 *	 around (or in general, consider the results of the first run before
		 *	 the second one)
		 */

		uint32_t num_vars = solver.variable_data_store->lastVariable();
		uint32_t num_lits = num_vars * 2 + 2; // literals start at 2

		Variable lvar = var(l);
		bool lqtype = solver.variable_data_store->varType(var(l));
		bool target_qtype = 1 - lqtype;

		vector<bool> reachable(num_lits);
		vector<bool> explored(num_lits);
		std::stack<Literal> landing_literals;
		if (lqtype == 1) { // l is universal
			reachable.assign(num_lits, false);
			explored.assign(num_lits, false);
			landing_literals.push(l);
		} else {
			reachable.assign(num_lits, true);
		}

		Literal negl = ~l;

		uint32_t max_target_lits = 2*solver.variable_data_store->countVarsOfTypeRightOf(target_qtype, lvar);

		uint32_t target_lits_found = 0;
		while (!landing_literals.empty()) {
			Literal current_lit = landing_literals.top();
			landing_literals.pop();
			int current_lit_idx = toInt(current_lit);
			if (explored[current_lit_idx])
				continue;
			explored[current_lit_idx] = true;

			for (auto occit = solver.constraint_database->literalOccurrencesBegin(current_lit, ConstraintType::clauses);
					occit != solver.constraint_database->literalOccurrencesEnd(current_lit, ConstraintType::clauses);
					occit++) {
				//auto fel_ref = first_entry_literal.find(*occit);
				Constraint& clause = solver.constraint_database->getConstraint(*occit, ConstraintType::clauses);
				bool clause_has_negl = false;
				for (Literal lit : clause) {
					if (lit == negl) {
						// the path is not u-pure
						clause_has_negl = true;
						break;
					}
				}
				if (clause_has_negl)
					continue;
				//first_entry_literal[*occit] = current_lit;
				for (Literal lit : clause) {
					if (lit == current_lit)
						continue;
					if (!explored[toInt(~lit)]) {
						Variable litvar = var(lit);
						int lit_idx = toInt(lit);
						bool litvarqtype = solver.variable_data_store->varType(litvar);
						if (litvarqtype == 0 && l <= lit) {
							landing_literals.push(~lit);
						}
						if (litvarqtype == target_qtype && l < lit) {
							/* lit is validly reached by the current path */
							if (!reachable[lit_idx]) {
								reachable[lit_idx] = true;
								max_target_lits++;
							}
						}
					}
				}
				if (target_lits_found == max_target_lits) {
					/* clear the stack and break out of clause traversal,
					 * ensures a single return statement and RVO
					 */
					landing_literals = {};
					break;
				}
			}
		}

		return reachable;
	}

	void DependencyManagerUPure::addNonDependency(Variable of, Variable on) {
		solver.solver_statistics.nr_independencies++;
		variable_dependencies[on - 1].independent_of.push_back(of);
	}

}
