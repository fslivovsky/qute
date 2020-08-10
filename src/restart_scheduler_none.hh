#ifndef restart_scheduler_none_hh
#define restart_scheduler_none_hh

#include "solver_types.hh"
#include "restart_scheduler.hh"

namespace Qute {

class RestartSchedulerNone: public RestartScheduler {

public:
  virtual void notifyConflict(ConstraintType constraint_type);
  virtual void notifyLearned(Constraint& c);
  virtual bool restart();

};

// Implementation of inline methods.
inline void RestartSchedulerNone::notifyConflict(ConstraintType constraint_type) {}

inline void RestartSchedulerNone::notifyLearned(Constraint& c) {}

inline bool RestartSchedulerNone::restart() {
  return false;
}

}

#endif