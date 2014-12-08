#ifndef __EXTENSION_INTERACTIVE_STRATEGY_HPP
#define __EXTENSION_INTERACTIVE_STRATEGY_HPP

#include <boost/algorithm/string.hpp>


/**
 * A class that opens an interactive shell to allow examining the property of strategies computed
 *
 */
template<class T> class XInteractiveStrategy : public T {
protected:
    XInteractiveStrategy<T>(std::list<std::string> &filenames) : T(filenames) {}

    using T::checkRealizability;
    using T::realizable;
    using T::variables;
    using T::variableNames;
    using T::variableTypes;
    using T::mgr;
    using T::winningPositions;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::safetyEnv;
    using T::safetySys;
    using T::strategyDumpingData;
    using T::varCubePostInput;
    using T::varCubePostOutput;
    using T::varCubePre;
    using T::varCubePost;
    using T::postOutputVars;
    using T::determinize;
    using T::initEnv;
    using T::initSys;
    using T::preVars;
    using T::postVars;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::doesVariableInheritType;


public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XInteractiveStrategy<T>(filenames);
    }

    void execute() {
        checkRealizability();

        if (realizable) {
            std::cerr << "RESULT: Specification is realizable.\n";
        } else {
            std::cerr << "RESULT: Specification is unrealizable.\n";
        }

        // Condense Strategy to positional strategies for the individual goals
        std::vector<BF> positionalStrategiesForTheIndividualGoals;

        if (realizable) {

            // Use the strategy dumping data that we have already from the synthesis procedure.
            for (unsigned int i=0;i<livenessGuarantees.size();i++) {
                BF casesCovered = mgr.constantFalse();
                BF strategy = mgr.constantFalse();
                for (auto it = strategyDumpingData.begin();it!=strategyDumpingData.end();it++) {
                    if (it->first == i) {
                        BF newCases = it->second.ExistAbstract(varCubePostOutput) & !casesCovered;
                        strategy |= newCases & it->second;
                        casesCovered |= newCases;
                    }
                }
                positionalStrategiesForTheIndividualGoals.push_back(strategy);
                std::ostringstream filename;
                filename << "/tmp/realizableStratForSystemGoal" << i << ".dot";
                BF_newDumpDot(*this,strategy,"PreInput PreOutput PostInput PostOutput",filename.str().c_str());
            }
        } else {

            strategyDumpingData.clear();

            // Compute Counter-Strategy
            BFFixedPoint mu2(mgr.constantFalse());
            for (;!mu2.isFixedPointReached();) {
                BF nextContraintsForGoals = mgr.constantFalse();
                for (unsigned int j=0;j<livenessGuarantees.size();j++) {
                    BF livetransitions = (!livenessGuarantees[j]) | (mu2.getValue().SwapVariables(varVectorPre,varVectorPost));
                    std::vector<std::pair<unsigned int,BF> > strategyDumpingDataOld = strategyDumpingData;
                    BFFixedPoint nu1(mgr.constantTrue());
                    for (;!nu1.isFixedPointReached();) {
                        strategyDumpingData = strategyDumpingDataOld;
                        livetransitions &= nu1.getValue().SwapVariables(varVectorPre,varVectorPost);
                        BF goodForAllLivenessAssumptions = nu1.getValue();
                        for (unsigned int i=0;i<livenessAssumptions.size();i++) {
                            BF foundPaths = mgr.constantFalse();
                            BFFixedPoint mu0(mgr.constantFalse());
                            for (;!mu0.isFixedPointReached();) {
                                foundPaths = livetransitions & (mu0.getValue().SwapVariables(varVectorPre,varVectorPost) | (livenessAssumptions[i]));
                                foundPaths = (safetyEnv & safetySys.Implies(foundPaths)).UnivAbstract(varCubePostOutput);
                                strategyDumpingData.push_back(std::pair<unsigned int,BF>(i,foundPaths));
                                mu0.update(foundPaths.ExistAbstract(varCubePostInput));
                            }
                            goodForAllLivenessAssumptions &= mu0.getValue();
                        }
                        nu1.update(goodForAllLivenessAssumptions);
                    }
                    nextContraintsForGoals |= nu1.getValue();
                }
                mu2.update(nextContraintsForGoals);
            }
            winningPositions = mu2.getValue();

            // Prepare Positional Strategy
            for (unsigned int i=0;i<livenessAssumptions.size();i++) {
                BF casesCovered = mgr.constantFalse();
                BF strategy = mgr.constantFalse();
                for (auto it = strategyDumpingData.begin();it!=strategyDumpingData.end();it++) {
                    if (it->first == i) {
                        BF actions = it->second.UnivAbstract(varCubePostOutput);
                        BF newCases = actions.ExistAbstract(varCubePost) & !casesCovered;
                        strategy |= newCases & actions;
                        casesCovered |= newCases;
                    }
                }
                positionalStrategiesForTheIndividualGoals.push_back(strategy);
                std::ostringstream filename;
                filename << "/tmp/realizableStratForEnvironmentGoal" << i << ".dot";
                BF_newDumpDot(*this,strategy,"PreInput PreOutput PostInput PostOutput",filename.str().c_str());
            }
        }


        BF currentPosition = mgr.constantFalse();
        unsigned int currentLivenessGuarantee = 0;

        while(true) {

            // The prompt
            std::cout << "> ";
            std::cout.flush();
            std::string command = "";
            while (command=="") {
                std::getline(std::cin,command);
                if (std::cin.eof()) return;
            }

            // Check the command
            boost::trim(command);
            boost::to_upper(command);

            if ((command=="QUIT") || (command=="EXIT")) {
                return;
            } else if (command=="STARTPOS") {
                BF initialPosition = winningPositions & initEnv & initSys;
                assert(!(initialPosition.isFalse()));
                initialPosition = determinize(initialPosition,preVars);
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i, Pre)) {
                        std::cout << " - " << variableNames[i] << ": ";
                        if ((variables[i] & initialPosition).isFalse()) {
                            std::cout << "0\n";
                        } else {
                            std::cout << "1\n";
                        }
                    }
                }
                currentPosition = initialPosition;
            } else if (command=="CHECKTRANS") {

                std::cout << "From: \n";
                BF from = mgr.constantTrue();
                for (unsigned int i=0;i<variables.size();i++) {
                    if ((variableTypes[i]==PreInput) || (variableTypes[i]==PreOutput)) {
                        std::cout << " - " << variableNames[i] << ": ";
                        std::cout.flush();
                        int value;
                        std::cin >> value;
                        if (std::cin.fail()) {
                            std::cout << "    -> Error reading value. Assuming 0.\n";
                            value = 0;
                        }
                        if (value==0) {
                            from &= !variables[i];
                        } else if (value==1) {
                            from &= variables[i];
                        } else {
                            std::cout << "    -> Value != 0 or 1. Assuming 1.\n";
                            from &= variables[i];
                        }
                    }
                }

                std::cout << "To: \n";
                BF to = mgr.constantTrue();
                for (unsigned int i=0;i<variables.size();i++) {
                    if ((variableTypes[i]==PostInput) || (variableTypes[i]==PostOutput)) {
                        std::cout << " - " << variableNames[i] << ": ";
                        std::cout.flush();
                        int value;
                        std::cin >> value;
                        if (std::cin.fail()) {
                            std::cout << "    -> Error reading value. Assuming 0.\n";
                            value = 0;
                        }
                        if (value==0) {
                            from &= !variables[i];
                        } else if (value==1) {
                            from &= variables[i];
                        } else {
                            std::cout << "    -> Value != 0 or 1. Assuming 1.\n";
                            from &= variables[i];
                        }
                    }
                }

                std::cout << "Result: \n";
                if ((from & winningPositions).isFalse()) {
                    std::cout << "- The pre-position is not winning.\n";
                } else {
                    std::cout << "- The pre-position is winning.\n";
                }
                if ((from & to & safetyEnv).isFalse()) {
                    std::cout << "- The transition VIOLATES the SAFETY ASSUMPTIONS\n";
                } else {
                    std::cout << "- The transition SATISFIES the SAFETY ASSUMPTIONS\n";
                }
                if ((from & to & safetySys).isFalse()) {
                    std::cout << "- The transition VIOLATES the SAFETY GUARANTEES\n";
                } else {
                    std::cout << "- The transition SATISFIES the SAFETY GUARANTEES\n";
                }
                std::cout << "- The transition is a goal transition for the following liveness assumptions: ";
                bool foundOne = false;
                for (unsigned int i=0;i<livenessAssumptions.size();i++) {
                    if (!(livenessAssumptions[i] & from & to).isFalse()) {
                        if (foundOne) std::cout << ", ";
                        foundOne = true;
                        std::cout << i;
                    }
                }
                if (!foundOne) std::cout << "none";
                std::cout << std::endl;
                std::cout << "- The transition is a goal transition for the following liveness guarantees: ";
                foundOne = false;
                for (unsigned int i=0;i<livenessGuarantees.size();i++) {
                    if (!(livenessGuarantees[i] & from & to).isFalse()) {
                        if (foundOne) std::cout << ", ";
                        foundOne = true;
                        std::cout << i;
                    }
                }
                if (!foundOne) std::cout << "none";
                std::cout << std::endl;

                // Analyse if it is part of a possible strategy
                std::cout << "- The transition is a possible transition in a strategy for the following goals: ";
                foundOne = false;
                for (unsigned int i=0;i<livenessGuarantees.size();i++) {
                    if (!(positionalStrategiesForTheIndividualGoals[i] & from & to).isFalse()) {
                        if (foundOne) std::cout << ", ";
                        foundOne = true;
                        std::cout << i;
                    }
                }
                if (!foundOne) std::cout << "none";
                std::cout << std::endl;

            } else if (command=="SETPOS") {

                std::cout << "Position: \n";
                BF from = mgr.constantTrue();
                for (unsigned int i=0;i<variables.size();i++) {
                    if ((variableTypes[i]==PreInput) || (variableTypes[i]==PreOutput)) {
                        std::cout << " - " << variableNames[i] << ": ";
                        std::cout.flush();
                        int value;
                        std::cin >> value;
                        if (std::cin.fail()) {
                            std::cout << "    -> Error reading value. Assuming 0.\n";
                            value = 0;
                        }
                        if (value==0) {
                            from &= !variables[i];
                        } else if (value==1) {
                            from &= variables[i];
                        } else {
                            std::cout << "    -> Value != 0 or 1. Assuming 1.\n";
                            from &= variables[i];
                        }
                    }
                }
                currentPosition = from;
            } else if (command=="MOVE") {

                if (currentPosition == mgr.constantFalse()) {
                    std::cout << "Error: The current position is undefined. Use SETPOS prior to calling MOVE." << std::endl;
                } else {

                    std::cout << "Guarantee No.: ";
                    std::cout.flush();
                    unsigned int guarantee;
                    std::cin >> guarantee;
                    if (std::cin.fail()) {
                        std::cout << "    -> Error reading value. Aborting \n";
                    } else if (guarantee>=livenessGuarantees.size()) {
                        std::cout << "    -> Number too large. Aborting \n";
                    } else {

                        BF allowedInputs = (currentPosition & safetyEnv);
                        if (allowedInputs.isFalse()) {
                            std::cout << "No move possible. There is no allowed next input!\n";
                        } else {
                            BF_newDumpDot(*this,allowedInputs,"Pre Post","/tmp/allowedInputs.dot");

                            std::cout << "To: \n";
                            BF to = mgr.constantTrue();
                            BF nextPosition = mgr.constantTrue();
                            for (unsigned int i=0;i<variables.size();i++) {
                                if (variableTypes[i]==PostInput) {
                                    std::cout << " - " << variableNames[i] << ": ";
                                    std::cout.flush();
                                    int value;
                                    std::cin >> value;
                                    if (std::cin.fail()) {
                                        std::cout << "    -> Error reading value. Assuming 0.\n";
                                        value = 0;
                                    }
                                    if (value==0) {
                                        to &= !variables[i];
                                        nextPosition &= !variables[i];
                                    } else if (value==1) {
                                        to &= variables[i];
                                        nextPosition &= variables[i];
                                    } else {
                                        std::cout << "    -> Value != 0 or 1. Assuming 1.\n";
                                        to &= variables[i];
                                    }
                                }
                            }

                            // Simple check for sanity
                            if ((currentPosition & to & safetyEnv).isFalse()) {
                                std::cout << "Warning: that is an illegal next input!\n";
                            }

                            // Compute which transition to takes
                            BF transition = currentPosition & to & positionalStrategiesForTheIndividualGoals[guarantee];


                            if (transition.isFalse()) {
                                std::cout << "    -> Error: Input not allowed here.\n";
                                if (!(currentPosition & to & safetyEnv).isFalse()) {
                                    std::cout << "       -> Actually, that's an internal error!\n";
                                }
                            } else {

                                transition = determinize(transition,postOutputVars);

                                for (unsigned int i=0;i<variables.size();i++) {
                                    if (variableTypes[i]==PostOutput) {
                                        if ((variables[i] & transition).isFalse()) {
                                            std::cout << " - " << variableNames[i] << " = 0\n";
                                            nextPosition &= !variables[i];
                                        } else {
                                            std::cout << " - " << variableNames[i] << " = 1\n";
                                            nextPosition &= variables[i];
                                        }
                                    }
                                }

                                std::cout << "- The transition is a goal transition for the following liveness assumptions: ";
                                bool foundOne = false;
                                for (unsigned int i=0;i<livenessAssumptions.size();i++) {
                                    if (!(livenessAssumptions[i] & transition).isFalse()) {
                                        if (foundOne) std::cout << ", ";
                                        foundOne = true;
                                        std::cout << i;
                                    }
                                }
                                if (!foundOne) std::cout << "none";
                                std::cout << std::endl;
                                std::cout << "- The transition is a goal transition for the following liveness guarantees: ";
                                foundOne = false;
                                for (unsigned int i=0;i<livenessGuarantees.size();i++) {
                                    if (!(livenessGuarantees[i] & transition).isFalse()) {
                                        if (foundOne) std::cout << ", ";
                                        foundOne = true;
                                        std::cout << i;
                                    }
                                }
                                if (!foundOne) std::cout << "none";
                                std::cout << std::endl;

                                // Analyse if it is part of a possible strategy
                                std::cout << "- The transition is a possible transition in a strategy for the following goals: ";
                                foundOne = false;
                                for (unsigned int i=0;i<livenessGuarantees.size();i++) {
                                    if (!(positionalStrategiesForTheIndividualGoals[i] & transition).isFalse()) {
                                        if (foundOne) std::cout << ", ";
                                        foundOne = true;
                                        std::cout << i;
                                    }
                                }
                                if (!foundOne) std::cout << "none";
                                std::cout << std::endl;

                                currentPosition = nextPosition.SwapVariables(varVectorPre,varVectorPost);
                            }
                        }
                    }
                }
            }

            //========================================
            // Simplified functions to be called from
            // other tools.
            //========================================
            else if (command=="XMAKETRANS") {
                std::cout << "\n"; // Get rid of the prompt
                BF postInput = mgr.constantTrue();
                for (unsigned int i=0;i<variables.size();i++) {
                    if (variableTypes[i]==PostInput) {
                        char c;
                        std::cin >> c;
                        if (c=='0') {
                            postInput &= !variables[i];
                        } else if (c=='1') {
                            postInput &= variables[i];
                        } else {
                            std::cerr << "Error: Illegal XMAKETRANS string given.\n";
                        }
                    }
                }
                BF trans = currentPosition & postInput & safetyEnv;
                if (trans.isFalse()) {
                    std::cout << "ERROR\n";
                    if (currentPosition.isFalse()) {
                    }
                } else {
                    trans &= positionalStrategiesForTheIndividualGoals[currentLivenessGuarantee];

                    // Switching goals
                    BF newCombination = determinize(trans,postVars);

                    // Jump as much forward  in the liveness guarantee list as possible ("stuttering avoidance")
                    unsigned int nextLivenessGuarantee = currentLivenessGuarantee;
                    bool firstTry = true;
                    while (((nextLivenessGuarantee != currentLivenessGuarantee) || firstTry) && !((livenessGuarantees[nextLivenessGuarantee] & newCombination).isFalse())) {
                        nextLivenessGuarantee = (nextLivenessGuarantee + 1) % livenessGuarantees.size();
                        firstTry = false;
                    }

                    currentLivenessGuarantee = nextLivenessGuarantee;
                    currentPosition = newCombination.ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);

                    // Print position
                    for (unsigned int i=0;i<variables.size();i++) {
                        if (variableTypes[i]==PreInput) {
                            if ((variables[i] & currentPosition).isFalse()) {
                                std::cout << "0";
                            } else {
                                std::cout << "1";
                            }
                        }
                    }
                    for (unsigned int i=0;i<variables.size();i++) {
                        if (variableTypes[i]==PreOutput) {
                            if ((variables[i] & currentPosition).isFalse()) {
                                std::cout << "0";
                            } else {
                                std::cout << "1";
                            }
                        }
                    }
                    std::cout << "," << currentLivenessGuarantee << std::endl; // Flushes, too.
                }
                std::cout.flush();
            } else if (command=="XPRINTINPUTS") {
                std::cout << "\n"; // Get rid of the prompt
                for (unsigned int i=0;i<variables.size();i++) {
                    if (variableTypes[i]==PreInput)
                        std::cout << variableNames[i] << "\n";
                }
                std::cout << std::endl; // Flushes
            } else if (command=="XPRINTOUTPUTS") {
                std::cout << "\n"; // Get rid of the prompt
                for (unsigned int i=0;i<variables.size();i++) {
                    if (variableTypes[i]==PreOutput)
                        std::cout << variableNames[i] << "\n";
                }
                std::cout << std::endl; // Flushes
            } else if (command=="XGETNOFLIVENESSPROPERTIES") {
                std::cout << "\n"; // Get rid of the prompt
                std::cout << livenessAssumptions.size() << std::endl;
                std::cout << livenessGuarantees.size() << std::endl;
            } else if (command=="XGETINIT") {
                std::cout << "\n"; // Get rid of the prompt
                BF initialPosition = winningPositions & initEnv & initSys;
                initialPosition = determinize(initialPosition,preVars);
                for (unsigned int i=0;i<variables.size();i++) {
                    if (variableTypes[i]==PreInput) {
                        if ((variables[i] & initialPosition).isFalse()) {
                            std::cout << "0";
                        } else {
                            std::cout << "1";
                        }
                    }
                }
                for (unsigned int i=0;i<variables.size();i++) {
                    if (variableTypes[i]==PreOutput) {
                        if ((variables[i] & initialPosition).isFalse()) {
                            std::cout << "0";
                        } else {
                            std::cout << "1";
                        }
                    }
                }
                std::cout << ",0" << std::endl; // Flushes, too.
                currentPosition = initialPosition;
            } else if (command=="XCOMPLETEINIT") {
                // This command takes a set of forced values and completes it such that
                // the result is an initial state that is winning for the
                // player that is normally winning and satisfies all constraints
                //
                // It also returns the "mustness" of the values, i.e., which
                // bits *have* to have a certain value (and whether this depends on the forcing)
                std::cout << "\n"; // Get rid of the prompt
                BF forced  = mgr.constantTrue();
                // First parse the inputs, then the outputs, so that external tools can always set the inputs first
                for (const VariableType &type: {PreInput, PreOutput}) {
                    for (unsigned int i=0;i<variables.size();i++) {
                        if (doesVariableInheritType(i,type)) {
                            char c;
                            std::cin >> c;
                            if (c=='0') {
                                forced &= !variables[i];
                            } else if (c=='1') {
                                forced &= variables[i];
                            } else if (c=='.') {
                                // No forced value
                            } else {
                                std::cerr << "Error: Illegal XCOMPLETEINIT string given.\n";
                            }
                        }
                    }
                }

                BF possibleInitialPositions = initEnv & forced;

                // There exists an allowed initial position
                char result[preVars.size()];
                unsigned int resultPtr = 0;
                for (const VariableType &type: {PreInput, PreOutput}) {
                    for (unsigned int i=0;i<variables.size();i++) {
                        if (doesVariableInheritType(i,type)) {
                            std::cerr << "Considering variable: " << variableNames[i] << std::endl;
                            if ((possibleInitialPositions & variables[i]).isFalse()) {
                                result[resultPtr] = 'a';
                            } else if ((possibleInitialPositions & !variables[i]).isFalse()) {
                                result[resultPtr] = 'A';
                            } else {
                                result[resultPtr] = '.';
                            }
                            resultPtr++;
                        }
                    }
                }                
                assert(resultPtr==preVars.size());

                if (possibleInitialPositions.isFalse()) {
                    std::cout << "FAILASSUMPTIONS" << std::endl;
                } else {
                    BF_newDumpDot(*this,possibleInitialPositions,NULL,"/tmp/init.dot");

                    // Find the newly forced values
                    BF oldInitialPositions = possibleInitialPositions;
                    if (realizable) {
                        possibleInitialPositions &= initSys;
                    } else {
                        possibleInitialPositions &= winningPositions;
                    }

                    if (possibleInitialPositions.isFalse()) {
                        if (realizable) {
                            std::cout << "FAILGUARANTEES" << std::endl;

                            resultPtr = 0;
                            // Print variable assignment that satisfies forced and assumptions
                            BF_newDumpDot(*this,oldInitialPositions,NULL,"/tmp/init.dot");
                            for (const VariableType &type: {PreInput, PreOutput}) {
                                for (unsigned int i=0;i<variables.size();i++) {
                                    if (doesVariableInheritType(i,type)) {
                                        if (result[resultPtr]=='.') {
                                            if (!((oldInitialPositions & !variables[i]).isFalse())) {
                                                result[resultPtr] = '0';
                                                oldInitialPositions &= !variables[i];
                                            } else if (!((oldInitialPositions & variables[i]).isFalse())) {
                                                result[resultPtr] = '1';
                                                oldInitialPositions &= variables[i];
                                            } else {
                                                throw "Fatal error! Should not occur (1).";
                                            }
                                        }
                                        resultPtr++;
                                    }
                                }
                            }

                            // Print position result
                            for (unsigned int i=0;i<preVars.size();i++) {
                                std::cout << result[i];
                            }
                            std::cout << std::endl;
                            // std::cout << livenessAssumption % livenessAssumptions.size() << std::endl;
                            // std::cout << livenessGuarantee % livenessGuarantees.size() << std::endl;
                        } else {
                            std::cout << "FORCEDNONWINNING" << std::endl;
                        }
                    } else {

                        // Determinize part 2
                        resultPtr = 0;
                        for (const VariableType &type: {PreInput, PreOutput}) {
                            for (unsigned int i=0;i<variables.size();i++) {
                                if (doesVariableInheritType(i,type)) {
                                    if (result[resultPtr]=='.') {
                                        if ((possibleInitialPositions & !variables[i]).isFalse()) {
                                            result[resultPtr] = (realizable)?'G':'S';
                                            std::cerr << "!Nabend!\n";
                                        } else if ((possibleInitialPositions & variables[i]).isFalse()) {
                                            result[resultPtr] = (realizable)?'g':'s';
                                        }
                                    }
                                    resultPtr++;
                                }
                            }
                        }

                        oldInitialPositions = possibleInitialPositions;
                        if (realizable) {
                            possibleInitialPositions &= winningPositions;
                        } else {
                            possibleInitialPositions &= initSys;
                        }

                        if (possibleInitialPositions.isFalse()) {
                            if (realizable) {
                                std::cout << "FORCEDNONWINNING" << std::endl;
                            } else {
                                std::cout << "FAILGUARANTEES" << std::endl;

                                resultPtr = 0;
                                BF_newDumpDot(*this,oldInitialPositions,NULL,"/tmp/init.dot");
                                // Print variable assignment that satisfies forced and assumptions
                                for (const VariableType &type: {PreInput, PreOutput}) {
                                    for (unsigned int i=0;i<variables.size();i++) {
                                        if (doesVariableInheritType(i,type)) {
                                            if (result[resultPtr]=='.') {
                                                if (!((oldInitialPositions & !variables[i]).isFalse())) {
                                                    result[resultPtr] = '0';
                                                    oldInitialPositions &= !variables[i];
                                                } else if (!((oldInitialPositions & variables[i]).isFalse())) {
                                                    result[resultPtr] = '1';
                                                    oldInitialPositions &= variables[i];
                                                } else {
                                                    throw "Fatal error! Should not occur (2).";
                                                }
                                            }
                                            resultPtr++;
                                        }
                                    }
                                }

                                // Print position result
                                for (unsigned int i=0;i<preVars.size();i++) {
                                    std::cout << result[i];
                                }
                                std::cout << std::endl;
                                // std::cout << livenessAssumption % livenessAssumptions.size() << std::endl;
                                // std::cout << livenessGuarantee % livenessGuarantees.size() << std::endl;
                            }
                        } else {

                            BF_newDumpDot(*this,possibleInitialPositions,NULL,"/tmp/afterInitSys.dot");

                            // Determinize part 3
                            resultPtr = 0;
                            for (const VariableType &type: {PreInput, PreOutput}) {
                                for (unsigned int i=0;i<variables.size();i++) {
                                    if (doesVariableInheritType(i,type)) {
                                        if (result[resultPtr]=='.') {
                                            if ((possibleInitialPositions & variables[i]).isFalse()) {
                                                result[resultPtr] = (realizable)?'s':'g';
                                            } else if ((possibleInitialPositions & !variables[i]).isFalse()) {
                                                result[resultPtr] = (realizable)?'S':'G';
                                            } else {
                                                result[resultPtr] = '.';
                                            }
                                        }
                                        resultPtr++;
                                    }
                                }
                            }
                            assert(!(possibleInitialPositions.isFalse()));

                            // Determinize the rest
                            resultPtr = 0;
                            for (const VariableType &type: {PreInput, PreOutput}) {
                                for (unsigned int i=0;i<variables.size();i++) {
                                    if (doesVariableInheritType(i,type)) {
                                        if (result[resultPtr]=='.') {
                                            if (!((possibleInitialPositions & !variables[i]).isFalse())) {
                                                result[resultPtr] = '0';
                                                possibleInitialPositions &= !variables[i];
                                            } else if (!((possibleInitialPositions & variables[i]).isFalse())) {
                                                result[resultPtr] = '1';
                                                possibleInitialPositions &= variables[i];
                                            } else {
                                                throw "Fatal error! Should not occur (3).";
                                            }
                                        }
                                        resultPtr++;
                                    }
                                }
                            }
                            BF_newDumpDot(*this,possibleInitialPositions,NULL,"/tmp/finalPossibleInitialPositions.dot");
                            assert(!(possibleInitialPositions.isFalse()));
                            assert(determinize(possibleInitialPositions,preVars)==possibleInitialPositions);

                            // Print result
                            for (unsigned int i=0;i<preVars.size();i++) {
                                std::cout << result[i];
                            }
                            std::cout << std::endl;
                        }
                    }
                }
            } else if (command=="XSTRATEGYTRANSITION") {
                // This command computes the strategy's next move
                std::cout << "\n"; // Get rid of the prompt

                // First parse the inputs, then the outputs, so that external tools can always set the inputs first
                BF startingPoint = mgr.constantTrue();
                for (const VariableType &type: {PreInput, PreOutput}) {
                    for (unsigned int i=0;i<variables.size();i++) {
                        if (doesVariableInheritType(i,type)) {
                            char c;
                            std::cin >> c;
                            if (c=='0') {
                                startingPoint &= !variables[i];
                            } else if (c=='1') {
                                startingPoint &= variables[i];
                            } else {
                                std::cerr << "Error: Illegal XSTRATEGYTRANSITION string given.\n";
                            }
                        }
                    }
                }

                BF forced  = mgr.constantTrue();
                // First parse the inputs, then the outputs, so that external tools can always set the inputs first
                for (const VariableType &type: {PostInput, PostOutput}) {
                    for (unsigned int i=0;i<variables.size();i++) {
                        if (doesVariableInheritType(i,type)) {
                            char c;
                            std::cin >> c;
                            if (c=='0') {
                                forced &= !variables[i];
                            } else if (c=='1') {
                                forced &= variables[i];
                            } else if (c=='.') {
                                // No forced value
                            } else {
                                std::cerr << "Error: Illegal XSTRATEGYTRANSITION string given.\n";
                            }
                        }
                    }
                }

                // Parse current liveness goal numbers
                unsigned int livenessAssumption;
                unsigned int livenessGuarantee;
                std::cin >> livenessAssumption;
                std::cin >> livenessGuarantee;

                std::cerr << "Starting from liveness assumption/guarantee " << livenessAssumption << "/" << livenessGuarantee << std::endl;

                // We have a split here:
                //
                // - For realizable specifications, we augment with safety assumptions and guarantees
                //   first.
                // - For unrealizable ones, we have to consider the strategy before the safety gua-
                //   rantees in order to capture the environment forcing the system to lose in
                //   finite time correctly.
                BF allSoFar = startingPoint & safetyEnv & forced;

                if (allSoFar.isFalse()) {
                    std::cout << "FAILASSUMPTIONS" << std::endl;
                } else {

                    // So there exist transitions. Prepare result array
                    char result[preVars.size()];
                    unsigned int resultPtr = 0;
                    for (const VariableType &type: {PostInput, PostOutput}) {
                        for (unsigned int i=0;i<variables.size();i++) {
                            if (doesVariableInheritType(i,type)) {
                                std::cerr << "Considering variable: " << variableNames[i] << std::endl;
                                if ((allSoFar & variables[i]).isFalse()) {
                                    result[resultPtr] = 'a';
                                } else if ((allSoFar & !variables[i]).isFalse()) {
                                    result[resultPtr] = 'A';
                                } else {
                                    result[resultPtr] = '.';
                                }
                                resultPtr++;
                            }
                        }
                    }
                    assert(resultPtr==postVars.size());

                    // Add next component
                    BF oldAllSoFar = allSoFar;
                    if (realizable) {
                        allSoFar &= safetySys;
                    } else {
                        allSoFar &= positionalStrategiesForTheIndividualGoals[livenessAssumption];
                    }

                    if (allSoFar.isFalse()) {
                        if (realizable) {
                            std::cout << "FAILGUARANTEES" << std::endl;

                            // Print variable assignment that satisfies forced and assumptions
                            resultPtr = 0;
                            for (const VariableType &type: {PostInput, PostOutput}) {
                                for (unsigned int i=0;i<variables.size();i++) {
                                    if (doesVariableInheritType(i,type)) {
                                        if (result[resultPtr]=='.') {
                                            if (!((oldAllSoFar & !variables[i]).isFalse())) {
                                                result[resultPtr] = '0';
                                                oldAllSoFar &= !variables[i];
                                            } else if (!((oldAllSoFar & variables[i]).isFalse())) {
                                                result[resultPtr] = '1';
                                                oldAllSoFar &= variables[i];
                                            } else {
                                                throw "Fatal error! Should not occur (4).";
                                            }
                                        }
                                        resultPtr++;
                                    }
                                }
                            }

                            // Print position result
                            for (unsigned int i=0;i<preVars.size();i++) {
                                std::cout << result[i];
                            }
                            std::cout << std::endl;
                            std::cout << livenessAssumption % livenessAssumptions.size() << std::endl;
                            std::cout << livenessGuarantee % livenessGuarantees.size() << std::endl;
                        } else {
                            std::cout << "FORCEDNONWINNING" << std::endl;
                        }
                    } else {

                        resultPtr = 0;
                        for (const VariableType &type: {PostInput, PostOutput}) {
                            for (unsigned int i=0;i<variables.size();i++) {
                                if (doesVariableInheritType(i,type)) {
                                    std::cerr << "Considering variable: " << variableNames[i] << std::endl;
                                    if (result[resultPtr]=='.') {
                                        if ((allSoFar & variables[i]).isFalse()) {
                                            result[resultPtr] = (realizable)?'g':'s';
                                        } else if ((allSoFar & !variables[i]).isFalse()) {
                                            result[resultPtr] = (realizable)?'G':'S';
                                        }
                                    }
                                    resultPtr++;
                                }
                            }
                        }
                        assert(resultPtr==postVars.size());

                        // Then combine with the strategy (or safetySys)
                        BF_newDumpDot(*this,allSoFar,NULL,"/tmp/allSoFarPre.dot");
                        oldAllSoFar = allSoFar;
                        if (realizable) {
                            allSoFar &= positionalStrategiesForTheIndividualGoals[livenessGuarantee];
                        } else {
                            allSoFar &= safetySys;
                        }

                        BF_newDumpDot(*this,allSoFar,NULL,"/tmp/allSoFarIncludingStrategy.dot");

                        if (allSoFar.isFalse()) {
                            if (realizable) {
                                std::cout << "FORCEDNONWINNING" << std::endl;
                            } else {
                                std::cout << "FAILGUARANTEES" << std::endl;

                                resultPtr = 0;
                                // Print variable assignment that satisfies forced and assumptions
                                for (const VariableType &type: {PostInput, PostOutput}) {
                                    for (unsigned int i=0;i<variables.size();i++) {
                                        if (doesVariableInheritType(i,type)) {
                                            if (result[resultPtr]=='.') {
                                                if (!((oldAllSoFar & !variables[i]).isFalse())) {
                                                    result[resultPtr] = '0';
                                                    oldAllSoFar &= !variables[i];
                                                } else if (!((oldAllSoFar & variables[i]).isFalse())) {
                                                    result[resultPtr] = '1';
                                                    oldAllSoFar &= variables[i];
                                                } else {
                                                    throw "Fatal error! Should not occur (5).";
                                                }
                                            }
                                            resultPtr++;
                                        }
                                    }
                                }

                                // Print position result
                                for (unsigned int i=0;i<preVars.size();i++) {
                                    std::cout << result[i];
                                }
                                std::cout << std::endl;
                                std::cout << livenessAssumption % livenessAssumptions.size() << std::endl;
                                std::cout << livenessGuarantee % livenessGuarantees.size() << std::endl;

                            }
                        } else {
                            // Ok, so we found a suitable transition. Concretize it.

                            // Find the newly forced values
                            resultPtr = 0;
                            for (const VariableType &type: {PostInput, PostOutput}) {
                                for (unsigned int i=0;i<variables.size();i++) {
                                    if (doesVariableInheritType(i,type)) {
                                        if (result[resultPtr]=='.') {
                                            if ((allSoFar & variables[i]).isFalse()) {
                                                result[resultPtr] = (realizable)?'s':'g';
                                            } else if ((allSoFar & !variables[i]).isFalse()) {
                                                result[resultPtr] = (realizable)?'S':'G';
                                            } else {
                                                result[resultPtr] = '.';
                                            }
                                        }
                                        resultPtr++;
                                    }
                                }
                            }

                            // See if we can suggest a next trace element that does not enforce violation
                            BF allSafe = (safetySys & safetyEnv).ExistAbstract(varCubePost);
                            BF candidate = allSafe.SwapVariables(varVectorPost,varVectorPre) & allSoFar;
                            if (!(candidate.isFalse())) allSoFar = candidate;

                            // Determinize the rest
                            resultPtr = 0;
                            for (const VariableType &type: {PostInput, PostOutput}) {
                                for (unsigned int i=0;i<variables.size();i++) {
                                    if (doesVariableInheritType(i,type)) {
                                        if (result[resultPtr]=='.') {
                                            if (!((allSoFar & !variables[i]).isFalse())) {
                                                result[resultPtr] = '0';
                                                allSoFar &= !variables[i];
                                            } else if (!((allSoFar & variables[i]).isFalse())) {
                                                result[resultPtr] = '1';
                                                allSoFar &= variables[i];
                                            } else {
                                                throw "Fatal error! Should not occur (6).";
                                            }
                                        }
                                        resultPtr++;
                                    }
                                }
                            }

                            BF_newDumpDot(*this,allSoFar,NULL,"/tmp/finalTransitionChosen.dot");

                            // Print position result
                            for (unsigned int i=0;i<preVars.size();i++) {
                                std::cout << result[i];
                            }
                            std::cout << std::endl;

                            // Print new assumption and guarantee goal counters
                            if (allSoFar < livenessAssumptions[livenessAssumption]) livenessAssumption++;
                            if (allSoFar < livenessGuarantees[livenessGuarantee]) livenessGuarantee++;
                            std::cout << livenessAssumption % livenessAssumptions.size() << std::endl;
                            std::cout << livenessGuarantee % livenessGuarantees.size() << std::endl;

                        }
                    }
                }

            } else {
                std::cout << "Error: Did not understand command '" << command << "'" << std::endl;
            }
        }

    }


};


#endif
