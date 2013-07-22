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

    // Variables to store the initialization and safety assumptions and guarantees separately.
    std::map<std::string,BF> separateSafetyAssumptions;
    std::map<std::string,BF> separateSafetyGuarantees;
    std::map<std::string,BF> separateInitAssumptions;
    std::map<std::string,BF> separateInitGuarantees;
    std::map<std::string,BF> separateLivenessAssumptions;
    std::map<std::string,BF> separateLivenessGuarantees;

    // Special options for execution
    bool exitOnError;

    // Keeping track of the state
    bool madeLifeHarderForTheSystem;
    bool madeLifeEasierForTheSystem;

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

        madeLifeHarderForTheSystem = false;
        madeLifeEasierForTheSystem = false;
        lineNumberCurrentlyRead = 0;

        while (true) {

            // Read next line. Ignore lines that start with '#'.
            std::string currentLine;
            std::getline(std::cin,currentLine);
            lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                std::cerr.flush();
                std::cout << lineNumberCurrentlyRead << "> ";
                std::cout.flush();
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

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XIncrementalSynthesis<T>(filenames);
    }

};




#endif
