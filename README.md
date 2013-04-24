slugs - SmalL bUt Complete GROne Synthesizer
============================================
Slugs is a stand-alone reactive synthesis tool for generalized reactivity(1) synthesis.


Installation
============
Slugs is a reactive synthesis tool for the class of specification commonly referred to as reactive(1) specifications. It uses binary decision diagrams (BDDs) as the primary data structure for efficient symbolic reasoning.

Requirements
------------
- A BDD library - currently, only the library CUDD is supported
- The make utility qmake

Using CUDD on Linux
-------------------

The default BDD engine for slugs is 'cudd' by Fabio Somenzi. It can be obtained from the homepage of the author, which is 'http://vlsi.colorado.edu/~fabio/'. For using CUDD with slugs, please extract the the 'tar.gz' to the 'lib' folder. Then, you need to compile CUDD. For this, execute the following commands in the 'cudd-2.5.0' folder.

> patch Makefile ../../tools/CuddMakefile.patch

This will modify CUDD's makefile to automatically adapt to 32- or 64-bit architectures. Then, make cudd by running:

> make

Afterwards, we can compile slugs. For this, move into the src directory of slugs and hit:

> qmake Tool.pro  
> make

The slugs executable will be put into the src directory.


A short primer on the internal structure of slugs
=================================================

The structure of slugs is documented in a doxygen compatible format. Internally, it uses the 'BFAbstractionLibrary' - It handles all calls to the BDD library, but allows to easily replace the BDD library actually used. For using the BFAbstractionLibrary, it suffices to include 'BF.h' - the choice of concrete BDD library used is done via preprocessor directives (see 'BF.h'). The 'BF' in 'BFAbstractionLibrary' stands for 'Boolean function'.

That library has a couple of classes:

- BF - The actual Boolean function. Supports operations like AND, OR, ..., using C++ operator overloading
- BFManager - It keeps a list of BF variables, provided TRUE or FALSE constants, and can compute variable vectors or cubes
- BFVarCube - An unsorted list of variables - used for existentially or universally quantifying out variables from BDDs
- BFVarVector - A sorted list of variables - used for replacing variables in BFs by other variables

There is also a library component for dumping BDDs - this is useful for debugging. For this to work, the application has to provide the dumping function with information about string names for BDD variable and the like. The 'Slugs' main container class inherits the class 'VariableInfoContainer' for this purpose. 

For getting to know the structure of the actual synthesis part, the use is referred to the automatically generated doxygen documentation. The class GR1Context is the main class that can be inherited for modifications of the synthesis algorithm. The 'main' function in the file 'main.cpp' is concerned with building the proper context class for synthesis and running the synthesis algorithm then.



