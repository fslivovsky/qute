#ifndef docopt_test_hh
#define docopt_test_hh

#include <docopt.h>
#include <docopt_util.h>
#include <algorithm>
#include <regex>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <memory>

using std::string;
using std::map;
using std::stoi;
using std::regex;
using std::regex_match;
using std::vector;
using std::unique_ptr;
using std::make_unique;
using std::find;

class ArgumentConstraint {
public:
  virtual ~ArgumentConstraint() {}
  virtual bool check(map<string, docopt::value> args) = 0;
  virtual string message() = 0;
};

class IfThenConstraint: public ArgumentConstraint {
public:
  IfThenConstraint(string first_parameter_name, string first_parameter_value, string second_parameter_name, string second_parameter_value, string error_message): first_parameter_name(first_parameter_name), first_parameter_value(first_parameter_value), second_parameter_name(second_parameter_name), second_parameter_value(second_parameter_value), error_message(error_message) {}

  virtual bool check(map<string, docopt::value> args) {
    return (!args[first_parameter_name] || args[first_parameter_name].asString() != first_parameter_value) || (args[second_parameter_name] && args[second_parameter_name].asString() == second_parameter_value);
  }

  virtual string message() {
    return "ERROR: " + error_message;
  }

protected:
  string first_parameter_name, first_parameter_value, second_parameter_name, second_parameter_value, error_message;

};

class ListConstraint: public ArgumentConstraint {
public:
  ListConstraint(vector<string> legal_values, string parameter_name): parameter_name(parameter_name), legal_values(legal_values) {}

  virtual bool check(map<string, docopt::value> args) {
    return !args[parameter_name] || (find(legal_values.begin(), legal_values.end(), args[parameter_name].asString()) != legal_values.end());
  }

  virtual string message() {
    string values_string = join<vector<string>::iterator>(legal_values.begin(), legal_values.end(), ", ");
    return "ERROR: " + parameter_name + " must be one of (" + values_string + ")";
  }

protected:
  string parameter_name;
  vector<string> legal_values;
};

class RegexArgumentConstraint: public ArgumentConstraint {
public:
  RegexArgumentConstraint(regex r, string parameter_name, string required_type_name): r(r), parameter_name(parameter_name), required_type_name(required_type_name) {}
  
  virtual bool check(map<string, docopt::value> args) {
    return !args[parameter_name] || regex_match(args[parameter_name].asString(), r);
  }

  virtual string message() {
    return "ERROR: " + parameter_name + " must have type " + required_type_name;
  }

protected:
  regex r;
  string parameter_name;
  string required_type_name;

};

class DoubleConstraint: public ArgumentConstraint {
public:
  DoubleConstraint(string parameter_name): parameter_name(parameter_name) {}

  virtual bool check(map<string, docopt::value> args) {
    if (!args[parameter_name]) {
      return true;
    }
    try {
      std::stod(args[parameter_name].asString());
      return true;
    } catch (...) {
      return false;
    }
  }

  virtual string message() {
    return "ERROR: " + parameter_name + " must have type double";
  }

protected:
  string parameter_name;
};

template <class T> class RangeConstraint: public ArgumentConstraint {
public:
  RangeConstraint(const T& lower, const T& upper, string parameter_name, bool lower_open=false, bool upper_open=false): lower(lower), upper(upper), lower_open(lower_open), upper_open(upper_open), parameter_name(parameter_name) {}
  
  virtual bool check(map<string, docopt::value> args) = 0;

  virtual string message() {
    return "ERROR: " + parameter_name + " must be in the range " + (lower_open ? "(": "[") + std::to_string(lower) + ", " + std::to_string(upper) + (upper_open ? ")": "]");
  }

protected:
  const T lower, upper;
  bool lower_open, upper_open;
  string parameter_name;

};

class DoubleRangeConstraint: public RangeConstraint<double> {
public:
  DoubleRangeConstraint(const double& lower, const double& upper, string parameter_name, bool lower_open=false, bool upper_open=false): RangeConstraint(lower, upper, parameter_name, lower_open, upper_open) {}
  virtual bool check(map<string, docopt::value> args) {
    if (!args[parameter_name]) {
      return true;
    }
    try {
      double value = std::stod(args[parameter_name].asString());
      return ((lower_open && lower < value) || lower <= value) && ((upper_open && upper > value) || upper >= value);
    } catch (...) {
      return false;
    }
  }

};

#endif