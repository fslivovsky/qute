#include "parser.hh"
#include <vector>
#include <algorithm>
#include <assert.h>
#include <stdexcept>

//using namespace std;
using std::istream;

namespace Qute {

const char QTYPE_FORALL = 'a';
const char QTYPE_EXISTS = 'e';
const char QTYPE_UNDEF = 0;

const string QDIMACS_QTYPE_FORALL = "a";
const string QDIMACS_QTYPE_EXISTS = "e";

#define QCIR_BUFFER_SIZE 8192
#define QCIR_QTYPE_COUNT 3

const string QCIR_QTYPE[QCIR_QTYPE_COUNT] = {"exists", "forall", "free"};
const char QCIR_QTYPE_MAP[QCIR_QTYPE_COUNT] = {QTYPE_EXISTS, QTYPE_FORALL, QTYPE_EXISTS};

#define QCIR_GATE_COUNT 4

const string QCIR_GATE[QCIR_GATE_COUNT] = {"and", "or", "xor", "ite"};

istream& Parser::getline(istream& ifs, std::string& str) {
    ++current_line;
    return std::getline(ifs, str);
}

void Parser::readAUTO(istream& ifs) {
    int first_char = ifs.peek();
    while (isspace(first_char)) {
        ifs.get();
        first_char = ifs.peek();
    }
    if (first_char == 'p' || first_char == 'c') {
        readQDIMACS(ifs);
    } else {
        readQCIR(ifs);
    }
}

void Parser::readQDIMACS(istream& ifs) {
    string token;

    // discard leading comment lines
    ifs >> token;
    while (token == "c") {
        getline(ifs, token);
        ifs >> token;
    }

    string line;

    assert(token == "p");
    ifs >> token;
    assert(token == "cnf");

    // the declared number of clauses: when reading from the standard input, no more than this many will be read
    uint32_t num_clauses;
    // the declared bound on the number of variables
    int32_t max_var;

    ifs >> max_var;
    ifs >> num_clauses;

    pcnf.notifyMaxVarDeclaration(max_var);
    pcnf.notifyNumClausesDeclaration(num_clauses);

    // this calls the internal wrapper around std::getline, which also updates current_line
    current_line = 1;
    getline(ifs, line);

    // the map that converts old variable name to the new one
    vector<Variable> var_conversion_map(max_var+1, 0);

    char current_qtype = QTYPE_UNDEF;
    int32_t current_var;
    int vars_seen = 0;

    bool empty_formula = true;

    // Read the prefix.
    while (ifs >> token) {
        if (token == QDIMACS_QTYPE_FORALL) {
            current_qtype = QTYPE_FORALL;
        } else if (token == QDIMACS_QTYPE_EXISTS) {
            current_qtype = QTYPE_EXISTS;
        } else {
            /* The prefix has ended.
             * We have, however, already read the first literal of the first clause.
             * Therefore, when we jump out of the loop, we must take that literal into account.
             */
            empty_formula = false;
            break;
        }

        ifs >> current_var;
        while (current_var != 0) {
            vars_seen++;
            if (current_var < 0 || current_var > max_var) {
                variable_out_of_bounds_error(current_var);
            }
            if (var_conversion_map[current_var] != 0) {
              duplicate_variable_error(current_var);
            }
            var_conversion_map[current_var] = vars_seen;
            pcnf.addVariable(std::to_string(current_var), current_qtype, false);
            ifs >> current_var;
        }
        getline(ifs, line);
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
            while (literal != 0) {
                // Convert the read literal into the corresponding literal according to the renaming of variables.
                int32_t variable = abs(literal);
                if (variable > max_var) {
                    variable_out_of_bounds_error(variable);
                }
                if (var_conversion_map[variable] == 0) {
                    free_variable_error(literal);
                }
                temp_clause.push_back(mkLiteral(var_conversion_map[variable], literal > 0));
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
                    pcnf.addVariable(std::to_string(max_var + clauses_seen), QTYPE_FORALL, true);
                    top_level_term.push_back(mkLiteral(vars_seen, true));
                    for (auto lit: temp_clause) {
                        pcnf.addDependency(vars_seen, var(lit));
                        vector<Literal> term{lit, mkLiteral(vars_seen, false)};
                        pcnf.addConstraint(term, ConstraintType::terms);
                    }
                }
            }
            getline(ifs, line);
        } while ((&ifs != &std::cin || clauses_seen < num_clauses) && ifs >> literal);
    }
    if (!use_model_generation) {
        pcnf.addConstraint(top_level_term, ConstraintType::terms);
    }
}

