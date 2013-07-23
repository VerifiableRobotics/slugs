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
    using T::winningPositions;
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
    std::map<std::string,BF> separateSafetyAssumptions;
    std::map<std::string,BF> separateSafetyGuarantees;
    std::map<std::string,BF> separateInitAssumptions;
    std::map<std::string,BF> separateInitGuarantees;
    std::map<std::string,BF> separateLivenessAssumptions;
    std::map<std::string,BF> separateLivenessGuarantees;
    unsigned int oldLivenessGuaranteesSize;
    unsigned int oldLivenessAssumptionsSize;

    // Special options for execution
    bool exitOnError;

    // Keeping track of the state
    bool madeLifeHarderForTheSystem;
    bool madeLifeEasierForTheSystem;

    // Intermediate data
    std::vector<BF> ZHistory;
    std::vector<std::vector<BF> > YHistory;
    std::vector<std::vector<std::vector<BF> > > XHistory;

    XIncrementalSynthesis<T>(std::list<std::string> &filenames) : T(), exitOnError(false) {
        if (filenames.size()>0) {
            SlugsException e(true);
            e << "Error when using the incremental synthesis extension. No filename expected - reading from stdin instead!";
            throw e;
        }
        std::cerr << "Warning: The incremental synthesis extension is still under development! Though shall not trust its results at this point in any form.\n";
    }

    /**
     * @brief Starts an interactive shell that allows the user to type in synthesis commands.s
     */
    void execute() {

        std::cout << "Note Incremental synthesis mode active. Reading commands from stdin. Commands available are:\n - Adding input bits and output bits: INPUT <Bitname> and OUTPUT <Bitname>\n";
        std::cout << " - Adding/remove safety/liveness assumption/guarantee. Use the first letter of each case to describe what you want, e.g., RLA for removing liveness assumptions. For removing, add the property name as parameter. For adding, add as parameters first the property name and then the string as used by the slugs input format.";
        std::cout << " - Check realizability: CR\n";
        std::cout << " - Quit: QUIT\n";
        std::cout << " - Switch ExitOnError on/off: EXITONERROR [0/1]\n";
        std::cout << "=========================================================================\n";

        // Init State
        madeLifeHarderForTheSystem = false;
        madeLifeEasierForTheSystem = false;
        lineNumberCurrentlyRead = 0;
        winningPositions = mgr.constantTrue(); // So that we can start with the "making the life for the system harder" case
        safetySys = mgr.constantTrue();
        safetyEnv = mgr.constantTrue();
        initSys = mgr.constantTrue();
        initEnv = mgr.constantTrue();

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
                                if (separateSafetyAssumptions.count(nameString)>0) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateSafetyAssumptions[nameString] = newProperty;
                                madeLifeEasierForTheSystem = true;
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
                                if (separateSafetyGuarantees.count(nameString)>0) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateSafetyGuarantees[nameString] = newProperty;
                                madeLifeHarderForTheSystem = true;
                            } catch (SlugsException e) {
                                if (exitOnError) throw e;
                                std::cerr << "Error: " << e.getMessage() << std::endl;
                            }
                        }
                    }
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
                                if (separateInitGuarantees.count(nameString)>0) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateInitGuarantees[nameString] = newProperty;
                            } catch (SlugsException e) {
                                if (exitOnError) throw e;
                                std::cerr << "Error: " << e.getMessage() << std::endl;
                            }
                        }
                    }
                } else if (currentCommand=="AIS") {
                    // Add safety Assumption
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
                                if (separateInitAssumptions.count(nameString)>0) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateInitAssumptions[nameString] = newProperty;
                            } catch (SlugsException e) {
                                if (exitOnError) throw e;
                                std::cerr << "Error: " << e.getMessage() << std::endl;
                            }
                        }
                    }
                } else if (currentCommand=="ALG") {
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
                                if (separateLivenessGuarantees.count(nameString)>0) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateLivenessGuarantees[nameString] = newProperty;
                                madeLifeHarderForTheSystem = true;
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
                                if (separateLivenessAssumptions.count(nameString)>0) {
                                    std::cerr << "Error: Trying to re-use name of the property string. \n";
                                    if (exitOnError) throw SlugsException(false,"Aborting due to ExitOnError being set to true.");
                                }
                                separateLivenessAssumptions[nameString] = newProperty;
                                madeLifeEasierForTheSystem = true;
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
                        separateLivenessAssumptions["__AUTO_ADDED__"] = mgr.constantTrue();
                    }
                    if (separateLivenessGuarantees.size()==0) {
                        separateLivenessGuarantees["__AUTO_ADDED__"] = mgr.constantTrue();
                    }

                    // TODO Here: Remove True assumption/guarantee if no longer needed (can only be the first one in the list).
                    // Don't forget to remove the buffer content along the way!


                    // Making life easier....
                    if (madeLifeEasierForTheSystem) {
                        // Resynthesize under making life easier for the system

                        // Remove guarantees here

                        // Try to synthesize - but only if we have a history already.
                        if (ZHistory.size()>0) {
                            throw SlugsException(false,"Making life easier is unsupported.");
                        }
                    }

                    // Making life harder
                    if (madeLifeHarderForTheSystem || ZHistory.size()==0) {

                        // Resynthesis under making life harder for the system (or synthesize for the first time)

                        // Add new guarantees
                        oldLivenessAssumptionsSize = livenessGuarantees.size();
                        for (auto it = separateSafetyGuarantees.begin();it!=separateSafetyGuarantees.end();it++) {
                            safetySys &= it->second;
                        }
                        oldLivenessGuaranteesSize = livenessGuarantees.size();
                        livenessGuarantees.clear();
                        for (auto it = separateLivenessGuarantees.begin();it!=separateLivenessGuarantees.end();it++) {
                            livenessGuarantees.push_back(it->second);
                        }

                        // Remove assumptions here.

                        // Perform re-synthesis!
                        resynthesisUnderMakingSystemLifeHarder();
                    }

                    // Now check realizability
                    // Check if for every possible environment initial position the system has a good system initial position
                    BF result;
                    result = initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput);

                    // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
                    if (!result.isConstant()) throw "Internal error: Could not establish realizability/unrealizability of the specification.";

                    // Return the result in Boolean form.
                    realizable = result.isTrue();

                    if (realizable) {
                        std::cerr << "RESULT: Specification is realizable.\n";
                    } else {
                        std::cerr << "RESULT: Specification is unrealizable.\n";
                    }

                    // Done!
                    madeLifeEasierForTheSystem = false;
                    madeLifeHarderForTheSystem = false;

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

    // Resynthesis for the case the system Life became harder.
    void resynthesisUnderMakingSystemLifeHarder() {

        // The greatest fixed point - called "Z" in the GR(1) synthesis paper
        BFFixedPoint nu2(winningPositions);
        bool firstZIteration = true;

        // Iterate until we have found a fixed point
        for (;!nu2.isFixedPointReached();) {

            // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
            // goal. Then, after we have iterated over the goals, we can update nu2.
            BF nextContraintsForGoals = winningPositions;
            for (unsigned int j=(firstZIteration?oldLivenessGuaranteesSize:0);j<livenessGuarantees.size();j++) {

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
            }

            // Update the outer-most fixed point
            nu2.update(nextContraintsForGoals);
            ZHistory.push_back(nu2.getValue());
            firstZIteration = false;
        }

        // We found the set of winning positions
        winningPositions = nu2.getValue();
    }

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XIncrementalSynthesis<T>(filenames);
    }

};




#endif
