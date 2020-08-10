# Qute: A Dependency Learning QCDCL Solver

Qute is a solver for quantified Boolean formulas (QBFs) based on quantified conflict-driven constraint learning (QCDCL).

## Installation

Use the following sequence of commands to clone the repository and build Qute:

```
git clone https://github.com/perebor/qute.git
cd qute
git submodule init
git submodule update
mkdir build
cd build
cmake ..
make
```
Building requires **cmake** version 3.2 or newer and a C++ compiler that supports the C++14 standard.

## Usage

Qute accepts QBFs in QDIMACS or (cleansed) QCIR format.
```
qute [filename]
``` 
If no filename is given, Qute will read a formula from standard input.

By default, Qute will ignore the quantifier prefix and use a technique we call "dependency learning" to add necessary dependencies during runtime. In certain cases, this can be detrimental to performance. Dependency learning can be disabled by calling Qute with  the ```--dependency-learning off``` option.

For further options, call Qute with ```-h```.