void Parser::readQCIR(istream& ifs) {
    string line;

    qcir_var_conversion_map.clear();
    nr_vars = 0;
    current_line = 0;
    string qcir_output_clause_var = "", qcir_output_term_var = "";

    bool is_prefix_line;
    vector<string> inputs;
    while (getline(ifs, line)) {
        size_t idx = 0;
        skip_space(line, idx);
        if (idx == line.size() || line[idx] == '#') {
            continue;
        }
        string identifier = extract_next(line, idx, "(=");
        is_prefix_line = (line[idx] == '(');
        if (is_prefix_line) {
            std::transform(identifier.begin(), identifier.end(), identifier.begin(), ::tolower);
            bool is_quantifier_block = false;
            uint32_t qtype;
            for (qtype = 0; qtype < QCIR_QTYPE_COUNT; qtype++) {
                if (identifier == QCIR_QTYPE[qtype]) {
                    is_quantifier_block = true;
                    break;
                }
            }
            if (is_quantifier_block) {
                while (line[idx] != ')' && ++idx < line.size()) {
                    pushQCIRVar(extract_next(line, idx, ",)"), QCIR_QTYPE_MAP[qtype], false);
                }
                if (idx >= line.size()) {
                    unexpected_eol_error();
                }
            } else if (identifier == "output") {
                if (qcir_output_clause_var != "") {
                    duplicate_qcir_output_gate_error();
                }
                string name = extract_next(line, ++idx, ")");
                qcir_output_clause_var = name + ".e";
                qcir_output_term_var = name + ".a";
            } else {
                unknown_identifier_error(identifier);
            }
        } else {
            string gate_type = extract_next(line, ++idx, "(");
            std::transform(gate_type.begin(), gate_type.end(), gate_type.begin(), ::tolower);
            bool is_valid_gate = false;
            for (uint32_t i = 0; i < QCIR_GATE_COUNT; i++) {
                if (gate_type == QCIR_GATE[i]) {
                    inputs.clear();
                    while (line[idx] != ')' && ++idx < line.size()) {
                        inputs.push_back(extract_lit(line, idx));
                        if (inputs.back().empty()) {
                            if (inputs.size() > 1) {
                                unexpected_char_error(line[idx], idx+1);
                            } else {
                                inputs.pop_back();
                            }
                        }
                    }
                    if (idx >= line.size()) {
                        unexpected_eol_error();
                    }
                    addQCIRGate(identifier, i, inputs);
                    is_valid_gate = true;
                    break;
                }
            }
            if (!is_valid_gate) {
                invalid_gate_type_error(gate_type);
            }
        }
    }
    if (qcir_output_clause_var == "") {
        output_gate_missing_error();
    }
    vector<Literal> output_clause{mkLiteral(qcir_var_conversion_map.at(qcir_output_clause_var), true)};
    vector<Literal> output_term{mkLiteral(qcir_var_conversion_map.at(qcir_output_term_var), true)};
    pcnf.addConstraint(output_clause, ConstraintType::clauses);
    pcnf.addConstraint(output_term, ConstraintType::terms);
}

