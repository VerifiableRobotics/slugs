#ifndef __INCREMENTAL_SYNTHESIS_HPP__
#define __INCREMENTAL_SYNTHESIS_HPP__

#include "gr1context.hpp"
#include <time.h>
#include <boost/algorithm/string.hpp>

/**
 * This extension lets the robot perform incremental synthesis/resynthesis in case of changing specifications.
 * The extension
 */
template<class T> class XIncrementalSynthesis : public T {
protected:
    // Inherit stuff that we actually use here.
    using T::variables;
    using T::variableTypes;
    using T::safetyEnv;
    using T::safetySys;
    using T::determinize;
    using T::mgr;
    using T::variableNames;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::initSys;
    using T::initEnv;
    using T::lineNumberCurrentlyRead;
    using T::parseBooleanFormula;
    //using T::winningPositions; - SUPERSEEDED (in this context) BY Level1IntermediateResults.winningPositons (so that the Level1 functions can revert them to TRUE)
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::varCubePostInput;
    using T::varCubePreOutput;
    using T::varCubePreInput;
    using T::varCubePre;
    using T::realizable;
    using T::preVars;
    using T::postVars;

    // Variables to store the initialization and safety assumptions and guarantees separately.
    // TODO: Replace these by ordered maps so that last inserted elements always occur last. This
    // is important for recycling!

    class OrderedRequirementPropertyStorage {
    private:
        std::vector<BF> data;
        std::map<std::string,unsigned int> lookup;
    public:
        OrderedRequirementPropertyStorage() {}
        void add(std::string name, BF bf) {
            lookup[name] = data.size();
            data.push_back(bf);
        }
        std::vector<BF>::const_iterator begin() { return data.begin(); }
        std::vector<BF>::const_iterator end() { return data.end(); }
        bool hasKey(std::string keyName) { return lookup.count(keyName)>0; }
        unsigned int size() { assert(data.size()==lookup.size()); return data.size(); }
    };

    OrderedRequirementPropertyStorage separateSafetyAssumptions;
    OrderedRequirementPropertyStorage separateSafetyGuarantees;
    OrderedRequirementPropertyStorage separateInitAssumptions;
    OrderedRequirementPropertyStorage separateInitGuarantees;
    OrderedRequirementPropertyStorage separateLivenessAssumptions;
    OrderedRequirementPropertyStorage separateLivenessGuarantees;

    // Special options for execution
    bool exitOnError;

    // Classes for storing the intermediate data
    // Levels:
    // 1: Outer fixed point
    // 2: Iteration over the livness guarantees
    // 3: Middle fixed point
    // 4: Iteration over the liveness assumptions
    // 5: Inner-most fixed point

    class Level5IntermediateResults {

    };

    class Level4IntermediateResults {

    };

    class Level3IntermediateResults {

    };

    class Level2IntermediateResults {
    public:
        unsigned int level1IterationNumber;
        BF prefixPointY_0_infty_infty;
        bool prefixPointY_0_infty_infty_valid;
        Level2IntermediateResults() : level1IterationNumber(0), prefixPointY_0_infty_infty_valid(false) {}
    };

    class Level1IntermediateResults {
    public:
        // Important Variables
        BF winningPositions;
        unsigned int iterationNumber;
        std::vector<Level2IntermediateResults*> sub;

        // Functions
        void clearEverythingExceptForTheWinningPositions() {
            for (auto it = sub.begin();it!=sub.end();it++) {
                delete *it;
            }
            sub.clear();
        }
        bool isEmpty() {
            return sub.size()==0;
        }
        void addNewLevel2() {
            sub.push_back(new Level2IntermediateResults());
        }
        void invalidateAllPrefixPointY_0_infty_infty() {
            for (auto it=sub.begin();it!=sub.end();it++) {
                (*it)->prefixPointY_0_infty_infty_valid = false;
            }
        }

        ~Level1IntermediateResults() {
            clearEverythingExceptForTheWinningPositions(); // No need to delete the winning positions
        }
    };

    // The one instance that we use here.
    Level1IntermediateResults intermediateResults;

    XIncrementalSynthesis<T>(std::list<std::string> &filenames) : T(), exitOnError(false) {
        if (filenames.size()>0) {
            SlugsException e(true);
            e << "Error when using the incremental synthesis extension. No filename expected - reading from stdin instead!";
            throw e;
        }
        std::cerr << "Warning: The incremental synthesis extension is still under development! Though shall not trust its results at this point in any form.\n";
    }

    /**
     * @brief Starts an interactive shell that allows the user to type in synthesis commands.
     */
    void execute() {

        std::cout << "Note Incremental synthesis mode active. Reading commands from stdin. Commands available are:\n - Adding input bits and output bits: INPUT <Bitname> and OUTPUT <Bitname>\n";
        std::cout << " - Adding/remove safety/liveness assumption/guarantee. Use the first letter of each case to describe what you want, e.g., RLA for removing liveness assumptions. For removing, add the property name as parameter. For adding, add as parameters first the property name and then the string as used by the slugs input format.";
        std::cout << " - Check realizability: CR\n";
        std::cout << " - Quit: QUIT\n";
        std::cout << " - Switch ExitOnError on/off: EXITONERROR [0/1]\n";
        std::cout << "=========================================================================\n";

        // Make the results reproducible/repeatable/robust against BDD stuff
        // mgr.setAutomaticOptimisation(false);

        // Init State
        lineNumberCurrentlyRead = 0;
        intermediateResults.winningPositions = mgr.constantTrue(); // So that we can start with the "making the life for the system harder" case
        intermediateResults.iterationNumber = 1;
        safetySys = mgr.constantTrue();
        safetyEnv = mgr.constantTrue();
        initSys = mgr.constantTrue();
        initEnv = mgr.constantTrue();
        std::map<unsigned int,std::string> makingLifeForSystemHarderCommands;
        std::map<unsigned int,std::string> makingLifeForSystemEasierCommands;

        while (true) {

            // Read next line. Ignore lines that start with '#'.
            std::cerr.flush();
            std::cout << lineNumberCurrentlyRead << "> ";
            std::cout.flush();
            std::string currentLine;
            std::getline(std::cin,currentLine);
            lineNumberCurrentlyRead++;
            if (std::cin.bad()) throw SlugsException(false,"Error reading from stdin");
            if (std::cin.eof()) return;
            boost::trim(currentLine);
            if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                size_t positionOfFirstSpace = currentLine.find(" ");
                std::string currentCommand = (positionOfFirstSpace!=std::string::npos)?currentLine.substr(0,positionOfFirstSpace):currentLine;
                boost::to_upper(currentCommand);

                //==============================
                // Switch statement for commands
                //==============================
                if (currentCommand=="QUIT") {
                    return;
                } else if (currentCommand=="INPUT") {
                    std::string apName = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                    if (apName.find(" ")!=std::string::npos) {
                        std::cerr << "Error: Stray ' ' in atomic proposition definition command.\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    } else if (positionOfFirstSpace==std::string::npos) {
                        std::cerr << "Error: Atomic proposition name missing.\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    } else {
                        bool found = false;
                        for (auto it = variableNames.begin();it!=variableNames.end();it++) {
                            found |= (*it==apName) || ((*it)==apName+"'");
                        }
                        if (found) {
                            std::cerr << "Error: variable name has already been used (note that primed versions of variables are assigned automatically).\n";
                            if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                        } else {
                            std::cout << "Added input AP: '" << apName << "'\n";
                            variables.push_back(mgr.newVariable());
                            variableNames.push_back(apName);
                            variableTypes.push_back(PreInput);
                            variables.push_back(mgr.newVariable());
                            variableNames.push_back(apName+"'");
                            variableTypes.push_back(PostInput);
                        }
                    }
                } else if (currentCommand=="OUTPUT") {
                    std::string apName = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                    if (apName.find(" ")!=std::string::npos) {
                        std::cerr << "Error: Stray ' ' in atomic proposition definition command.\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    } else if (positionOfFirstSpace==std::string::npos) {
                        std::cerr << "Error: Atomic proposition name missing.\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    } else {
                        bool found = false;
                        for (auto it = variableNames.begin();it!=variableNames.end();it++) {
                            found |= (*it==apName) || ((*it)==apName+"'");
                        }
                        if (found) {
                            std::cerr << "Error: variable name has already been used (note that primed versions of variables are assigned automatically).\n";
                            if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                        } else {
                            std::cout << "Added output AP: '" << apName << "'\n";
                            variables.push_back(mgr.newVariable());
                            variableNames.push_back(apName);
                            variableTypes.push_back(PreOutput);
                            variables.push_back(mgr.newVariable());
                            variableNames.push_back(apName+"'");
                            variableTypes.push_back(PostOutput);
                        }
                    }
                } else if (currentCommand=="ASA") {
                    makingLifeForSystemEasierCommands[lineNumberCurrentlyRead] = currentLine;
                } else if (currentCommand=="ASG") {
                    makingLifeForSystemHarderCommands[lineNumberCurrentlyRead] = currentLine;
                } else if (currentCommand=="ALA") {
                    makingLifeForSystemEasierCommands[lineNumberCurrentlyRead] = currentLine;
                } else if (currentCommand=="ALG") {
                    makingLifeForSystemHarderCommands[lineNumberCurrentlyRead] = currentLine;
                } else if (currentCommand=="AIG") {
                    // Add safety Assumption
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    if (positionOfFirstSpace==std::string::npos) {
                        std::cerr << "Error: Expected name of property and property string (case 1).\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    } else {
                        std::string restOfTheCommand = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                        size_t positionOfSecondSpace = restOfTheCommand.find(" ");
                        if (positionOfSecondSpace==std::string::npos) {
                            std::cerr << "Error: Expected name of property and property string (case 2).\n";
                            if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                        } else {
                            std::string propertyString = restOfTheCommand.substr(positionOfSecondSpace+1,std::string::npos);
                            std::string nameString = restOfTheCommand.substr(0,positionOfSecondSpace);
                            try {
                                BF newProperty = parseBooleanFormula(propertyString,allowedTypes);
                                if (separateInitGuarantees.hasKey(nameString)) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateInitGuarantees.add(nameString,newProperty);
                            } catch (SlugsException e) {
                                if (exitOnError) throw e;
                                std::cerr << "Error: " << e.getMessage() << std::endl;
                            }
                        }
                    }
                } else if (currentCommand=="AIA") {
                    // Add Init Assumption
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    if (positionOfFirstSpace==std::string::npos) {
                        std::cerr << "Error: Expected name of property and property string (case 1).\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    } else {
                        std::string restOfTheCommand = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                        size_t positionOfSecondSpace = restOfTheCommand.find(" ");
                        if (positionOfSecondSpace==std::string::npos) {
                            std::cerr << "Error: Expected name of property and property string (case 2).\n";
                            if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                        } else {
                            std::string propertyString = restOfTheCommand.substr(positionOfSecondSpace+1,std::string::npos);
                            std::string nameString = restOfTheCommand.substr(0,positionOfSecondSpace);
                            try {
                                BF newProperty = parseBooleanFormula(propertyString,allowedTypes);
                                if (separateInitAssumptions.hasKey(nameString)) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateInitAssumptions.add(nameString,newProperty);
                            } catch (SlugsException e) {
                                if (exitOnError) throw e;
                                std::cerr << "Error: " << e.getMessage() << std::endl;
                            }
                        }
                    }
                } else if (currentCommand=="EXITONERROR") {
                    if (positionOfFirstSpace==std::string::npos) {
                        std::cerr << "Error: Expected 0 or 1.\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    }
                    std::string restOfTheCommand = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                    if (restOfTheCommand=="0") {
                        exitOnError = false;
                    } else if (restOfTheCommand=="1") {
                        exitOnError = true;
                    } else {
                        std::cerr << "Error: Expected 0 or 1. but found '" << restOfTheCommand << "'\n";
                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                    }
                } else if (currentCommand=="CR") {

                    // Line numbers are temporarily reassigned during delayed parsing
                    unsigned oldLineNumber = lineNumberCurrentlyRead;

                    // Compute Cubes and Var Vectors
                    preVars.clear();
                    postVars.clear();
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

                    // Add Trueliveness assumptions/guarantee if needed
                    if (separateLivenessAssumptions.size()==0) {
                        separateLivenessAssumptions.add("__AUTO_ADDED__",mgr.constantTrue());
                    }
                    if (separateLivenessGuarantees.size()==0) {
                        separateLivenessGuarantees.add("__AUTO_ADDED__",mgr.constantTrue());
                        intermediateResults.addNewLevel2();
                    }

                    //==================================
                    // Making life easier for the system
                    //==================================
                    if (makingLifeForSystemEasierCommands.size()>0) {
                        for (auto it = makingLifeForSystemEasierCommands.begin();it!=makingLifeForSystemEasierCommands.end();it++) {
                            std::string currentLine = it->second;
                            lineNumberCurrentlyRead = it->first;
                            size_t positionOfFirstSpace = currentLine.find(" ");
                            std::string currentCommand = (positionOfFirstSpace!=std::string::npos)?currentLine.substr(0,positionOfFirstSpace):currentLine;
                            boost::to_upper(currentCommand);

                            if (currentCommand=="ASA") {
                                // Add safety Assumption
                                std::set<VariableType> allowedTypes;
                                allowedTypes.insert(PreInput);
                                allowedTypes.insert(PreOutput);
                                allowedTypes.insert(PostInput);
                                if (positionOfFirstSpace==std::string::npos) {
                                    std::cerr << "Error: Expected name of property and property string (case 1).\n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                } else {
                                    std::string restOfTheCommand = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                                    size_t positionOfSecondSpace = restOfTheCommand.find(" ");
                                    if (positionOfSecondSpace==std::string::npos) {
                                        std::cerr << "Error: Expected name of property and property string (case 2).\n";
                                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                    } else {
                                        std::string propertyString = restOfTheCommand.substr(positionOfSecondSpace+1,std::string::npos);
                                        std::string nameString = restOfTheCommand.substr(0,positionOfSecondSpace);
                                        try {
                                            BF newProperty = parseBooleanFormula(propertyString,allowedTypes);
                                            if (separateSafetyAssumptions.hasKey(nameString)) {
                                                std::cerr << "Error: Trying to re-use name of the property string. \n";
                                                if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                            }
                                            separateSafetyAssumptions.add(nameString,newProperty);

                                            // TODO: Remove the stuff that we can no longer use.

                                        } catch (SlugsException e) {
                                            if (exitOnError) throw e;
                                            std::cerr << "Error: " << e.getMessage() << std::endl;
                                        }
                                    }
                                }
                            } else if (currentCommand=="ALA") {
                                // Add safety Assumption
                                std::set<VariableType> allowedTypes;
                                allowedTypes.insert(PreInput);
                                allowedTypes.insert(PreOutput);
                                allowedTypes.insert(PostInput);
                                if (positionOfFirstSpace==std::string::npos) {
                                    std::cerr << "Error: Expected name of property and property string (case 1).\n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                } else {
                                    std::string restOfTheCommand = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                                    size_t positionOfSecondSpace = restOfTheCommand.find(" ");
                                    if (positionOfSecondSpace==std::string::npos) {
                                        std::cerr << "Error: Expected name of property and property string (case 2).\n";
                                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                    } else {
                                        std::string propertyString = restOfTheCommand.substr(positionOfSecondSpace+1,std::string::npos);
                                        std::string nameString = restOfTheCommand.substr(0,positionOfSecondSpace);
                                        try {
                                            BF newProperty = parseBooleanFormula(propertyString,allowedTypes);
                                            if (separateLivenessAssumptions.hasKey(nameString)) {
                                                std::cerr << "Error: Trying to re-use name of the property string. \n";
                                                if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                            }
                                            separateLivenessAssumptions.add(nameString,newProperty);

                                            // TODO: Remove the stuff that we can no longer use.
                                        } catch (SlugsException e) {
                                            if (exitOnError) throw e;
                                            std::cerr << "Error: " << e.getMessage() << std::endl;
                                        }
                                    }
                                }
                            } else {
                                SlugsException e (false,"Internal error in line ");
                                e << __LINE__ << " in file " << __FILE__ << "\n";
                                throw e;
                            }
                        }

                        // TODO Here: Remove True liveness assumption if no longer needed (can only be the first one in the list).

                        // Try to synthesize - but only if we have a history already.
                        if (intermediateResults.iterationNumber!=1) {

                            // Recompute variables used in the synthesis algorithm
                            safetyEnv = mgr.constantTrue();
                            for (auto it = separateSafetyAssumptions.begin();it!=separateSafetyAssumptions.end();it++) {
                                safetyEnv &= *it;
                            }
                            livenessAssumptions.clear();
                            for (auto it = separateLivenessAssumptions.begin();it!=separateLivenessAssumptions.end();it++) {
                                livenessAssumptions.push_back(*it);
                            }
                            safetySys = mgr.constantTrue();
                            for (auto it = separateSafetyGuarantees.begin();it!=separateSafetyGuarantees.end();it++) {
                                safetySys &= *it;
                            }
                            livenessGuarantees.clear();
                            for (auto it = separateLivenessGuarantees.begin();it!=separateLivenessGuarantees.end();it++) {
                                livenessGuarantees.push_back(*it);
                            }

                            // Perform re-synthesis!
                            resynthesisUnderMakingSystemLifeEasier();
                        }
                        makingLifeForSystemEasierCommands.clear();
                    }

                    // Making life harder
                    std::cout << "MakingLifeHarderSize: "<<makingLifeForSystemHarderCommands.size()<<std::endl;
                    if (makingLifeForSystemHarderCommands.size()>0) {
                        for (auto it = makingLifeForSystemHarderCommands.begin();it!=makingLifeForSystemHarderCommands.end();it++) {
                            std::string currentLine = it->second;
                            lineNumberCurrentlyRead = it->first;
                            size_t positionOfFirstSpace = currentLine.find(" ");
                            std::string currentCommand = (positionOfFirstSpace!=std::string::npos)?currentLine.substr(0,positionOfFirstSpace):currentLine;
                            boost::to_upper(currentCommand);

                            if (currentCommand=="ALG") {
                                // Add safety Assumption
                                std::set<VariableType> allowedTypes;
                                allowedTypes.insert(PreInput);
                                allowedTypes.insert(PreOutput);
                                allowedTypes.insert(PostInput);
                                allowedTypes.insert(PostOutput);
                                if (positionOfFirstSpace==std::string::npos) {
                                    std::cerr << "Error: Expected name of property and property string (case 1).\n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                } else {
                                    std::string restOfTheCommand = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                                    size_t positionOfSecondSpace = restOfTheCommand.find(" ");
                                    if (positionOfSecondSpace==std::string::npos) {
                                        std::cerr << "Error: Expected name of property and property string (case 2).\n";
                                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                    } else {
                                        std::string propertyString = restOfTheCommand.substr(positionOfSecondSpace+1,std::string::npos);
                                        std::string nameString = restOfTheCommand.substr(0,positionOfSecondSpace);
                                        try {
                                            BF newProperty = parseBooleanFormula(propertyString,allowedTypes);
                                            if (separateLivenessGuarantees.hasKey(nameString)) {
                                                std::cerr << "Error: Trying to re-use name of the property string. \n";
                                                if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                            }
                                            separateLivenessGuarantees.add(nameString,newProperty);
                                            intermediateResults.addNewLevel2();
                                        } catch (SlugsException e) {
                                            if (exitOnError) throw e;
                                            std::cerr << "Error: " << e.getMessage() << std::endl;
                                        }
                                    }
                                }
                            } else if (currentCommand=="ASG") {
                                // Add safety Assumption
                                std::set<VariableType> allowedTypes;
                                allowedTypes.insert(PreInput);
                                allowedTypes.insert(PreOutput);
                                allowedTypes.insert(PostInput);
                                allowedTypes.insert(PostOutput);
                                if (positionOfFirstSpace==std::string::npos) {
                                    std::cerr << "Error: Expected name of property and property string (case 1).\n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                } else {
                                    std::string restOfTheCommand = currentLine.substr(positionOfFirstSpace+1,std::string::npos);
                                    size_t positionOfSecondSpace = restOfTheCommand.find(" ");
                                    if (positionOfSecondSpace==std::string::npos) {
                                        std::cerr << "Error: Expected name of property and property string (case 2).\n";
                                        if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                    } else {
                                        std::string propertyString = restOfTheCommand.substr(positionOfSecondSpace+1,std::string::npos);
                                        std::string nameString = restOfTheCommand.substr(0,positionOfSecondSpace);
                                        try {
                                            BF newProperty = parseBooleanFormula(propertyString,allowedTypes);
                                            if (separateSafetyGuarantees.hasKey(nameString)) {
                                                std::cerr << "Error: Trying to re-use name of the property string. \n";
                                                if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                            }
                                            separateSafetyGuarantees.add(nameString,newProperty);
                                            intermediateResults.iterationNumber++; // Forces going over all of the liveness guarantees at least once more
                                            intermediateResults.invalidateAllPrefixPointY_0_infty_infty();
                                        } catch (SlugsException e) {
                                            if (exitOnError) throw e;
                                            std::cerr << "Error: " << e.getMessage() << std::endl;
                                        }
                                    }
                                }
                            } else {
                                SlugsException e (false,"Internal error in line ");
                                e << __LINE__ << " in file " << __FILE__ << "\n";
                                throw e;
                            }
                        }

                        // Recompute variables used in the synthesis algorithm
                        safetyEnv = mgr.constantTrue();
                        for (auto it = separateSafetyAssumptions.begin();it!=separateSafetyAssumptions.end();it++) {
                            safetyEnv &= *it;
                        }
                        livenessAssumptions.clear();
                        for (auto it = separateLivenessAssumptions.begin();it!=separateLivenessAssumptions.end();it++) {
                            livenessAssumptions.push_back(*it);
                        }
                        safetySys = mgr.constantTrue();
                        for (auto it = separateSafetyGuarantees.begin();it!=separateSafetyGuarantees.end();it++) {
                            safetySys &= *it;
                        }
                        livenessGuarantees.clear();
                        for (auto it = separateLivenessGuarantees.begin();it!=separateLivenessGuarantees.end();it++) {
                            livenessGuarantees.push_back(*it);
                        }

                        // Perform re-synthesis!
                        resynthesisUnderMakingSystemLifeHarder();

                        // Clean up
                        makingLifeForSystemHarderCommands.clear();
                    }

                    // Now check realizability
                    initEnv = mgr.constantTrue();
                    for (auto it = separateInitAssumptions.begin();it!=separateInitAssumptions.end();it++) {
                        initEnv &= *it;
                    }
                    initSys = mgr.constantTrue();
                    for (auto it = separateInitGuarantees.begin();it!=separateInitGuarantees.end();it++) {
                        initSys &= *it;
                    }


                    // Check if for every possible environment initial position the system has a good system initial position
                    BF result;
                    result = initEnv.Implies((intermediateResults.winningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput);

                    // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
                    if (!result.isConstant()) throw "Internal error: Could not establish realizability/unrealizability of the specification.";

                    // Return the result in Boolean form.
                    realizable = result.isTrue();

                    if (realizable) {
                        std::cout << "RESULT: Specification is realizable.\n";
                    } else {
                        std::cout << "RESULT: Specification is unrealizable.\n";
                    }

                    // Done!
                    lineNumberCurrentlyRead = oldLineNumber;

                } else {
                    if (exitOnError) {
                        SlugsException e(false);
                        e << "The command '" << currentCommand << "' is not recognized.";
                        throw e;
                    } else {
                        std::cout << "Error: The command '" << currentCommand << "' is not recognized \n";
                    }
                }
            }
            std::cout << "Time (in s): " << (double)clock()/CLOCKS_PER_SEC << std::endl;
        }
    }

    // ============================================================
    // Resynthesis for the case that the system life became easier
    // ============================================================
    void resynthesisUnderMakingSystemLifeEasier() {

       // The greatest fixed point - called "Z" in the GR(1) synthesis paper
       BFFixedPoint nu2(mgr.constantTrue());

       // Only increase iteration number in the second round.
       intermediateResults.iterationNumber=0;

       // Iterate until we have found a fixed point
       for (;!nu2.isFixedPointReached();) {

           intermediateResults.iterationNumber++;

           // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
           // goal. Then, after we have iterated over the goals, we can update nu2.
           BF nextContraintsForGoals = mgr.constantTrue();
           for (unsigned int j=0;j<livenessGuarantees.size();j++) {

               std::cout << "Working on guarantee: " << j << std::endl;

               // Start computing the transitions that lead closer to the goal and lead to a position that is not yet known to be losing.
               // Start with the ones that actually represent reaching the goal (which is a transition in this implementation as we can have
               // nexts in the goal descriptions).
               BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));

               // Compute the middle least-fixed point (called 'Y' in the GR(1) paper)
               BFFixedPoint mu1(((intermediateResults.iterationNumber==1) && intermediateResults.sub[j]->prefixPointY_0_infty_infty_valid)?intermediateResults.sub[j]->prefixPointY_0_infty_infty:mgr.constantFalse());
               // BFFixedPoint mu1(mgr.constantFalse());
               for (;!mu1.isFixedPointReached();) {

                   std::cout << "Mu1 working." << std::endl;

                   // Update the set of transitions that lead closer to the goal.
                   livetransitions |= mu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                   // Iterate over the liveness assumptions. Store the positions that are found to be winning for *any*
                   // of them into the variable 'goodForAnyLivenessAssumption'.
                   BF goodForAnyLivenessAssumption = mu1.getValue();
                   for (unsigned int i=0;i<livenessAssumptions.size();i++) {

                       // Prepare the variable 'foundPaths' that contains the transitions that stay within the inner-most
                       // greatest fixed point or get closer to the goal. Only used for strategy extraction
                       BF foundPaths = mgr.constantTrue();

                       // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                       BFFixedPoint nu0(mgr.constantTrue());
                       for (;!nu0.isFixedPointReached();) {

                           // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                           foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                           foundPaths &= safetySys;

                           // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                           nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput));
                       }

                       // Update the set of positions that are winning for some liveness assumption
                       goodForAnyLivenessAssumption |= nu0.getValue();
                   }

                   // Update the moddle fixed point
                   mu1.update(goodForAnyLivenessAssumption);
               }

               // assert((!intermediateResults.sub[j]->prefixPointY_0_infty_infty_valid) || ((intermediateResults.sub[j]->prefixPointY_0_infty_infty & !mu1.getValue())==mgr.constantFalse()));

               // Update the set of positions that are winning for any goal for the outermost fixed point
               nextContraintsForGoals &= mu1.getValue();

               // Update intermediate stuff
               intermediateResults.sub[j]->level1IterationNumber = intermediateResults.iterationNumber;
               if (nu2.getValue()==mgr.constantTrue()) {
                   intermediateResults.sub[j]->prefixPointY_0_infty_infty = mu1.getValue();
                   intermediateResults.sub[j]->prefixPointY_0_infty_infty_valid = true;
               }
           }

           // Update the outer-most fixed point
           nu2.update(nextContraintsForGoals);
       }

       // We found the set of winning positions
       intermediateResults.winningPositions = nu2.getValue();
    }

    // ============================================================
    // Resynthesis for the case that the system life became harder.
    // ============================================================
    void resynthesisUnderMakingSystemLifeHarder() {

        // The greatest fixed point - called "Z" in the GR(1) synthesis paper
        BFFixedPoint nu2(intermediateResults.winningPositions);

        // Only increase iteration number in the second round.
        intermediateResults.iterationNumber--;

        // Iterate until we have found a fixed point
        for (;!nu2.isFixedPointReached();) {
            std::cout << "Nu2 is running....\n";

            intermediateResults.iterationNumber++;
            BF nextContraintsForGoals = mgr.constantTrue();

            // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
            // goal. Then, after we have iterated over the goals, we can update nu2.
            for (unsigned int j=0;j<livenessGuarantees.size();j++) {

                std::cout << "Working on liveness Guarantee " << j << " with the actual iteration number " << intermediateResults.iterationNumber << std::endl;

                if (intermediateResults.iterationNumber != intermediateResults.sub[j]->level1IterationNumber) {

                    std::cout << "....actually working on it." << std::endl;

                    // Start computing the transitions that lead closer to the goal and lead to a position that is not yet known to be losing.
                    // Start with the ones that actually represent reaching the goal (which is a transition in this implementation as we can have
                    // nexts in the goal descriptions).
                    BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));

                    // Compute the middle least-fixed point (called 'Y' in the GR(1) paper)
                    BFFixedPoint mu1(mgr.constantFalse());
                    for (;!mu1.isFixedPointReached();) {

                        // Update the set of transitions that lead closer to the goal.
                        livetransitions |= mu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                        // Iterate over the liveness assumptions. Store the positions that are found to be winning for *any*
                        // of them into the variable 'goodForAnyLivenessAssumption'.
                        BF goodForAnyLivenessAssumption = mu1.getValue();
                        for (unsigned int i=0;i<livenessAssumptions.size();i++) {

                            // Prepare the variable 'foundPaths' that contains the transitions that stay within the inner-most
                            // greatest fixed point or get closer to the goal. Only used for strategy extraction
                            BF foundPaths = mgr.constantTrue();

                            // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                            BFFixedPoint nu0(mgr.constantTrue());
                            for (;!nu0.isFixedPointReached();) {

                                // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                                foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                                foundPaths &= safetySys;

                                // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                                nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput));
                            }

                            // Update the set of positions that are winning for some liveness assumption
                            goodForAnyLivenessAssumption |= nu0.getValue();
                        }

                        // Update the moddle fixed point
                        mu1.update(goodForAnyLivenessAssumption);
                    }

                    // Update the set of positions that are winning for any goal for the outermost fixed point
                    nextContraintsForGoals &= mu1.getValue();
                    //BF_newDumpDot(*this,nextContraintsForGoals,NULL,"/tmp/inter.dot");
                    //throw 23;
                    intermediateResults.sub[j]->level1IterationNumber = intermediateResults.iterationNumber;
                    if (nu2.getValue()==mgr.constantTrue()) {
                        intermediateResults.sub[j]->prefixPointY_0_infty_infty = mu1.getValue();
                        intermediateResults.sub[j]->prefixPointY_0_infty_infty_valid = true;
                    }
                }
            }
            if (nu2.getValue() == nextContraintsForGoals) {
                std::cout << "No change!\n";
            }
            nu2.update(nu2.getValue() & nextContraintsForGoals);

        }

        // We found the set of winning positions
        intermediateResults.winningPositions = nu2.getValue();
    }

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XIncrementalSynthesis<T>(filenames);
    }

};




#endif
