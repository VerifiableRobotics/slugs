#ifndef __EXTENSION_COMPUTE_INTERESTING_RUN_OF_THE_SYSTEM_HPP
#define __EXTENSION_COMPUTE_INTERESTING_RUN_OF_THE_SYSTEM_HPP
#include "gr1context.hpp"

/**
 * An extension that computes an "interesting" run of the synthesized system, i.e., a run in which
 * both the assumptions and the guarantees are fulfilled (forever).
 *
 * Uses the class "StructuredSlugsVariableGroup" defined in "extensionAnalyzeInitialPositions"
 */
template<class T> class XComputeInterestingRunOfTheSystem : public T {
protected:

    using T::initSys;
    using T::safetySys;
    using T::safetyEnv;
    using T::initEnv;
    using T::determinize;
    using T::winningPositions;
    using T::checkRealizability;
    using T::preVars;
    using T::mgr;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::postVars;
    using T::strategyDumpingData;
    using T::varCubePostOutput;
    using T::varCubePre;
    using T::varCubePost;
    using T::varCubePostInput;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::variables;
    using T::variableNames;
    using T::doesVariableInheritType;
    using T::determinizeRandomized;

    BF currentPosition;

    XComputeInterestingRunOfTheSystem<T>(std::list<std::string> &filenames) : T(filenames) {}

public:

    void computeRun() {
        //==================================================
        // First, compute a symbolic strategy for the system
        //==================================================
        // We don't want any reordering from this point onwards, as
        // the BDD manipulations from this point onwards are 'kind of simple'.
        mgr.setAutomaticOptimisation(false);

        // Prepare positional strategies for the individual goals
        std::vector<BF> positionalStrategiesForTheIndividualGoals(livenessGuarantees.size());
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
            positionalStrategiesForTheIndividualGoals[i] = strategy;
            //BF_newDumpDot(*this,strategy,"PreInput PreOutput PostInput PostOutput","/tmp/generalStrategy.dot");
        }

        //=========================================================================================
        // Then, compute a symbolic strategy for the environment to enforce its liveness objectives
        //=========================================================================================
        // 1. Step one: Compute Deadlock-avoiding strategy
        BF nonDeadlockingStates = mgr.constantTrue();
        BF oldNonDeadlockingStates = mgr.constantFalse();
        BF nonDeadlockingTransitions;
        while (nonDeadlockingStates!=oldNonDeadlockingStates) {
            oldNonDeadlockingStates = nonDeadlockingStates;
            nonDeadlockingTransitions = ((nonDeadlockingStates.SwapVariables(varVectorPre,varVectorPost) | !safetySys).UnivAbstract(varCubePostOutput) & safetyEnv);
            nonDeadlockingStates = nonDeadlockingTransitions.ExistAbstract(varCubePostInput);
        }
        //BF_newDumpDot(*this,nonDeadlockingTransitions,NULL,"/tmp/ndt.dot");
        //BF_newDumpDot(*this,nonDeadlockingStates,NULL,"/tmp/nds.dot");

        // 2. Step two: Compute strategy to work towards
        std::vector<BF> environmentStrategyForAchievingTheLivenessAssumptions;
        for (unsigned int i=0;i<livenessAssumptions.size();i++) {
            BF winning = mgr.constantFalse();
            BF oldWinning = mgr.constantFalse();
            BF moves = mgr.constantFalse();
            do {
                oldWinning = winning;
                BF newMoves = (((livenessAssumptions[i] | oldWinning.SwapVariables(varVectorPre,varVectorPost)) & nonDeadlockingTransitions) | (!safetySys)).UnivAbstract(varCubePostOutput);
                winning = newMoves.ExistAbstract(varCubePostInput);
                moves |= winning & !oldWinning & newMoves;
            } while (winning!=oldWinning);
            BF_newDumpDot(*this,winning,NULL,"/tmp/uga.dot");
            BF_newDumpDot(*this,moves,NULL,"/tmp/moves.dot");
            environmentStrategyForAchievingTheLivenessAssumptions.push_back(moves | !winning);
        }




        //====================
        // Compute Runs
        //====================
        std::vector<BF> trace;
        std::vector<int> systemGoals;
        std::vector<bool> systemGoalsSat;
        std::vector<int> environmentGoals;
        std::vector<bool> environmentGoalsSat;
        std::vector<bool> safetyEnvViolation;
        std::vector<bool> safetySysViolation;

        trace.push_back(currentPosition);
        systemGoals.push_back(0);
        environmentGoals.push_back(0);
        for (unsigned int i=0;(i<100) && !(trace.back().isFalse());i++) {
            BF currentPosition = trace.back();

            BF nextPosition = currentPosition & safetyEnv & positionalStrategiesForTheIndividualGoals[systemGoals.back()] & environmentStrategyForAchievingTheLivenessAssumptions[environmentGoals.back()];
            BF edge = determinizeRandomized(nextPosition,postVars);

            //std::ostringstream edgename;
            //edgename << "/tmp/edge" << i << ".dot";
            //BF_newDumpDot(*this,edge,"Pre Post",edgename.str().c_str());

            //std::ostringstream flexname;
            //flexname << "/tmp/flex" << i << ".dot";
            //BF_newDumpDot(*this,currentPosition,"Pre Post",flexname.str().c_str());

            // Sys Liveness progress
            if (edge < livenessGuarantees[systemGoals.back()]) {
                systemGoals.push_back((systemGoals.back() + 1) % livenessGuarantees.size());
                systemGoalsSat.push_back(true);
            } else {
                systemGoals.push_back(systemGoals.back());
                systemGoalsSat.push_back(false);
                assert((edge & livenessGuarantees[systemGoals.back()]).isFalse());
            }

            // Env Liveness progress
            if (edge < livenessAssumptions[environmentGoals.back()]) {
                environmentGoals.push_back((environmentGoals.back() + 1) % livenessAssumptions.size());
                environmentGoalsSat.push_back(true);
            } else {
                environmentGoals.push_back(environmentGoals.back());
                environmentGoalsSat.push_back(false);
                assert((edge & livenessAssumptions[environmentGoals.back()]).isFalse());
            }

            // Violations
            safetyEnvViolation.push_back(!(edge < safetyEnv));
            safetySysViolation.push_back(!(edge < safetySys));

            // Compute next position
            assert(nextPosition < safetySys);
            nextPosition = determinizeRandomized(nextPosition.ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost),preVars);
            trace.push_back(nextPosition);

            //std::ostringstream npname;
            //npname << "/tmp/np" << i << ".dot";
            //BF_newDumpDot(*this,nextPosition,NULL,npname.str().c_str());
        }

        systemGoalsSat.push_back(false);

        //========================
        // Compute variable groups
        //========================
        // Interpret grouped variables (the ones with an '@' in their names)
        // It is assumed that they have precisely the shape produced by the structured
        // slugs parser. This is checked, however, and an exception is thrown otherwise.
        std::map<std::string,StructuredSlugsVariableGroup> variableGroups;
        std::map<unsigned int, std::string> masterVariables;
        std::set<unsigned int> variablesInSomeGroup;
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,Pre)) {
                size_t firstAt = variableNames[i].find_first_of('@');
                if (firstAt!=std::string::npos) {
                    if (firstAt<variableNames[i].size()-1) {
                        std::string prefix = variableNames[i].substr(0,firstAt);
                        // std::cerr << variableNames[i] << std::endl;
                        // There is a latter after the '@'
                        // Now check if this is the master variabe
                        if ((firstAt<variableNames[i].size()-2) && (variableNames[i][firstAt+1]=='0') && (variableNames[i][firstAt+2]=='.')) {
                            // Master variable found. Read the information from the master variable
                            variableGroups[prefix].setMasterVariable(i);
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
                            masterVariables[i] = prefix;
                        } else {
                            // Verify that slave variables are defined in the correct order
                            variableGroups[prefix].slaveVariables.push_back(i);
                            std::istringstream is(variableNames[i].substr(firstAt+1,std::string::npos));
                            unsigned int number;
                            is >> number;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 4)";
                            if (number!=variableGroups[prefix].slaveVariables.size()) throw std::string("Error: in the variable groups introduced by the structured slugs parser, the slave variables are not in the right order: ")+variableNames[i];
                        }
                    }
                    variablesInSomeGroup.insert(i);
                }
            }
        }

        //===========================
        // Compute table for the run
        //===========================
        std::vector<std::vector<std::string> > table;
        int nofInputColumns = 0;
        for (unsigned int runPart = 0;runPart<trace.size();runPart++) {
            table.push_back(std::vector<std::string>());
            std::vector<std::string> &thisLine = table.back();
            BF thisState = trace[runPart];

            // Environment goal number
            std::ostringstream environmentGoalNumber;
            environmentGoalNumber << environmentGoals[runPart];
            environmentGoalNumber << ((environmentGoalsSat[runPart])?"+":" ");
            if (safetyEnvViolation[runPart]) environmentGoalNumber << " (safety prop. violation)";
            thisLine.push_back(environmentGoalNumber.str());
            thisLine.push_back("|");

            // System goal number
            std::ostringstream systemGoalNumber;
            systemGoalNumber << systemGoals[runPart];
            systemGoalNumber << ((systemGoalsSat[runPart])?"+":" ");
            if (safetySysViolation[runPart]) environmentGoalNumber << " (safety prop. violation)";
            thisLine.push_back(systemGoalNumber.str());
            thisLine.push_back("|");

            if (!(thisState.isFalse())) {
                // Input ungrouped
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i,PreInput)) {
                        if (variablesInSomeGroup.count(i)==0) {
                            if ((thisState & variables[i]).isFalse()) {
                                thisLine.push_back("!"+variableNames[i]);
                            } else {
                                thisLine.push_back(variableNames[i]);
                            }
                            if (runPart==0) nofInputColumns++;
                        }
                    }
                }

                // Input Grouped
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i,PreInput)) {
                        if (variablesInSomeGroup.count(i)!=0) {
                            auto it = masterVariables.find(i);
                            if (it!=masterVariables.end()) {

                                // This is a master variable
                                std::ostringstream valueStream;
                                std::string key = it->second;
                                valueStream << key;
                                StructuredSlugsVariableGroup &group = variableGroups[key];
                                unsigned int value = 0;
                                if (!((thisState & variables[group.masterVariable]).isFalse())) value++;

                                // Process slave variables
                                unsigned int multiplier=2;
                                for (auto it = group.slaveVariables.begin();it!=group.slaveVariables.end();it++) {
                                    if (!((thisState & variables[*it]).isFalse())) value += multiplier;
                                    multiplier *= 2;
                                }
                                valueStream << "=" << (value+group.minValue);
                                thisLine.push_back(valueStream.str());
                                if (runPart==0) nofInputColumns++;
                            }
                        }
                    }
                }

                // Seperator
                thisLine.push_back("|");

                // Output ungrouped
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i,PreOutput)) {
                        if (variablesInSomeGroup.count(i)==0) {
                            if ((thisState & variables[i]).isFalse()) {
                                thisLine.push_back("!"+variableNames[i]);
                            } else {
                                thisLine.push_back(variableNames[i]);
                            }
                        }
                    }
                }

                // Output Grouped
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i,PreOutput)) {
                        if (variablesInSomeGroup.count(i)!=0) {
                            auto it = masterVariables.find(i);
                            if (it!=masterVariables.end()) {

                                // This is a master variable
                                std::ostringstream valueStream;
                                std::string key = it->second;
                                valueStream << key;
                                StructuredSlugsVariableGroup &group = variableGroups[key];
                                unsigned int value = 0;
                                if (!((thisState & variables[group.masterVariable]).isFalse())) value++;

                                // Process slave variables
                                unsigned int multiplier=2;
                                for (auto it = group.slaveVariables.begin();it!=group.slaveVariables.end();it++) {
                                    if (!((thisState & variables[*it]).isFalse())) value += multiplier;
                                    multiplier *= 2;
                                }
                                valueStream << "=" << (value+group.minValue);
                                thisLine.push_back(valueStream.str());
                            }
                        }
                    }
                }


            } else {
                // Either the Environment or the system loses
                BF newMoves = trace[runPart-1] & safetyEnv;
                if (newMoves.isFalse()) {
                    thisLine.push_back("Env.Loses");
                } else {
                    thisLine.push_back("Sys.Loses");
                    //BF_newDumpDot(*this,newMoves,NULL,"/tmp/newmoves.dot");
                    //BF_newDumpDot(*this,trace[runPart-1],NULL,"/tmp/thisState.dot");
                }
                while (thisLine.size()!=table[0].size()) thisLine.push_back("");
            }

            assert(thisLine.size()==table[0].size());
        }

        //===========================
        // Print table for the run
        //===========================
        // Compute column lengths
        std::vector<size_t> maxLengths;
        maxLengths.push_back(8);
        maxLengths.push_back(1);
        maxLengths.push_back(8);
        maxLengths.push_back(1);
        for (unsigned int i=4;i<table[0].size();i++) {
            maxLengths.push_back(0);
        }
        for (auto it = table.begin();it!=table.end();it++) {
            for (unsigned int i=0;i<it->size();i++) {
                maxLengths[i] = std::max(maxLengths[i],(*it)[i].length());
            }
        }

        // Print column headers
        std::cout << "Env.L.A. | Sys.L.G. | Input";
        int inputColsLeft = -5;
        for (int i=4;i<nofInputColumns+4;i++) {
            inputColsLeft+=maxLengths[i]+1;
        }
        for (int i=0;i<inputColsLeft;i++) {
            std::cout << " ";
        }
        std::cout << "| Output";
        int outputColsLeft = -7;
        for (unsigned int i=nofInputColumns+5;i<maxLengths.size();i++) {
            outputColsLeft+=maxLengths[i]+1;
        }
        for (int i=0;i<outputColsLeft;i++) {
            std::cout << " ";
        }
        std::cout << "\n---------+----------+-";
        for (int i=0;i<inputColsLeft+5;i++) {
            std::cout << "-";
        }
        std::cout << "+";
        for (int i=0;i<outputColsLeft+7;i++) {
            std::cout << "-";
        }
        std::cout << "\n";

        // Print columns
        for (auto it=table.begin();it!=table.end();it++) {
            for (unsigned int i=0;i<maxLengths.size();i++) {
                for (unsigned int j=(*it)[i].length();j<maxLengths[i];j++) std::cout << " ";
                std::cout << (*it)[i] << " ";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }


    void execute() {
        checkRealizability();
        currentPosition = winningPositions & initEnv & initSys;
        if (currentPosition.isFalse()) {
            std::cout << "There is no position that is winning for the system and that satisfies both the initialization assumptions and the initialization guarantees.\n";
            return;
        }
        currentPosition = determinize(currentPosition,preVars);
        computeRun();
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XComputeInterestingRunOfTheSystem<T>(filenames);
    }
};




#endif

