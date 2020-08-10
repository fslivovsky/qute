#ifndef logging_hh
#define logging_hh

#include <iostream>
#include <memory>

#ifdef NO_LOGGING
#define LOG(ARG) if (0) std::cerr
#else
#define LOG(ARG) Logger::get().setMessageLevel(Loglevel::ARG)
#endif

namespace Qute {

enum class Loglevel: char {trace=1, info=2, error=3}; 

class StreamSink: public std::ostream, std::streambuf {
public:
  StreamSink(): std::ostream(this), output_level(Loglevel::error) {}

  int overflow(int c)
  {
    conditionalPut(c);
    return 0;
  }

  StreamSink& setMessageLevel(Loglevel level) {
    current_message_level = level;
    return (*this);
  }

  void setOutputLevel(Loglevel level) {
    output_level = level;
  }

  void conditionalPut(char c)
  {
    if (current_message_level >= output_level) {
      std::cerr.put(c);
    }
  }

protected:
  Loglevel output_level, current_message_level;
};

class Logger
{
public:
  static StreamSink& get()
  {
    static StreamSink instance;
    return instance;
  }
private:
  Logger() {}
public:
  Logger(Logger const&) = delete;
  void operator=(Logger const&) = delete;
};

}

#endif