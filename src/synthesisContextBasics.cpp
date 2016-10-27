#include "gr1context.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>


/**
 * @brief Constructor that reads the problem instance from file and prepares the BFManager, the BFVarCubes, and the BFVarVectors
 * @param inFile the input filename
 */
GR1Context::GR1Context(std::list<std::string> &filenames) {
    (void)filenames;
}

/**
 * @brief Recurse internal function to parse a Boolean formula from a line in the input file
 * @param is the input stream from which the tokens in the line are read
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormulaRecurse(std::istringstream &is,std::set<VariableType> &allowedTypes, std::vector<BF> &memory) {
    std::string operation = "";
    is >> operation;
    if (operation=="") {
        SlugsException e(false);
        e << "Error reading line " << lineNumberCurrentlyRead << ". Premature end of line.";
        throw e;
    }
    if (operation=="|") return parseBooleanFormulaRecurse(is,allowedTypes,memory) | parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="^") return parseBooleanFormulaRecurse(is,allowedTypes,memory) ^ parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="&") return parseBooleanFormulaRecurse(is,allowedTypes,memory) & parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="!") return !parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="1") return mgr.constantTrue();
    if (operation=="0") return mgr.constantFalse();

    // Memory Functionality - Create Buffer
    if (operation=="$") {
        unsigned nofElements;
        is >> nofElements;
        if (is.fail()) {
            SlugsException e(false);
            e << "Error reading line " << lineNumberCurrentlyRead << ". Expected number of memory elements.";
            throw e;
        }
        std::vector<BF> memoryNew(nofElements);
        for (unsigned int i=0;i<nofElements;i++) {
            memoryNew[i] = parseBooleanFormulaRecurse(is,allowedTypes,memoryNew);
        }
        return memoryNew[nofElements-1];
    }

    // Memory Functionality - Recall from Buffer
    if (operation=="?") {
        unsigned int element;
        is >> element;
        if (is.fail()) {
            SlugsException e(false);
            e << "Error reading line " << lineNumberCurrentlyRead << ". Expected number after memory recall operator '?'.";
            throw e;
        }
        if (element>=memory.size()) {
            SlugsException e(false);
            e << "Error reading line " << lineNumberCurrentlyRead << ". Trying to recall a memory element that has not been stored (yet).";
            throw e;
        }
        return memory[element];
    }

    // Has to be a variable!
    for (unsigned int i=0;i<variableNames.size();i++) {
        if (variableNames[i]==operation) {
            if (allowedTypes.count(variableTypes[i])==0) {
                SlugsException e(false);
                e << "Error reading line " << lineNumberCurrentlyRead << ". The variable " << operation << " is not allowed for this type of expression.";
                throw e;
            }
            return variables[i];
        }
    }
    SlugsException e(false);
    e << "Error reading line " << lineNumberCurrentlyRead << ". The variable " << operation << " has not been found.";
    throw e;
}

/**
 * @brief Internal function for parsing a Boolean formula from a line in the input file - calls the recursive function to do all the work.
 * @param currentLine the line to parse
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormula(std::string currentLine, std::set<VariableType> &allowedTypes) {

    std::istringstream is(currentLine);

    std::vector<BF> memory;
    BF result = parseBooleanFormulaRecurse(is,allowedTypes,memory);
    assert(memory.size()==0);
    std::string nextPart = "";
    is >> nextPart;
    if (nextPart=="") return result;

    SlugsException e(false);
    e << "Error reading line " << lineNumberCurrentlyRead << ". There are stray characters: '" << nextPart << "'";
    throw e;
}


void GR1Context::execute() {
    checkRealizability();
    if (realizable) {
        std::cerr << "RESULT: Specification is realizable.\n";
    } else {
        std::cerr << "RESULT: Specification is unrealizable.\n";
    }
}

void GR1Context::init(std::list<std::string> &filenames) {
    if (filenames.size()==0) {
        throw "Error: Cannot load SLUGS input file - there has been no input file name given!";
    }

    std::string inFileName = filenames.front();
    filenames.pop_front();

    // Open input file or produce error message if that does not work
    std::ifstream inFile(inFileName.c_str());
    if (inFile.fail()) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot open input file '" << inFileName << "'";
        throw errorMessage.str();
    }

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
                    addVariable(PreInput,currentLine);
                    addVariable(PostInput,currentLine+"'");
                } else if (readMode==1) {
                    addVariable(PreOutput,currentLine);
                    addVariable(PostOutput,currentLine+"'");
                } else if (readMode==2) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
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
                    allowedTypes.insert(PostOutput);
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

    // Check if variable names have been used twice
    std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
    if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

    // Make sure that there is at least one liveness assumption and one liveness guarantee
    // The synthesis algorithm might be unsound otherwise
    if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
    if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
}

