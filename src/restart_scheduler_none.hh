#ifndef restart_scheduler_none_hh
#define restart_scheduler_none_hh

#include "solver_types.hh"
#include "restart_scheduler.hh"

namespace Qute {

class RestartSchedulerNone: public RestartScheduler {

public:
  virtual bool notifyConflict(ConstraintType constraint_type);

};

// Implementation of inline methods.
inline bool RestartSchedulerNone::notifyConflict(ConstraintType constraint_type) {
  return false;
}

}

#endif