#ifndef _EXTERNAL_PROPAGATOR_
#define _EXTERNAL_PROPAGATOR_

#include "qcdcl.hh"

namespace Qute {

  class ExternalPropagator {

  public:
    virtual bool checkSolution() = 0;
    virtual bool checkAssignment() = 0;
    virtual void printStats() = 0;

    QCDCL_solver* solver;

  };
}

#endif /* ifndef _EXTERNAL_PROPAGATOR_ */