void Parser::addQCIRGate(string gate_name, uint32_t gate_type, vector<string>& inputs) {
    string existential_var_name = gate_name + ".e";
    string universal_var_name = gate_name + ".a";
    // Detect duplicate gate definitions
    if (qcir_var_conversion_map.find(gate_name) != qcir_var_conversion_map.end()) {
        duplicate_qcir_gate_error(gate_name);
    }
    if (qcir_var_conversion_map.find(existential_var_name) != qcir_var_conversion_map.end()) {
        duplicate_qcir_gate_error(gate_name);
    }
    vector<int32_t> clause_inputs, term_inputs;
    clause_inputs.reserve(inputs.size());
    term_inputs.reserve(inputs.size());
    uint32_t num_empty_inputs = 0;
    for (string& input : inputs) {
        string clause_key, term_key;
        int32_t neg_multiplier = 1;
        if (input[0] == '-') {
            neg_multiplier = -1;
            clause_key = input.substr(1);
            term_key = input.substr(1);
        } else {
            clause_key = input;
            term_key = input;
        }
        if (clause_key == "") {
            num_empty_inputs++;
        }
        if (qcir_var_conversion_map.find(clause_key) == qcir_var_conversion_map.end()) {
            clause_key += ".e";
            term_key += ".a";
        }
        try {
            clause_inputs.push_back(neg_multiplier * qcir_var_conversion_map.at(clause_key));
            term_inputs.push_back(neg_multiplier * qcir_var_conversion_map.at(term_key));
        } catch (std::out_of_range&) {
            undeclared_gate_input_literal_error(input);
        }
    }
    pushQCIRVar(existential_var_name, QTYPE_EXISTS, true);
    pushQCIRVar(universal_var_name, QTYPE_FORALL, true);
    int32_t gate_clause_var = nr_vars - 1;
    int32_t gate_term_var = nr_vars;
    for (int32_t lit : clause_inputs) {
        pcnf.addDependency(gate_clause_var, abs(lit));
    }
    for (int32_t lit : term_inputs) {
        pcnf.addDependency(gate_term_var, abs(lit));
    }
    Literal g_c = mkLiteral(gate_clause_var, true);
    Literal g_t = mkLiteral(gate_term_var, true);

    switch (gate_type) {
        case 0: {
            // AND gate
            vector<Literal> big_constraint;

            big_constraint.reserve(clause_inputs.size()+1);
            for (int32_t l: clause_inputs) {
                Literal lit = mkLiteral(abs(l), l > 0);
                vector<Literal> small_clause{lit, ~g_c};
                pcnf.addConstraint(small_clause, ConstraintType::clauses);
                big_constraint.push_back(~lit);
            }
            big_constraint.push_back(g_c);
            pcnf.addConstraint(big_constraint, ConstraintType::clauses);
            big_constraint.clear();

            big_constraint.reserve(term_inputs.size()+1);
            for (int32_t l: term_inputs) {
                Literal lit = mkLiteral(abs(l), l > 0);
                vector<Literal> small_term{~lit, g_t};
                pcnf.addConstraint(small_term, ConstraintType::terms);
                big_constraint.push_back(lit);
            }
            big_constraint.push_back(~g_t);
            pcnf.addConstraint(big_constraint, ConstraintType::terms);
            break;
        }
        case 1: {
            // OR gate
            vector<Literal> big_constraint;

            big_constraint.reserve(clause_inputs.size()+1);
            for (int32_t l: clause_inputs) {
                Literal lit = mkLiteral(abs(l), l > 0);
                vector<Literal> small_clause{~lit, g_c};
                pcnf.addConstraint(small_clause, ConstraintType::clauses);
                big_constraint.push_back(lit);
            }
            big_constraint.push_back(~g_c);
            pcnf.addConstraint(big_constraint, ConstraintType::clauses);
            big_constraint.clear();

            big_constraint.reserve(term_inputs.size()+1);
            for (int32_t l: term_inputs) {
                Literal lit = mkLiteral(abs(l), l > 0);
                vector<Literal> small_term{lit, ~g_t};
                pcnf.addConstraint(small_term, ConstraintType::terms);
                big_constraint.push_back(~lit);
            }
            big_constraint.push_back(g_t);
            pcnf.addConstraint(big_constraint, ConstraintType::terms);
            break;
        }
        case 2: {
			// XOR gate
            if (clause_inputs.size() != 2) {
                invalid_xor_gate_size_error();
            }
            Literal x = mkLiteral(abs(clause_inputs[0]), clause_inputs[0] > 0);
            Literal y = mkLiteral(abs(clause_inputs[1]), clause_inputs[1] > 0);
            vector<vector<Literal>> clauses = {
                {~g_c, ~x, ~y},
                {~g_c,  x,  y},
                { g_c, ~x,  y},
                { g_c,  x, ~y}
            };
            for (vector<Literal>& c : clauses) {
                pcnf.addConstraint(c, ConstraintType::clauses);
            }
            x = mkLiteral(abs(term_inputs[0]), term_inputs[0] > 0);
            y = mkLiteral(abs(term_inputs[1]), term_inputs[1] > 0);
            vector<vector<Literal>> terms = {
                { g_t,  x,  y},
                { g_t, ~x, ~y},
                {~g_t,  x, ~y},
                {~g_t, ~x,  y}
            };
            for (vector<Literal>& t : terms) {
                pcnf.addConstraint(t, ConstraintType::terms);
            }
            break;
        }
        case 3: {
			// ITE gate
            if (clause_inputs.size() != 3) {
                invalid_ite_gate_size_error();
            }
            Literal lit_cond = mkLiteral(abs(clause_inputs[0]), clause_inputs[0] > 0);
            Literal lit_then = mkLiteral(abs(clause_inputs[1]), clause_inputs[1] > 0);
            Literal lit_else = mkLiteral(abs(clause_inputs[2]), clause_inputs[2] > 0);
            vector<vector<Literal>> clauses = {
                {~g_c, ~lit_cond,  lit_then},
                {~g_c,  lit_cond,  lit_else},
                { g_c, ~lit_cond, ~lit_then},
                { g_c,  lit_cond, ~lit_else}
            };
            for (vector<Literal>& c : clauses) {
                pcnf.addConstraint(c, ConstraintType::clauses);
            }
            lit_cond = mkLiteral(abs(term_inputs[0]), term_inputs[0] > 0);
            lit_then = mkLiteral(abs(term_inputs[1]), term_inputs[1] > 0);
            lit_else = mkLiteral(abs(term_inputs[2]), term_inputs[2] > 0);
            vector<vector<Literal>> terms = {
                { g_t,  lit_cond, ~lit_then},
                { g_t, ~lit_cond, ~lit_else},
                {~g_t,  lit_cond,  lit_then},
                {~g_t, ~lit_cond,  lit_else}
            };
            for (vector<Literal>& t : terms) {
                pcnf.addConstraint(t, ConstraintType::terms);
            }
            break;
        }
    }
}

void Parser::pushQCIRVar(const string& var_name, char qtype, bool auxiliary) {
    assert(qcir_var_conversion_map.find(var_name) == qcir_var_conversion_map.end());
    nr_vars++;
    qcir_var_conversion_map.insert({var_name, nr_vars});
    pcnf.addVariable(var_name, qtype, auxiliary);
}

char * Parser::uintToCharArray(uint32_t x) {
    uint32_t num_digits = 0;
    uint32_t t = x;
    do {
        t /= 10;
        num_digits++;
    } while(t);
    char * result = new char[num_digits + 2];
    sprintf(result, "%d ", x);
    return result;
}

}
