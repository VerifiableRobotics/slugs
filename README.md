slugs - SmalL bUt Complete GROne Synthesizer
============================================
Slugs is a stand-alone reactive synthesis tool for generalized reactivity(1) synthesis. It uses binary decision diagrams (BDDs) as the primary data structure for efficient symbolic reasoning. 

If you want to cite slugs in a scientific paper, please cite its tool paper:

- Rüdiger Ehlers and Vasumathi Raman: _Slugs: Extensible GR(1) Synthesis_. 28th International Conference on Computer Aided Verification (CAV 2016), Volume 2, p.333-339

You can find an author-archived version of the paper [here](http://motesy.cs.uni-bremen.de/pdfs/cav2016.pdf). The paper has an appendix that contains an introduction to using slugs and its input language.

The slugs distribution comes with the CUDD library for manipulating binary decision diagrams (BDDs), written by Fabio Somenzi. Please see the README and LICENSE files in the `lib/cudd-3.0.0` folder for details. The _dddmp_ library for loading and saving BDDs that comes with CUDD has other licensing terms than CUDD that permit only academic and educational use. Please consult the source files in the `lib/cudd-3.0.0/dddmp` folder for details.

An introduction video to reactive synthesis with a focus on generalized reactivity(1) synthesis [here](https://www.ruediger-ehlers.de/blog/introtoreactivesynthesis.html). It also contains a Slugs tool demo.


Installation
============

Requirements
------------
- A moderately modern C++ and C compiler installed in a Unix-like environment, including the C++ library boost. Linux and MacOS should be fine.
- An installation of Python 2, version 2.7 or above. The Python _curses_ library must be installed for the interactive specification debugger to be usable.

Using Slugs on Linux
-------------------
In order to build slugs, open a terminal and type:

> cd src; make

The slugs executable will be put into the src directory.

Using slugs on OS X
-------------------
Things should generally work fine if you have a package management system (i.e. Homebrew or Macports) installed and follow the above instructions for Linux.
Note that you will need to:

- have [`gcc, g++`](https://gcc.gnu.org/) in your `$PATH`,
- have installed at least the package: `boost`.

A short primer on the internal structure of slugs
=================================================

The structure of slugs is documented in a doxygen compatible format. Internally, it uses the 'BFAbstractionLibrary' - It handles all calls to the BDD library, but allows to easily replace the BDD library actually used. For using the BFAbstractionLibrary, it suffices to include 'BF.h' - the choice of concrete BDD library used is done via preprocessor directives (see 'BF.h'). The 'BF' in 'BFAbstractionLibrary' stands for 'Boolean function'.

That library has a couple of classes:

- BF - The actual Boolean function. Supports operations like AND, OR, ..., using C++ operator overloading
- BFManager - It keeps a list of BF variables, provided TRUE or FALSE constants, and can compute variable vectors or cubes
- BFVarCube - An unsorted list of variables - used for existentially or universally quantifying out variables from BDDs
- BFVarVector - A sorted list of variables - used for replacing variables in BFs by other variables

There is also a library component for dumping BDDs - this is useful for debugging. For this to work, the application has to provide the dumping function with information about string names for BDD variable and the like. The 'Slugs' main container class inherits the class 'VariableInfoContainer' for this purpose. 

The actual synthesis part is built around a class that is named "GR1Context". It is explained in the automatically generated doxygen documentation. The class GR1Context is the main class that can be inherited for modifications of the synthesis algorithm. The 'main' function in the file 'main.cpp' is concerned with building the proper context class for synthesis and running the synthesis algorithm then.


