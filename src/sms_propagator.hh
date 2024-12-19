#ifndef _SMS_PROPAGATOR_
#define _SMS_PROPAGATOR_

#include "external_propagator.hh"
#include "sms.hpp"

namespace Qute {

class SMSPropagator : public ExternalPropagator {
  public:
    SMSPropagator(QCDCL_solver* solver, int vertices, int cutoff);
    virtual bool checkSolution();
    virtual bool checkAssignment();
    virtual void printStats() { checker.printStats(); };
    QCDCL_solver* solver;

    SolverConfig config;
    MinimalityChecker checker;

    adjacency_matrix_t getAdjMatrix(bool from_solution = false);
    vector<Literal> blockingClause(const forbidden_graph_t& fg);
};
}

#endif /* ifndef _SMS_PROPAGATOR_ */
