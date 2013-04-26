#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "gr1context.hpp"
#undef fail

/**
 * @brief Constructor that reads the problem instance from file and prepares the BFManager, the BFVarCubes, and the BFVarVectors
 * @param inFile the input filename
 */
GR1Context::GR1Context(std::string inFileName) {

    std::ifstream inFile(inFileName.c_str());
    if (inFile.fail()) throw "Error: Cannot open input file";

    // Prepare safety and initialization constraints
    initEnv = mgr.constantTrue();
    initSys = mgr.constantTrue();
    safetyEnv = mgr.constantTrue();
    safetySys = mgr.constantTrue();

    // The readmode variable stores in which chapter of the input file we are
    int readMode = -1;
    std::string currentLine;
    lineNumberCurrentlyRead = 0;
    while (std::getline(inFile,currentLine)) {
        lineNumberCurrentlyRead++;
        boost::trim(currentLine);
        if ((currentLine.length()>0) && (currentLine[0]!='#')) {
            if (currentLine[0]=='[') {
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
            } else {
                if (readMode==0) {
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine);
                    variableTypes.push_back(PreInput);
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine+"'");
                    variableTypes.push_back(PostInput);
                } else if (readMode==1) {
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine);
                    variableTypes.push_back(PreOutput);
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine+"'");
                    variableTypes.push_back(PostOutput);
                } else if (readMode==2) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    initEnv &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==3) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    initSys &= parseBooleanFormula(currentLine,allowedTypes);
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
                    allowedTypes.insert(PostInput);
                    livenessAssumptions.push_back(parseBooleanFormula(currentLine,allowedTypes));
                } else if (readMode==7) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    allowedTypes.insert(PostInput);
                    allowedTypes.insert(PostOutput);
                    livenessGuarantees.push_back(parseBooleanFormula(currentLine,allowedTypes));
                } else {
                    std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                    throw "Found a line in the specification file that has no proper categorial context.";
                }
            }
        }
    }

    // Compute VarVectors and VarCubes
    std::vector<BF> postOutputVars;
    std::vector<BF> postInputVars;
    std::vector<BF> preOutputVars;
    std::vector<BF> preInputVars;
    for (unsigned int i=0;i<variables.size();i++) {
        switch (variableTypes[i]) {
        case PreInput:
            preVars.push_back(variables[i]);
            preInputVars.push_back(variables[i]);
            break;
        case PreOutput:
            preVars.push_back(variables[i]);
            preOutputVars.push_back(variables[i]);
            break;
        case PostInput:
            postVars.push_back(variables[i]);
            postInputVars.push_back(variables[i]);
            break;
        case PostOutput:
            postVars.push_back(variables[i]);
            postOutputVars.push_back(variables[i]);
            break;
        default:
            throw "Error: Found a variable of unknown type";
        }
    }
    varVectorPre = mgr.computeVarVector(preVars);
    varVectorPost = mgr.computeVarVector(postVars);
    varCubePostInput = mgr.computeCube(postInputVars);
    varCubePostOutput = mgr.computeCube(postOutputVars);
    varCubePreInput = mgr.computeCube(preInputVars);
    varCubePreOutput = mgr.computeCube(preOutputVars);
    varCubePre = mgr.computeCube(preVars);

    // Make sure that there is at least one liveness assumption and one liveness guarantee
    // The synthesis algorithm might be unsound otherwise
    if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
    if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
}

/**
 * @brief Recurse internal function to parse a Boolean formula from a line in the input file
 * @param is the input stream from which the tokens in the line are read
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormulaRecurse(std::istringstream &is,std::set<VariableType> &allowedTypes) {
    std::string operation = "";
    is >> operation;
    if (operation=="") {
        std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. Premature end of line.\n";
        throw "Fatal Error";
    }
    if (operation=="|") return parseBooleanFormulaRecurse(is,allowedTypes) | parseBooleanFormulaRecurse(is,allowedTypes);
    if (operation=="&") return parseBooleanFormulaRecurse(is,allowedTypes) & parseBooleanFormulaRecurse(is,allowedTypes);
    if (operation=="!") return !parseBooleanFormulaRecurse(is,allowedTypes);
    if (operation=="1") return mgr.constantTrue();
    if (operation=="0") return mgr.constantFalse();

    // Has to be a variable!
    for (unsigned int i=0;i<variableNames.size();i++) {
        if (variableNames[i]==operation) {
            if (allowedTypes.count(variableTypes[i])==0) {
                std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. The variable " << operation << " is not allowed for this type of expression.\n";
                throw "Fatal Error";
            }
            return variables[i];
        }
    }
    std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. The variable " << operation << " has not been found.\n";
    throw "Fatal Error";
}

/**
 * @brief Internal function for parsing a Boolean formula from a line in the input file - calls the recursive function to do all the work.
 * @param currentLine the line to parse
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormula(std::string currentLine,std::set<VariableType> &allowedTypes) {

    std::istringstream is(currentLine);

    BF result = parseBooleanFormulaRecurse(is,allowedTypes);
    std::string nextPart = "";
    is >> nextPart;
    if (nextPart=="") return result;

    std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. There are stray characters: '" << nextPart << "'" << std::endl;
    throw "Fatal Error";
}

/**
 * @brief Prints the help to stderr that the user sees when running "slugs --help" or when supplying
 *        incorrect parameters
 */
