# Qute: A Dependency Learning QCDCL Solver

Qute is a solver for quantified Boolean formulas (QBFs) based on quantified conflict-driven constraint learning (QCDCL).

## Building

Simply call ```cmake .``` followed by ```make``` in the Qute base directory. Qute requires [Boost](http://www.boost.org/) version 1.60 or above.

## Usage

Qute accepts QBFs in QDIMACS or (cleansed) QCIR format.
```
qute [Filename]
``` 
If no filename is given, Qute will read a formula from stdin.

By default, Qute will ignore the quantifier prefix and use a technique we call "dependency learning" to add necessary dependencies during runtime. In certain cases, this can be detrimental to performance. Dependency learning can be disabled by calling Qute with the ```--prefix-mode``` option.

For further options, call Qute with ```-h```.