#ifndef parser_hh
#define parser_hh

#include <istream>
#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include "pcnf_container.hh"
#include "solver_types.hh"

namespace Qute {

class Parser
{
  PCNFContainer& pcnf;
  bool use_model_generation;
  std::map<std::string, int32_t> qcir_var_conversion_map;
  int32_t nr_vars;

  // helper methods
  char* uintToCharArray(uint32_t x);
  void addQCIRVars(const std::string& vars, char qtype);
  void pushQCIRVar(const std::string& var_name, char qtype, bool auxiliary);
  void addQCIRGate(std::string gate_name, uint32_t gate_type, std::string gate_data);
  bool isQCIRNameChar(char c);

public:
  Parser(PCNFContainer& pcnf, bool use_model_generation): pcnf(pcnf), use_model_generation(use_model_generation) {}

  // IO methods
  void readAUTO(std::istream& ifs = std::cin);
  void readQCIR(std::istream& ifs = std::cin);
  void readQDIMACS(std::istream& ifs = std::cin);
  void writeQDIMACS();
};

}

#endif
