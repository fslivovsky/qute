#ifndef restart_scheduler_inner_outer_hh
#define restart_scheduler_inner_outer_hh

#include "restart_scheduler.hh"
#include "logging.hh"

namespace Qute {

class RestartSchedulerInnerOuter: public RestartScheduler {

public:
  RestartSchedulerInnerOuter(uint32_t inner_restart_limit, uint32_t outer_restart_limit, double restart_multiplier): inner_restart_limit(inner_restart_limit), outer_restart_limit(outer_restart_limit), restart_multiplier(restart_multiplier), conflict_counter(0), current_inner_restart_limit(inner_restart_limit), restart_flag(false) {}
  virtual void notifyConflict(ConstraintType constraint_type);
  virtual void notifyLearned(Constraint& c);
  virtual bool restart();
  
protected:
  uint32_t inner_restart_limit;
  uint32_t outer_restart_limit;
  double restart_multiplier;
  uint32_t conflict_counter;
  uint32_t current_inner_restart_limit;
  bool restart_flag;

};

// Implementation of inline method.
void RestartSchedulerInnerOuter::notifyConflict(ConstraintType constraint_type) {
  conflict_counter++;
  if (conflict_counter >= current_inner_restart_limit) {
    LOG(info) << "Restarting after " << conflict_counter << " conflicts/solutions. " << std::endl;
    conflict_counter = 0;
    if (current_inner_restart_limit >= outer_restart_limit) {
      LOG(info) << "Outer restart."<< std::endl;
      outer_restart_limit *= restart_multiplier;
      current_inner_restart_limit = inner_restart_limit;
    } else {
      current_inner_restart_limit *= restart_multiplier;
    }
    restart_flag = true;
    return;
  }
}

inline void RestartSchedulerInnerOuter::notifyLearned(Constraint& c) {}

inline bool RestartSchedulerInnerOuter::restart() {
  if (restart_flag) {
    restart_flag = false;
    return true;
  } else {
    return false;
  }
}

}

#endif