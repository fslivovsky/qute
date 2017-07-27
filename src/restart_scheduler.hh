#ifndef restart_scheduler_hh
#define restart_scheduler_hh

#include "solver_types.hh"

namespace Qute {

class RestartScheduler {

public:
  virtual ~RestartScheduler() {}
  virtual bool notifyConflict(ConstraintType constraint_type) = 0;

};

}

#endif