#ifndef restart_scheduler_hh
#define restart_scheduler_hh

#include "solver_types.hh"

namespace Qute {

class RestartScheduler {

public:
  virtual ~RestartScheduler() {}
  virtual void notifyConflict(ConstraintType constraint_type) = 0;
  virtual void notifyLearned(Constraint& c) = 0;
  virtual bool restart() = 0;

};

}

#endif