#ifndef __EXTENSION_ANALYZE_INITIAL_POSITIONS_HPP
#define __EXTENSION_ANALYZE_INITIAL_POSITIONS_HPP

#include "gr1context.hpp"
#include <string>


/**
  * Internal class for storing information about a variable group intruduced by the structured slugs compiler
  */
class StructuredSlugsVariableGroup {
public:
    unsigned int masterVariable;
    std::vector<unsigned int> slaveVariables;
    unsigned int minValue;
    unsigned int maxValue;
public:
    StructuredSlugsVariableGroup() {
        masterVariable = (unsigned int)-1;
        minValue = (unsigned int)-1;
        maxValue = (unsigned int)-1;
    }
    void setMasterVariable(unsigned int master) {
        if (masterVariable!=(unsigned int)-1) throw "Error: Master variable declared twice. Note that in Slugs, the '@' in variable names is reserved for variables introduced by the structured slugs compiler.";
        masterVariable = master;
    }
};

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
    using T::varCubePreInput;
    using T::varCubePreOutput;

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

        // Interpret grouped variables (the ones with an '@' in their names)
        // It is assumed that they have precisely the shape produced by the structured
        // slugs parser. This is checked, however, and an exception is thrown otherwise.
        std::map<std::string,StructuredSlugsVariableGroup> variableGroups;
        std::set<unsigned int> variablesInSomeGroup;
        unsigned int varPointer = 0;
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,Pre)) {
                size_t firstAt = variableNames[i].find_first_of('@');
                if (firstAt!=std::string::npos) {
                    if (firstAt<variableNames[i].size()-1) {
                        std::string prefix = variableNames[i].substr(0,firstAt);
                        std::cerr << "Found prefix: " << prefix << std::endl;
                        // std::cerr << variableNames[i] << std::endl;
                        // There is a latter after the '@'
                        // Now check if this is the master variabe
                        if ((firstAt<variableNames[i].size()-2) && (variableNames[i][firstAt+1]=='0') && (variableNames[i][firstAt+2]=='.')) {
                            // Master variable found. Read the information from the master variable
                            variableGroups[prefix].setMasterVariable(varPointer);
                            if (variableGroups[prefix].slaveVariables.size()>0) throw "Error: in the variable groups introduced by the structured slugs parser, a master variable must be listed before its slaves.";
                            // Parse min/max information
                            std::istringstream is(variableNames[i].substr(firstAt+3,std::string::npos));
                            is >> variableGroups[prefix].minValue;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 1)";
                            char sep;
                            is >> sep;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 2)";
                            is >> variableGroups[prefix].maxValue;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 3)";
                        } else {
                            // Verify that slave variables are defined in the correct order
                            variableGroups[prefix].slaveVariables.push_back(varPointer);
                            std::istringstream is(variableNames[i].substr(firstAt+1,std::string::npos));
                            unsigned int number;
                            is >> number;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 4)";
                            if (number!=variableGroups[prefix].slaveVariables.size()) throw std::string("Error: in the variable groups introduced by the structured slugs parser, the slave variables are not in the right order: ")+variableNames[i];
                        }
                    }
                    variablesInSomeGroup.insert(i);
                }
                assert(variables[i]==preVars[varPointer]);
                varPointer++;
            }
        }


        BFMintermEnumerator enumerator(pre,preVars,postVars);
        unsigned int nofPrinted = 0;
        while ((nofPrinted++<3) && enumerator.hasNextMinterm()) {
            std::vector<int> minterm;
            enumerator.getNextMinterm(minterm);
            std::cout << "-";
            unsigned int varPointer = 0;

            // Working on the ungrouped variables
            for (unsigned int i=0;i<variables.size();i++) {
                if (doesVariableInheritType(i,Pre)) {
                    if (variablesInSomeGroup.count(i)==0) {
                        if (minterm[varPointer] == -1) {
                            std::cout << " !" << variableNames[i];
                        } else if (minterm[varPointer] == 1) {
                            std::cout << " " << variableNames[i];
                        }
                        assert(variables[i]==preVars[varPointer]);
                        varPointer++;
                    }
                }
            }

            // Working on the grouped variables
            for (auto it = variableGroups.begin();it!=variableGroups.end();it++) {
                bool foundExplicitVar = (minterm[it->second.masterVariable]!=0);
                for (auto it2=it->second.slaveVariables.begin();it2!=it->second.slaveVariables.end();it2++) {
                    foundExplicitVar |= minterm[*it2]!=0;
                }
                if (foundExplicitVar) {
                    std::cout << " " << it->first << "=";
                    int value = it->second.minValue+(minterm[it->second.masterVariable]>0?1:0);
                    int multiplyer = 2;
                    for (auto it2=it->second.slaveVariables.begin();it2!=it->second.slaveVariables.end();it2++) {
                        if (minterm[*it2]>0) value += multiplyer;
                        multiplyer *= 2;
                    }
                    std::cout << value;
                }
            }

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

        double nofInputs = mgr.constantTrue().getNofSatisfyingAssignments(varCubePreInput);
        double nofInputsSystemLosing = (initEnv & !((winningPositions & initSys).ExistAbstract(varCubePreOutput))).getNofSatisfyingAssignments(varCubePreInput);
        double nofAllowedInitialInputs = initEnv.getNofSatisfyingAssignments(varCubePreInput);

        std::cout << "Number of winning positions: " << nofWPos << " out of " << nofPos << std::endl;
        std::cout << "Number of winning positions satisfying the initialization assumptions: " << nofWPosA << " out of " << nofPosA << std::endl;
        std::cout << "Number of winning positions satisfying the initialization guarantees: " << nofWPosG << " out of " << nofPosG << std::endl;
        std::cout << "Number of winning positions satisfying the initialization assumptions and guarantees: " << nofWPosAG << " out of " << nofPosAG << std::endl;
        std::cout << "Number of initial input assignments that are allowed for the environment: " << nofAllowedInitialInputs << " out of " << nofInputs << std::endl;
        std::cout << "Number of initial input assignments that force the system to move to a losing position: " << nofInputsSystemLosing << " out of " << nofInputs << std::endl;


        std::cout << "\nSome cubes of winning positions: \n";
        printLargePreCubes(winningPositions);
        std::cout << "\nSome cubes of non-winning positions: \n";
        printLargePreCubes(!winningPositions);
        BF_newDumpDot(*this,winningPositions,NULL,"/tmp/w.dot");
        BF_newDumpDot(*this,!winningPositions,NULL,"/tmp/nw.dot");
        std::cout << "\nSome cubes of winning satisfying the initialization assumptions: \n";
        printLargePreCubes(winningPositions & initEnv);
        std::cout << "\nSome cubes of non-winning positions satisfying the initialization assumptions: \n";
        printLargePreCubes((!winningPositions) & initEnv);
        std::cout << "\nSome cubes of winning positions satisfying the initialization guarantees: \n";
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

