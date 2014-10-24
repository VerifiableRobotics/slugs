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

Using slugs on OS X
-------------------
Things should generally work fine if you have a package management system (i.e. Homebrew or Macports) installed and follow the above instructions for Linux.
Note that you will need to:

- have [`gcc, g++`](https://gcc.gnu.org/) in your `$PATH`,
- have installed at least the following packages: `qt`, `boost`.

In order to get CUDD to compile, you'll need to apply a small patch.  Inside the 'cudd-2.5.0' folder, run:

> patch -p0 < ../../tools/CuddOSXFixes.patch

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


Developing an extension for slugs
=================================================
The main approach for extending slugs is by deriving from the GR1Context class, and overwriting some of its functions. We however want to plug together a set of extensions at runtime. Standard inheritance is not flexible for this. In fact, we want to daisy-chain extensions whenever this makes sense, and prevent the user from daisy-chaining the extensions into meaningless combinations. At the same time, we do not want to the clutter the basic synthesis algorithm implementation, so that it stays understandable and easy to extend.

The way in which this is done in slugs is by *template-based* inheritance. It allows to statically compile implementations for different sets of extensions. The following steps have to be performed for implementing a new extension:

1. Add a new header file whose name should start with "extension" and should end with ".hpp" to the project. Add it to the list of header files in the file Tool.pro.
2. Implement the header file. It should start and end with the standard C preprocessor directives that make sure that include the file twice is no problem. Then, it should include "gr1context.hpp". Then, start with the following template for the template:
```c++
    template<class T> class XYourExtension : public T {
    protected:

        // Inherited stuff used


        // Constructor
        XYourExtension<T>(std::list<std::string> &filenames) : T(filenames) {}
      
    public:
      
        static GR1Context* makeInstance(std::list<std::string> &filenames) {
            return new XYourExtension<T>(filenames);
        }
    };
```
Replace "XYourExtension" by the name of your extension. Please let its name start with "X" (naming convention). The template can derive from an arbitrary class, namely class "T". This ensures that we can daisy-chain extensions statically. For example, "XExtractStrategy<XRoboticsSemantics<GR1Context> >" is a valid class in slugs that combines the extensions for extracting a strategy and using the special robotics semantics for initialization contraints. You can then overwrite methods from the GR1Context to implement your derived extension. Note that you will need to put under the line "// Inherited stuff used" some directives that give you access to the variables and methods from GR1Context that you use (e.g., "using T::mgr;"). This comes from the multi-stage compilation process in C++, and then the compiler basically deferres checking if variables are accessible to the stage in which the code for the template is actually implemented. The file "extensionBiasForAction.hpp" contains a nice overall example. Note that forgetting the "using" statement will result in strange compilation errors that are hard to interpret.

Now we have to tell slugs that the new extension is avaiable. This is done by modifying the file "main.cpp". First, add an "#include" line that makes sure that the header file for your new extension is found. Then, you need to modify two arrays - "commandLineArguments" and "optionCombinations". Most extensions will add a pair of entries to the first of these arrays, namely a new command line parameter and a description of the parameter. The description is shown when launching slugs with "--help" or without valid parameter combinations. The second step is to extend the array "optionCombinations" by those combinations of options to slugs that represent valid combinations of extensions. Please document the requirements to how your extension is combined with other extensions in the comment box above the list of option combinations. Then add all option combinations that involve your new extension and make sense. The list of command line options must always be lexicographically ordered - otherwise, the new comination is not found at runtime. Right of the option list, a pointer to a factory function for the combination must be given. For this, we added the function "makeInstance" to our new extension.
