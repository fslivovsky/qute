/************************************************************************************************
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef solver_types_hh
#define solver_types_hh

#include <vector>
#include <functional>
#include "alloc.hh"

using std::vector;
using std::hash;

namespace Qute {

typedef RegionAllocator<uint32_t>::Ref CRef;
const CRef CRef_Undef = RegionAllocator<uint32_t>::Ref_Undef;

typedef int32_t Variable;
typedef int8_t lbool;

const lbool l_False = 0;
const lbool l_True = 1;
const lbool l_Undef = 2;

enum ConstraintType: unsigned short { clauses = 0, terms = 1 };
const vector<ConstraintType> constraint_types = {ConstraintType::clauses, ConstraintType::terms};

// MiniSAT's literal class

struct Literal {
    int x;

    // Use this as a constructor:
    //friend Literal mkLiteral(Variable var, bool sign = false);

    bool operator == (Literal p) const { return x == p.x; }
    bool operator != (Literal p) const { return x != p.x; }
    bool operator <  (Literal p) const { return x < p.x;  } // '<' makes p, ~p adjacent in the ordering.
    bool operator <= (Literal p) const { return x <= p.x;  }
};

inline  Literal  mkLiteral (Variable var, bool sign = false) { Literal p; p.x = var + var + (int)sign; return p; }
inline  Literal  operator ~(Literal p)              { Literal q; q.x = p.x ^ 1; return q; }
inline  Literal  operator ^(Literal p, bool b)      { Literal q; q.x = p.x ^ (unsigned int)b; return q; }
inline  bool sign      (Literal p)              { return p.x & 1; }
inline  int  var       (Literal p)              { return p.x >> 1; }

// Mapping Literalerals to and from compact integers suitable for array indexing:
inline  int  toInt     (Variable v)             { return v; } 
inline  int  toInt     (Literal p)              { return p.x; } 
inline  Literal  toLiteral     (int i)              { Literal p; p.x = i; return p; } 

const Literal Literal_Undef = { -2 };  // }- Useful special constants.
const Literal Literal_Error = { -1 };  // }

const int Min_Literal_Int = 2; // Smallest integer representing a literal if 1 is the smallest Variable.

// MiniSAT's trail iterator class
class TrailIterator {
    const Literal* lits;
public:
    TrailIterator(const Literal* _lits) : lits(_lits){}

    void operator++()   { lits++; }
    Literal  operator*() const { return *lits; }

    bool operator==(const TrailIterator& ti) const { return lits == ti.lits; }
    bool operator!=(const TrailIterator& ti) const { return lits != ti.lits; }
};

inline bool disablingPolarity(ConstraintType constraint_type) {
  return !constraint_type;
}

}

namespace std {

// Hash function for std::unordered_map
template<> struct hash<Qute::Literal> {
  size_t operator()(const Qute::Literal& l) const {
    return hash<int>{}(l.x);
  }
};

}

#endif