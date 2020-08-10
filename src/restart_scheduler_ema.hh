#ifndef restart_scheduler_ema_hh
#define restart_scheduler_ema_hh

#include <algorithm>
#include <cmath>
#include "solver_types.hh"
#include "restart_scheduler.hh"
#include "logging.hh"

using std::max;
using std::log2;
using std::pow;

namespace Qute {

class RestartSchedulerEMA: public RestartScheduler {

public:
  RestartSchedulerEMA(double alpha, uint32_t minimum_distance, double threshold_factor): alpha(alpha), ema_long_term{0, 0}, ema_short_term{0, 0}, threshold_factor(threshold_factor), minimum_distance(minimum_distance), conflict_counter(0), restart_flag(false), nr_updates(0) {}
  virtual void notifyConflict(ConstraintType constraint_type);
  virtual void notifyLearned(Constraint& c);
  virtual bool restart();

protected:
  double alpha;
  double ema_long_term[2];
  double ema_short_term[2];
  double threshold_factor;
  uint32_t minimum_distance;
  uint32_t conflict_counter;
  bool restart_flag;
  uint32_t nr_updates;
  ConstraintType conflict_constraint_type;

};

// Implementation of inline methods.
inline void RestartSchedulerEMA::notifyConflict(ConstraintType constraint_type) {
  conflict_counter++;
  conflict_constraint_type = constraint_type;
}

inline void RestartSchedulerEMA::notifyLearned(Constraint& c) {
  bool index = (conflict_constraint_type == ConstraintType::terms);
  double long_term_delta = (c.LBD() - ema_long_term[index]) / (1 + nr_updates);
  ema_long_term[index] = ema_long_term[index] + long_term_delta;
  double alpha_smoothed = (nr_updates > -log2(alpha)) ? alpha : 1 / pow(2, nr_updates);
  ema_short_term[index] = c.LBD() * alpha_smoothed + ema_short_term[index] * (1 - alpha_smoothed);
  nr_updates++;
  if (ema_short_term[index] > ema_long_term[index] * threshold_factor && conflict_counter >= minimum_distance) {
    restart_flag = true;
  }
}

inline bool RestartSchedulerEMA::restart() {
  if (restart_flag) {
    LOG(info) << "Restarting after " << conflict_counter << " conflicts/solutions. " << std::endl;
    restart_flag = false;
    conflict_counter = 0;
    return true;
  } else {
    return false;
  }
}

}

#endif