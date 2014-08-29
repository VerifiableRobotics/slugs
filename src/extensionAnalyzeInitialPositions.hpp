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
template<class T, bool restrictToReachablePositions> class XAnalyzeInitialPositions : public T {
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
    using T::varCubePostInput;
    using T::safetyEnv;
    using T::safetySys;
    using T::lineNumberCurrentlyRead;
    using T::addVariable;
    using T::parseBooleanFormula;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::varVectorPre;
    using T::varVectorPost;

    // Constructor
    XAnalyzeInitialPositions<T,restrictToReachablePositions>(std::list<std::string> &filenames) : T(filenames) {}

    void printLargePreCubes(BF pre, BF careSet) {

        if (pre>=careSet) {
            std::cout << "- TRUE\n";
            return;
        } else if ((pre & careSet)==mgr.constantFalse()) {
            std::cout << "--(none)\n";
            return;
        }

        pre |= !careSet;

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

        // Now enumerate cubes. Due to the fact that different binary
        // cubes could have a/the same interpretation with grouped variables,
        // we store lines already given to the user in order to prevent
        // duplication. While this may lead to missing some configurations with
        // different numbers, we get a bit of variety in the cubes in this way.
        BFMintermEnumerator enumerator(pre,careSet,preVars,postVars);
        std::set<std::string> linesProducedAlready;
        unsigned int nofPrinted = 0;
        while ((nofPrinted<7) && enumerator.hasNextMinterm()) {
            std::vector<int> minterm;
            enumerator.getNextMinterm(minterm);
            std::ostringstream thisLine;
            thisLine << " -";
            unsigned int varPointer = 0;

            // Working on the ungrouped variables
            for (unsigned int i=0;i<variables.size();i++) {
                if (doesVariableInheritType(i,Pre)) {
                    if (variablesInSomeGroup.count(i)==0) {
                        if (minterm[varPointer] == -1) {
                            thisLine << " !" << variableNames[i];
                        } else if (minterm[varPointer] == 1) {
                            thisLine << " " << variableNames[i];
                        }
                    }
                    assert(variables[i]==preVars[varPointer]);
                    varPointer++;
                }
            }

            // Working on the grouped variables
            for (auto it = variableGroups.begin();it!=variableGroups.end();it++) {
                bool foundExplicitVar = (minterm[it->second.masterVariable]!=0);
                for (auto it2=it->second.slaveVariables.begin();it2!=it->second.slaveVariables.end();it2++) {
                    foundExplicitVar |= minterm[*it2]!=0;
                }
                if (foundExplicitVar) {
                    thisLine << " " << it->first << "=";
                    int value = it->second.minValue+(minterm[it->second.masterVariable]>0?1:0);
                    int multiplyer = 2;
                    for (auto it2=it->second.slaveVariables.begin();it2!=it->second.slaveVariables.end();it2++) {
                        if (minterm[*it2]>0) value += multiplyer;
                        multiplyer *= 2;
                    }
                    thisLine << value;
                }
            }

            thisLine << std::endl;

            if (linesProducedAlready.count(thisLine.str())==0) {
                linesProducedAlready.insert(thisLine.str());
                std::cout << thisLine.str();
                nofPrinted++;
            } else {
                // Skip to the next one!
            }
        }
    }


    // BFs into which variable limits are encoded if they are explicitly marked as such in the input file.
    // This helps to produce meaningful cubes.
    BF variableLimits;

public:

    void execute() {
        checkRealizability();

        if (restrictToReachablePositions) {
            BF reachable = initEnv & initSys;
            BF combined = safetyEnv & safetySys;
            std::cerr << "...computing assumptions...\n";
            BF reachableStates = initSys & initEnv;
            BF oldReachableStates = mgr.constantFalse();
            while (reachableStates!=oldReachableStates) {
                oldReachableStates = reachableStates;
                reachableStates |= (reachableStates & combined).ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);
            }
            variableLimits &= reachableStates;
        }

        double nofPos = variableLimits.getNofSatisfyingAssignments(varCubePre);
        double nofPosA = (initEnv & variableLimits).getNofSatisfyingAssignments(varCubePre);
        double nofPosG = (initSys & variableLimits).getNofSatisfyingAssignments(varCubePre);
        double nofPosAG = (initEnv & initSys & variableLimits).getNofSatisfyingAssignments(varCubePre);

        double nofWPos = (winningPositions & variableLimits).getNofSatisfyingAssignments(varCubePre);
        double nofWPosA = (initEnv & winningPositions & variableLimits).getNofSatisfyingAssignments(varCubePre);
        double nofWPosG = (initSys & winningPositions & variableLimits).getNofSatisfyingAssignments(varCubePre);
        double nofWPosAG = (initEnv & initSys & winningPositions & variableLimits).getNofSatisfyingAssignments(varCubePre);

        double nofInputs = variableLimits.ExistAbstract(varCubePreOutput).getNofSatisfyingAssignments(varCubePreInput);
        double nofInputsSystemLosing = (initEnv & variableLimits & !((winningPositions & initSys).ExistAbstract(varCubePreOutput))).getNofSatisfyingAssignments(varCubePreInput);
        double nofAllowedInitialInputs = (initEnv & variableLimits.ExistAbstract(varCubePreOutput)).getNofSatisfyingAssignments(varCubePreInput);

        std::cout << "Number of winning positions: " << nofWPos << " out of " << nofPos << std::endl;
        std::cout << "Number of winning positions satisfying the initialization assumptions: " << nofWPosA << " out of " << nofPosA << std::endl;
        std::cout << "Number of winning positions satisfying the initialization guarantees: " << nofWPosG << " out of " << nofPosG << std::endl;
        std::cout << "Number of winning positions satisfying the initialization assumptions and guarantees: " << nofWPosAG << " out of " << nofPosAG << std::endl;
        std::cout << "Number of initial input assignments that are allowed for the environment: " << nofAllowedInitialInputs << " out of " << nofInputs << std::endl;
        std::cout << "Number of initial input assignments that force the system to move to a losing position: " << nofInputsSystemLosing << " out of " << nofInputs << std::endl;

        std::cout << "\nSome cubes of winning positions: \n";
        printLargePreCubes(winningPositions, variableLimits);
        BF_newDumpDot(*this,winningPositions,"PreOutput PreInput","/tmp/winningInitialPositions.dot");
        BF_newDumpDot(*this,variableLimits,"PreOutput PreInput","/tmp/variableLimits.dot");
        std::cout << "\nSome cubes of non-winning positions: \n";
        printLargePreCubes(!winningPositions, variableLimits);
        BF_newDumpDot(*this,winningPositions,NULL,"/tmp/w.dot");
        BF_newDumpDot(*this,!winningPositions,NULL,"/tmp/nw.dot");
        std::cout << "\nSome cubes of winning positions satisfying the initialization assumptions: \n";
        printLargePreCubes(winningPositions & initEnv, variableLimits);
        std::cout << "\nSome cubes of non-winning positions satisfying the initialization assumptions: \n";
        printLargePreCubes((!winningPositions) & initEnv, variableLimits);
        std::cout << "\nSome cubes of winning positions satisfying the initialization guarantees: \n";
        printLargePreCubes(winningPositions & initSys, variableLimits);
        std::cout << "\nSome cubes of non-winning positions satisfying the initialization guarantees: \n";
        printLargePreCubes((!winningPositions) & initSys, variableLimits);
        std::cout << "\nSome cubes of winning positions satisfying the initialization assumptions & guarantees: \n";
        printLargePreCubes(winningPositions & initEnv & initSys, variableLimits);
        std::cout << "\nSome cubes of non-winning positions satisfying the initialization assumptions & guarantees: \n";
        printLargePreCubes((!winningPositions) & initEnv & initSys, variableLimits);
        std::cout << "\nSome positions from which the environment player cannot prevent to violate some safety assumptions:\n";
        printLargePreCubes(winningPositions & !(safetyEnv.ExistAbstract(varCubePostInput)), variableLimits);
    }

    /**
     * @brief Custom version of the init method that treats constraints introduced by the "StructuredSlugsCompiler" in
     *        a special way
     * @param filenames
     */
    void init(std::list<std::string> &filenames) {
        if (filenames.size()==0) {
            throw "Error: Cannot load SLUGS input file - there has been no input file name given!";
        }

        std::string inFileName = filenames.front();
        filenames.pop_front();

        std::ifstream inFile(inFileName.c_str());
        if (inFile.fail()) throw "Error: Cannot open input file";

        // Prepare safety and initialization constraints
        initEnv = mgr.constantTrue();
        initSys = mgr.constantTrue();
        safetyEnv = mgr.constantTrue();
        safetySys = mgr.constantTrue();
        variableLimits = mgr.constantTrue();

        // The readmode variable stores in which chapter of the input file we are
        int readMode = -1;
        std::string currentLine;
        bool readingVariableLimitLine = false;
        lineNumberCurrentlyRead = 0;
        while (std::getline(inFile,currentLine)) {
            lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if (currentLine.length()>0) {
                if (currentLine[0]=='#') {
                    if (currentLine.substr(0,20)=="## Variable limits: ") {
                       readingVariableLimitLine = true;
                    }
                } else if (currentLine[0]=='[') {
                    if (currentLine=="[INPUT]") {
                        readMode = 0;
                    } else if (currentLine=="[OUTPUT]") {
                        readMode = 1;
                    } else if (currentLine=="[ENV_INIT]") {
                        readMode = 2;
                    } else if (currentLine=="[SYS_INIT]") {
                        readMode = 3;
                    } else if (currentLine=="[ENV_TRANS]") {
                        readMode = 4;
                    } else if (currentLine=="[SYS_TRANS]") {
                        readMode = 5;
                    } else if (currentLine=="[ENV_LIVENESS]") {
                        readMode = 6;
                    } else if (currentLine=="[SYS_LIVENESS]") {
                        readMode = 7;
                    } else {
                        std::cerr << "Sorry. Didn't recognize category " << currentLine << "\n";
                        throw "Aborted.";
                    }
                    readingVariableLimitLine = false;
                } else {
                    if (readMode==0) {
                        addVariable(PreInput,currentLine);
                        addVariable(PostInput,currentLine+"'");
                    } else if (readMode==1) {
                        addVariable(PreOutput,currentLine);
                        addVariable(PostOutput,currentLine+"'");
                    } else if (readMode==2) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        BF newProperty = parseBooleanFormula(currentLine,allowedTypes);
                        initEnv &= newProperty;
                        if (readingVariableLimitLine) variableLimits &= newProperty;
                    } else if (readMode==3) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        BF newProperty = parseBooleanFormula(currentLine,allowedTypes);
                        initSys &= newProperty;
                        if (readingVariableLimitLine) variableLimits &= newProperty;
                    } else if (readMode==4) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        safetyEnv &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==5) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        safetySys &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==6) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostOutput);
                        allowedTypes.insert(PostInput);
                        livenessAssumptions.push_back(parseBooleanFormula(currentLine,allowedTypes));
                        if (readingVariableLimitLine) throw "Error: Did not expect a variable limit comment in a liveness objective.";
                    } else if (readMode==7) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        livenessGuarantees.push_back(parseBooleanFormula(currentLine,allowedTypes));
                        if (readingVariableLimitLine) throw "Error: Did not expect a variable limit comment in a liveness objective.";
                    } else {
                        std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                        throw "Found a line in the specification file that has no proper categorial context.";
                    }
                    readingVariableLimitLine = false;
                }
            }
        }

        // Check if variable names have been used twice
        std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
        if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

        // Make sure that there is at least one liveness assumption and one liveness guarantee
        // The synthesis algorithm might be unsound otherwise
        if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
        if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
    }



    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XAnalyzeInitialPositions<T,restrictToReachablePositions>(filenames);
    }
};




#endif

