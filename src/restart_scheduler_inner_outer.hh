#ifndef restart_scheduler_inner_outer_hh
#define restart_scheduler_inner_outer_hh

#include <boost/log/trivial.hpp>
#include "restart_scheduler.hh"

namespace Qute {

class RestartSchedulerInnerOuter: public RestartScheduler {

public:
  RestartSchedulerInnerOuter(uint32_t inner_restart_limit, uint32_t outer_restart_limit, double restart_multiplier): inner_restart_limit(inner_restart_limit), outer_restart_limit(outer_restart_limit), restart_multiplier(restart_multiplier), conflict_counter(0), current_inner_restart_limit(inner_restart_limit) {}
  virtual bool notifyConflict(ConstraintType constraint_type);
  
protected:
  uint32_t inner_restart_limit;
  uint32_t outer_restart_limit;
  double restart_multiplier;
  uint32_t conflict_counter;
  uint32_t current_inner_restart_limit;

};

// Implementation of inline method.
bool RestartSchedulerInnerOuter::notifyConflict(ConstraintType constraint_type) {
  conflict_counter++;
  if (conflict_counter >= current_inner_restart_limit) {
    BOOST_LOG_TRIVIAL(info) << "Restarting after " << conflict_counter << " conflicts. ";
    conflict_counter = 0;
    if (current_inner_restart_limit >= outer_restart_limit) {
      BOOST_LOG_TRIVIAL(info) << "Outer restart.";
      outer_restart_limit *= restart_multiplier;
      current_inner_restart_limit = inner_restart_limit;
    } else {
      current_inner_restart_limit *= restart_multiplier;
    }
    return true;
  }
  return false;
}

}

#endif