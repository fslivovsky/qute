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

#ifndef constraint_hh
#define constraint_hh

#include <vector>
#include <iostream>

#include "alloc.hh"
#include "solver_types.hh"

using std::ostream;

namespace Qute {

// Constraint class (clauses & terms).
struct Constraint
{
  unsigned size: 29;
  unsigned marked: 1;
  unsigned learnt: 1;
  unsigned is_reloced: 1;
  union { Literal lit; float activity; uint32_t LBD; CRef rel; uint32_t id; } data[0];

  friend class ConstraintAllocator;

  Literal& operator [] (int i) { return data[i].lit; }
  Literal operator [] (int i) const { return data[i].lit; }

  Literal* begin() { return &data[0].lit; }
  Literal* end() { return &data[size].lit; }

  bool         isMarked    ()      const   { return marked; }
  void         mark        ()              { marked = 1; }
  void         unmark      ()              { marked = 0; }
  bool         reloced     ()      const   { return is_reloced; }
  CRef         relocation  ()      const   { return data[0].rel; }
  void         relocate    (CRef cr)        { is_reloced = true; data[0].rel = cr; }
  float&       activity    ()              { return data[size].activity; }
  uint32_t&    LBD         ()              { return data[size + 1].LBD; }
  uint32_t&    id          ()              { return data[size + 2 * learnt].id; }

  Constraint(const Constraint& other, bool has_id): size(other.size), marked(false), learnt(other.learnt), is_reloced(false) {
    for (uint32_t i = 0; i < other.size; i++) {
      data[i].lit = other[i];
    }
    if (learnt) {
      data[size].activity = const_cast<Constraint&>(other).activity();
      data[size + 1].LBD = const_cast<Constraint&>(other).LBD();
    }
    if (has_id) {
      data[size + 2 * learnt].id = const_cast<Constraint&>(other).id();
    }
  }

  Constraint(const vector<Literal>& literals, bool learnt=false): size(literals.size()), marked(false), learnt(learnt), is_reloced(false) {
    for (uint32_t i = 0; i < literals.size(); i++) {
      data[i].lit = literals[i];
    }
    if (learnt) {
      data[size].activity = 0;
    }
  }
};

inline ostream& operator << (ostream& os, Constraint& constraint) {
  for (Literal l: constraint) {
    os << (sign(l) ? "" : "-") << var(l) << " ";
  }
  return os << "0";
}

// MiniSAT's Region-based Allocation.
class ConstraintAllocator
{
    RegionAllocator<uint32_t> ra;
    // If tracing is activated we need to allocate an extra 32 bits for the id.
    bool print_trace = false;
    // For learnt constraints, we additionally store both its LBD and activity.
    static uint32_t constraintWord32Size(int size, bool learnt=false, bool has_id=false) {
        return (sizeof(Constraint) + (sizeof(int32_t) * (size + 2 * learnt + has_id))) / sizeof(uint32_t); }

 public:
    enum { Unit_Size = RegionAllocator<uint32_t>::Unit_Size };

    ConstraintAllocator(uint32_t start_cap, bool print_trace): ra(start_cap), print_trace(print_trace) {}
    ConstraintAllocator(bool print_trace): print_trace(print_trace) {}

    void moveTo(ConstraintAllocator& to){
        ra.moveTo(to.ra); }

    CRef alloc(const vector<Literal>& literals, bool learnt=false)
    {
        assert(sizeof(Literal) == sizeof(uint32_t));
        CRef cid       = ra.alloc(constraintWord32Size(literals.size(), learnt, print_trace));
        new (lea(cid)) Constraint(literals, learnt);

        return cid;
    }

    CRef alloc(const Constraint& from)
    {
        CRef cid       = ra.alloc(constraintWord32Size(from.size, from.learnt, print_trace));
        new (lea(cid)) Constraint(from, print_trace);
        return cid; }

    uint32_t size      () const      { return ra.size(); }
    uint32_t wasted    () const      { return ra.wasted(); }

    // Deref, Load Effective Address (LEA), Inverse of LEA (AEL):
    Constraint&       operator[](CRef r)         { return (Constraint&)ra[r]; }
    const Constraint& operator[](CRef r) const   { return (Constraint&)ra[r]; }
    Constraint*       lea       (CRef r)         { return (Constraint*)ra.lea(r); }
    const Constraint* lea       (CRef r) const   { return (Constraint*)ra.lea(r); }
    CRef          ael       (const Constraint* t){ return ra.ael((uint32_t*)t); }

    void free(CRef cid)
    {
        Constraint& c = operator[](cid);
        ra.free(constraintWord32Size(c.size, c.learnt, print_trace));
    }

    void reloc(CRef& cr, ConstraintAllocator& to)
    {
        Constraint& c = operator[](cr);

        if (c.reloced()) { cr = c.relocation(); return; }

        cr = to.alloc(c);
        c.relocate(cr);
    }
};
}

#endif