void printToolUsageHelp() {
    std::cerr << "Usage of slugs:\n";
    std::cerr << "slugs [--onlyRealizability] [--sysInitRoboticsSemantics] <InFile> [OutFile]\n\n";
    std::cerr << "The input file is supposed to be in 'slugs' format. If no output file name is\n";
    std::cerr << "given, then in case of realizability, the synthesized controller is printed\n";
    std::cerr << "to the standard output. The other options are:\n\n";
    std::cerr << " --onlyRealizability  Use this parameter if no synthesized system should be\n";
    std::cerr << "                      computed, but only the realizability/unrealizability\n";
    std::cerr << "                      result is to be computed.\n\n";
    std::cerr << " --sysInitRoboticsSemantics  In standard GR(1) synthesis, a specification is\n";
    std::cerr << "                             called realizable if for every initial input\n";
    std::cerr << "                             proposition valuation that is allowed by the\n";
    std::cerr << "                             initialization contraints, there is some\n";
    std::cerr << "                             suitable output proposition valuation. In the\n";
    std::cerr << "                             modified semantics for robotics applications, the\n";
    std::cerr << "                             controller has to be fine with any admissible\n";
    std::cerr << "                             starting position.\n\n";
}

/**
 * @brief The main function. Parses arguments from the command line and instantiates a synthesizer object accordingly.
 * @return the error code: >0 means that some error has occured. In case of realizability or unrealizability, a value of 0 is returned.
 */
int main(int argc, const char **args) {
    std::cerr << "SLUGS: SmaLl bUt complete Gr(1) Synthesis tool (see the documentation for an author list).\n";

    std::string filenameInputFile = "";
    std::string filenameOutputFile = "";
    bool onlyCheckRealizability = false;
    bool initSpecialRoboticsSemantics = false;

    for (int i=1;i<argc;i++) {
        std::string arg = args[i];
        if (arg[0]=='-') {
            if (arg=="--onlyRealizability") {
                onlyCheckRealizability = true;
            } else if (arg=="--sysInitRoboticsSemantics") {
                initSpecialRoboticsSemantics = true;
            } else if (arg=="--help") {
                printToolUsageHelp();
                return 0;
            } else {
                std::cerr << "Error: Did not understand parameter " << arg << std::endl;
                printToolUsageHelp();
                return 1;
            }
        } else {
            if (filenameInputFile=="") {
                filenameInputFile = arg;
            } else if (filenameOutputFile=="") {
                filenameOutputFile = arg;
            } else {
                std::cerr << "Error: More than two file names given.\n";
                return 1;
            }
        }
    }

    if (filenameInputFile == "") {
        std::cerr << "Error: You did not give an input filename.\n";
        printToolUsageHelp();
        return 1;
    }

    // The output filename might be undefined. In that case, "stdout" is used.

    try {
        GR1Context context(filenameInputFile);
        bool realizable = context.checkRealizability(initSpecialRoboticsSemantics);
        if (realizable) {
            std::cerr << "RESULT: Specification is realizable.\n";

            // Print winning strategy
            if (!onlyCheckRealizability) {
                if (filenameOutputFile=="") {
                    context.computeAndPrintExplicitStateStrategy(std::cout);
                } else {
                    std::ofstream of(filenameOutputFile.c_str());
                    if (of.fail()) {
                        std::cerr << "Error: Could not open output file'" << filenameOutputFile << "\n";
                        return 1;
                    }
                    context.computeAndPrintExplicitStateStrategy(of);
                    if (of.fail()) {
                        std::cerr << "Error: Writing to output file'" << filenameOutputFile << "failed. \n";
                        return 1;
                    }
                    of.close();
                }

            }
        } else {
            std::cerr << "RESULT: Specification is unrealizable.\n";
        }


    } catch (const char *error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }

    return 0;
}
