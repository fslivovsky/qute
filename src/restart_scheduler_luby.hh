#ifndef restart_scheduler_luby_hh
#define restart_scheduler_luby_hh

#include "restart_scheduler.hh"
#include "logging.hh"

namespace Qute {

class RestartSchedulerLuby: public RestartScheduler {

public:
  RestartSchedulerLuby(int multiplier): u(1), v(1), multiplier(multiplier), conflict_counter(0), limit(multiplier), restart_flag(false) {}
  virtual void notifyConflict(ConstraintType constraint_type);
  virtual void notifyLearned(Constraint& c);
  virtual bool restart();
  
protected:

  void nextLuby();

  int u, v;

  int multiplier;
  int conflict_counter;
  int limit;
  bool restart_flag;
};

void RestartSchedulerLuby::notifyConflict(ConstraintType constraint_type) {
  conflict_counter++;
  if (conflict_counter >= limit) {
    restart_flag = true;
  }
}

inline void RestartSchedulerLuby::notifyLearned(Constraint& c) {}

inline bool RestartSchedulerLuby::restart() {
  if (restart_flag) {
    LOG(info) << "Restarting after " << conflict_counter << " conflicts/solutions. " << std::endl;
    restart_flag = false;
    conflict_counter = 0;
    nextLuby();
    limit = multiplier * v;
    return true;
  } else {
    return false;
  }
}

inline void RestartSchedulerLuby::nextLuby() {
  if ((u & -u) == v) {
    u = u + 1;
    v = 1;
  } else {
    v = 2 * v;
  }
}

}

#endif