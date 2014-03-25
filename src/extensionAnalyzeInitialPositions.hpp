#ifndef __EXTENSION_ANALYZE_INITIAL_POSITIONS_HPP
#define __EXTENSION_ANALYZE_INITIAL_POSITIONS_HPP

#include "gr1context.hpp"
#include <string>

/**
 * A class that checks realizability and then analyzes the winning positions.
 */
template<class T> class XAnalyzeInitialPositions : public T {
protected:
    using T::checkRealizability;
    using T::varCubePre;
    using T::winningPositions;
    using T::initSys;
    using T::initEnv;
    using T::mgr;
    using T::preVars;
    using T::postVars;
    using T::variables;
    using T::variableNames;
    using T::doesVariableInheritType;

    // Constructor
    XAnalyzeInitialPositions<T>(std::list<std::string> &filenames) : T(filenames) {}

    void printLargePreCubes(BF pre) {

        if (pre==mgr.constantTrue()) {
            std::cout << "- TRUE\n";
            return;
        } else if (pre==mgr.constantFalse()) {
            std::cout << "--(none)\n";
            return;
        }

        BFMintermEnumerator enumerator(pre,preVars,postVars);
        unsigned int nofPrinted = 0;
        while ((nofPrinted++<3) && enumerator.hasNextMinterm()) {
            std::vector<int> minterm;
            enumerator.getNextMinterm(minterm);
            std::cout << "-";
            unsigned int varPointer = 0;
            for (unsigned int i=0;i<variables.size();i++) {
                if (doesVariableInheritType(i,Pre)) {
                    if (minterm[varPointer] == -1) {
                        std::cout << " !" << variableNames[i];
                    } else if (minterm[varPointer] == 1) {
                        std::cout << " " << variableNames[i];
                    }
                    assert(variables[i]==preVars[varPointer]);
                    varPointer++;
                }
            }
            assert(varPointer==preVars.size());
            std::cout << std::endl;
        }
    }

public:

    void execute() {
        checkRealizability();

        double nofPos = mgr.constantTrue().getNofSatisfyingAssignments(varCubePre);
        double nofPosA = initEnv.getNofSatisfyingAssignments(varCubePre);
        double nofPosG = initSys.getNofSatisfyingAssignments(varCubePre);
        double nofPosAG = (initEnv & initSys).getNofSatisfyingAssignments(varCubePre);

        double nofWPos = winningPositions.getNofSatisfyingAssignments(varCubePre);
        double nofWPosA = (initEnv & winningPositions).getNofSatisfyingAssignments(varCubePre);
        double nofWPosG = (initSys & winningPositions).getNofSatisfyingAssignments(varCubePre);
        double nofWPosAG = (initEnv & initSys & winningPositions).getNofSatisfyingAssignments(varCubePre);

        std::cout << "Number of winning positions: " << nofWPos << " out of " << nofPos << std::endl;
        std::cout << "Number of winning positions satisfying the initialization assumptions: " << nofWPosA << " out of " << nofPosA << std::endl;
        std::cout << "Number of winning positions satisfying the initialization guarantees: " << nofWPosG << " out of " << nofPosG << std::endl;
        std::cout << "Number of winning positions satisfying the initialization assumptions and guarantees: " << nofWPosAG << " out of " << nofPosAG << std::endl;

        std::cout << "\nSome cubes of winning positions: \n";
        printLargePreCubes(winningPositions);
        std::cout << "\nSome cubes of non-winning positions: \n";
        printLargePreCubes(!winningPositions);
        std::cout << "\nSome cubes of winning satisfying the initialization assumptions: \n";
        printLargePreCubes(winningPositions & initEnv);
        std::cout << "\nSome cubes of non-winning positions satisfying the initialization assumptions: \n";
        printLargePreCubes((!winningPositions) & initEnv);
        std::cout << "\nSome cubes of winning satisfying the initialization guarantees: \n";
        printLargePreCubes(winningPositions & initSys);
        std::cout << "\nSome cubes of non-winning positions satisfying the initialization guarantees: \n";
        printLargePreCubes((!winningPositions) & initSys);
        std::cout << "\nSome cubes of winning positions satisfying the initialization assumptions & guarantees: \n";
        printLargePreCubes(winningPositions & initEnv & initSys);
        std::cout << "\nSome cubes of non-winning positions satisfying the initialization assumptions & guarantees: \n";
        printLargePreCubes((!winningPositions) & initEnv & initSys);
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XAnalyzeInitialPositions<T>(filenames);
    }
};




#endif

