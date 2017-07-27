#include "parser.hh"
#include <vector>
#include <algorithm>
#include <assert.h>

using namespace std;

namespace Qute {

const char QTYPE_FORALL = 'a';
const char QTYPE_EXISTS = 'e';
const char QTYPE_UNDEF = 0;

const string QDIMACS_QTYPE_FORALL = "a";
const string QDIMACS_QTYPE_EXISTS = "e";

#define QCIR_BUFFER_SIZE 8192
#define QCIR_QTYPE_COUNT 3

const string QCIR_QTYPE[QCIR_QTYPE_COUNT] = {"free", "exists", "forall"};
const char QCIR_QTYPE_MAP[QCIR_QTYPE_COUNT] = {QTYPE_EXISTS, QTYPE_EXISTS, QTYPE_FORALL};

#define QCIR_GATE_COUNT 4

const string QCIR_GATE[QCIR_GATE_COUNT] = {"and", "or", "xor", "ite"};

void Parser::readAUTO(std::istream& ifs) {
  auto first_character = ifs.peek();
  if (first_character == 'p' || first_character == 'c') {
    readQDIMACS(ifs);
  } else {
    readQCIR(ifs);
  }
}

void Parser::readQDIMACS(istream& ifs) {
  string token;

  // discard leading comment lines
  ifs >> token;
  while(token == "c"){
    getline(ifs, token);
    ifs >> token;
  }

  assert(token == "p");
  ifs >> token;
  assert(token == "cnf");

  // the declared number of clauses: when reading from the standard input, no more than this many will be read
	uint32_t num_clauses;
  // the declared bound on the number of variables
  uint32_t max_var;

	ifs >> max_var;
	ifs >> num_clauses;

	// the map that converts old variable name to the new one
  vector<Variable> var_conversion_map(max_var + 1);

	char current_qtype = QTYPE_UNDEF;
  uint32_t current_var;
  int vars_seen = 0;

	bool empty_formula = true;

	// Read the prefix.
	while(ifs >> token){
		if(token == QDIMACS_QTYPE_FORALL){
			current_qtype = QTYPE_FORALL;
		}else if(token == QDIMACS_QTYPE_EXISTS){
			current_qtype = QTYPE_EXISTS;
		}else{
			/* The prefix has ended.
			 * We have, however, already read the first literal of the first clause.
			 * Therefore, when we jump out of the loop, we must take that literal into account.
			 */
			empty_formula = false;
			break;
		}

		ifs >> current_var;
		while(current_var != 0){
      vars_seen++;
			var_conversion_map[current_var] = vars_seen;
      pcnf.addVariable(to_string(current_var) + " ", current_qtype, false);
			ifs >> current_var;
		}
	}

  // prepare the vector for the top-level term
  vector<Literal> top_level_term;

	// Handle the case of an empty formula
	if (!empty_formula) {
    int32_t literal = stoi(token);

    uint32_t clauses_seen = 0;

    // Read the matrix.
    do {
      vector<Literal> temp_clause;
      while (literal != 0){
        // Convert the read literal into the corresponding literal according to the renaming of variables.
        temp_clause.push_back(mkLiteral(var_conversion_map[abs(literal)], literal > 0));
        ifs >> literal;
      }
      clauses_seen++;
      sort(temp_clause.begin(), temp_clause.end());
      bool tautological = false;
      for (unsigned i = 1; i < temp_clause.size(); i++) {
        if (temp_clause[i] == ~temp_clause[i-1]) {
          // Tautological clause, do not add.
          tautological = true;
          break;
        }
      }
      if (!tautological) {
        pcnf.addConstraint(temp_clause, ConstraintType::clauses);
        if (!use_model_generation) {
          // add all of the Tseitin terms
          vars_seen++;
          pcnf.addVariable(to_string(max_var + clauses_seen) + " ", QTYPE_FORALL, true);
          top_level_term.push_back(mkLiteral(vars_seen, true));
          for (auto lit: temp_clause) {
            pcnf.addDependency(vars_seen, var(lit));
            vector<Literal> term{lit, mkLiteral(vars_seen, false)};
            pcnf.addConstraint(term, ConstraintType::terms);
          }
        }
      }
    } while((&ifs != &cin || clauses_seen < num_clauses) && ifs >> literal);
  }
  if (!use_model_generation) {
    pcnf.addConstraint(top_level_term, ConstraintType::terms);
  }
}

void Parser::readQCIR(istream& ifs) {
  string line;

  qcir_var_conversion_map.clear();
  nr_vars = 0;
  string qcir_output_clause_var, qcir_output_term_var;

  bool is_prefix_line;
  while (getline(ifs, line)) {
    // Remove all whitespaces.
    line.erase(remove_if(line.begin(), line.end(), ::isspace), line.end());
    // Convert to lower case.
    std::transform(line.begin(), line.end(), line.begin(), ::tolower);
    if(line.empty() || line[0] == '#')
      continue;
    is_prefix_line = false;
    for(uint32_t i = 0; i < QCIR_QTYPE_COUNT; i++) {
      if(!line.compare(0, QCIR_QTYPE[i].size(), QCIR_QTYPE[i])){
        size_t begin = line.find('(') + 1;
        size_t end = line.find(')', begin);
        string vars = line.substr(begin, end - begin);
        addQCIRVars(vars, QCIR_QTYPE_MAP[i]);
        is_prefix_line = true;
        break;
      }
    }
    if(!is_prefix_line){
      if(!line.compare(0, 6, "output")){
        uint32_t begin = line.find("(") + 1;
        string name = "";
        for(uint32_t i = begin; line[i] != ')'; i++)
          if(isQCIRNameChar(line[i]))
            name += line[i];
        qcir_output_clause_var = name + "._e";
        qcir_output_term_var = name + "._a";
      }
      else{
        uint32_t name_end = line.find("=");
        uint32_t gate_end = line.find("(", name_end);
        for(uint32_t i = 0; i < QCIR_GATE_COUNT; i++){
          if(!line.compare(gate_end - QCIR_GATE[i].size(), QCIR_GATE[i].size(), QCIR_GATE[i])){
            uint32_t begin = gate_end + 1;
            uint32_t end = line.find(")");
            addQCIRGate(line.substr(0, name_end), i, line.substr(begin, end - begin));
            break;
          }
        }
      }
    }
  }
  vector<Literal> output_clause{mkLiteral(qcir_var_conversion_map.at(qcir_output_clause_var), true)};
  vector<Literal> output_term{mkLiteral(qcir_var_conversion_map.at(qcir_output_term_var), true)};
  pcnf.addConstraint(output_clause, ConstraintType::clauses);
  pcnf.addConstraint(output_term, ConstraintType::terms);
}

void Parser::addQCIRVars(const string& vars, char qtype){
  string var_name;
  char c;
  for(uint32_t i = 0; i < vars.size(); i++){
    c = vars[i];
    if(c == ','){
      pushQCIRVar(var_name, qtype, false);
      var_name = "";
    }else if(isQCIRNameChar(c)){
      var_name += c;
    }
  }
  if(var_name.size())
    pushQCIRVar(var_name, qtype, false);
}

void Parser::addQCIRGate(string gate_name, uint32_t gate_type, string gate_data) {
  string existential_var_name = gate_name + "._e";
  string universal_var_name = gate_name + "._a";
  // Ignore duplicate gate definitions.
  if (qcir_var_conversion_map.find(existential_var_name) != qcir_var_conversion_map.end() ||
      qcir_var_conversion_map.find(universal_var_name) != qcir_var_conversion_map.end()) {
    return;
  }
  pushQCIRVar(existential_var_name, QTYPE_EXISTS, true);
  pushQCIRVar(universal_var_name, QTYPE_FORALL, true);
  int32_t gate_clause_var = nr_vars - 1;
  int32_t gate_term_var = nr_vars;
  string var_name = "";
  vector<int32_t> clause_inputs, term_inputs;
  // in order to avoid having to process last gate input separately, add ',' at the end
  gate_data += ',';
  for(uint32_t i = 0; i < gate_data.size(); i++){
    char c = gate_data[i];
    if(c == ',' && var_name.size() > 0){
      string clause_key, term_key;
      int32_t neg = 1;
      if(var_name[0] == '-'){
        clause_key = term_key = var_name.substr(1);
        neg = -1;
      }
      else
        clause_key = term_key = var_name;
      if(qcir_var_conversion_map.find(clause_key) == qcir_var_conversion_map.end()){
          clause_key += "._e";
          term_key += "._a";
      }
      clause_inputs.push_back(neg * qcir_var_conversion_map.at(clause_key));
      term_inputs.push_back(neg * qcir_var_conversion_map.at(term_key));
      var_name = "";
    }else if(isQCIRNameChar(c) || c == '-'){
      var_name += c;
    }
  }

  switch (gate_type) {
    case 0: {
      // AND gate
      for (int32_t v: clause_inputs) {
        vector<Literal> small_clause{mkLiteral(abs(v), v > 0), ~mkLiteral(gate_clause_var, true)};
        pcnf.addDependency(gate_clause_var, abs(v));
        pcnf.addConstraint(small_clause, ConstraintType::clauses);
      }
      for (int32_t v: term_inputs) {
        vector<Literal> small_term{~mkLiteral(abs(v), v > 0), mkLiteral(gate_term_var, true)};
        pcnf.addDependency(gate_term_var, abs(v));
        pcnf.addConstraint(small_term, ConstraintType::terms);
      }
      vector<Literal> big_clause;
      for (int32_t l: clause_inputs) {
        big_clause.push_back(~mkLiteral(abs(l), l > 0));
      }
      vector<Literal> big_term;
      for (int32_t l: term_inputs) {
        big_term.push_back(mkLiteral(abs(l), l > 0));
      }
      big_clause.push_back(mkLiteral(gate_clause_var, true));
      big_term.push_back(~mkLiteral(gate_term_var, true));
      pcnf.addConstraint(big_clause, ConstraintType::clauses);
      pcnf.addConstraint(big_term, ConstraintType::terms);
      break;
    }
    case 1: {
      // OR gate
      for (int32_t v : clause_inputs) {
        pcnf.addDependency(gate_clause_var, abs(v));
        vector<Literal> small_clause{~mkLiteral(abs(v), v > 0), mkLiteral(gate_clause_var, true)};
        pcnf.addConstraint(small_clause, ConstraintType::clauses);
      }
      for (int32_t v : term_inputs) {
        pcnf.addDependency(gate_term_var, abs(v));
        vector<Literal> small_term{mkLiteral(abs(v), v > 0), mkLiteral(gate_term_var, false)};
        pcnf.addConstraint(small_term, ConstraintType::terms);
      }
      vector<Literal> big_clause;
      for (int32_t l: clause_inputs) {
        big_clause.push_back(mkLiteral(abs(l), l > 0));
      }
      vector<Literal> big_term;
      for (int32_t l: term_inputs) {
        big_term.push_back(~mkLiteral(abs(l), l > 0));
      }
      big_clause.push_back(~mkLiteral(gate_clause_var, true));
      big_term.push_back(mkLiteral(gate_term_var, true));
      pcnf.addConstraint(big_clause, ConstraintType::clauses);
      pcnf.addConstraint(big_term, ConstraintType::terms);
      break;
    }
    case 2:
      // TODO support XOR GATE
      break;
    case 3:
      // TODO support ITE GATE
      break;
  }
}

void Parser::pushQCIRVar(const string& var_name, char qtype, bool auxiliary){
  assert(qcir_var_conversion_map.find(var_name) == qcir_var_conversion_map.end());
  nr_vars++;
  qcir_var_conversion_map.insert({var_name, nr_vars});
  pcnf.addVariable(var_name + " ", qtype, auxiliary);
}

bool Parser::isQCIRNameChar(char c){
  return c == '_' || ('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

char * Parser::uintToCharArray(uint32_t x){
  uint32_t num_digits = 0;
  uint32_t t = x;
  do{
    t /= 10;
    num_digits++;
  }while(t);
  char * result = new char[num_digits + 2];
  sprintf(result, "%d ", x);
  return result;
}

}
