#ifndef __EXTENSION_EXTRACT_STRATEGY_HPP
#define __EXTENSION_EXTRACT_STRATEGY_HPP

#include "gr1context.hpp"
#include <string>

/**
 * An extension that triggers that a strategy is actually extracted.
 *
 *  This extension contains some code by Github user "johnyf", responsible for producing JSON output.
 */
template<class T, bool oneStepRecovery, bool jsonOutput> class XExtractExplicitStrategy : public T {
protected:
    // New variables
    std::string outputFilename;

    // Inherited stuff used
    using T::mgr;
    using T::winningPositions;
    using T::initSys;
    using T::initEnv;
    using T::preVars;
    using T::livenessGuarantees;
    using T::strategyDumpingData;
    using T::variables;
    using T::safetyEnv;
    using T::variableTypes;
    using T::realizable;
    using T::postVars;
    using T::varCubePre;
    using T::variableNames;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::determinize;
    using T::doesVariableInheritType;

    XExtractExplicitStrategy<T,oneStepRecovery,jsonOutput>(std::list<std::string> &filenames): T(filenames) {}

    void init(std::list<std::string> &filenames) {
        T::init(filenames);
        if (filenames.size()==0) {
            outputFilename = "";
        } else {
            outputFilename = filenames.front();
            filenames.pop_front();
        }
    }

public:

    void execute() {
        T::execute();
        if (realizable) {
            if (outputFilename=="") {
                computeAndPrintExplicitStateStrategy(std::cout);
            } else {
                std::ofstream of(outputFilename.c_str());
                if (of.fail()) {
                    SlugsException ex(false);
                    ex << "Error: Could not open output file'" << outputFilename << "\n";
                    throw ex;
                }
                computeAndPrintExplicitStateStrategy(of);
                if (of.fail()) {
                    SlugsException ex(false);
                    ex << "Error: Writing to output file'" << outputFilename << "failed. \n";
                    throw ex;
                }
                of.close();
            }
        }
    }

    /**
     * @brief Compute and print out (to stdout) an explicit-state strategy that is winning for
     *        the system. The output is compatible with the old JTLV output of LTLMoP.
     *        This function requires that the realizability of the specification has already been
     *        detected and that the variables "strategyDumpingData" and
     *        "winningPositions" have been filled by the synthesis algorithm with meaningful data.
     * @param outputStream - Where the strategy shall be printed to.
     */
    void computeAndPrintExplicitStateStrategy(std::ostream &outputStream) {

        // We don't want any reordering from this point onwards, as
        // the BDD manipulations from this point onwards are 'kind of simple'.
        mgr.setAutomaticOptimisation(false);

        // List of states in existance so far. The first map
        // maps from a BF node pointer (for the pre variable valuation) and a goal
        // to a state number. The vector then contains the concrete valuation.
        std::map<std::pair<size_t, unsigned int>, unsigned int > lookupTableForPastStates;
        std::vector<BF> bfsUsedInTheLookupTable;
        std::list<std::pair<size_t, unsigned int> > todoList;

        // Prepare initial to-do list from the allowed initial states
        BF todoInit = (oneStepRecovery)?(winningPositions & initSys):(winningPositions & initSys & initEnv);
        while (!(todoInit.isFalse())) {
            BF concreteState = determinize(todoInit,preVars);
            std::pair<size_t, unsigned int> lookup = std::pair<size_t, unsigned int>(concreteState.getHashCode(),0);
            lookupTableForPastStates[lookup] = bfsUsedInTheLookupTable.size();
            bfsUsedInTheLookupTable.push_back(concreteState);
            todoInit &= !concreteState;
            todoList.push_back(lookup);
        }

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

        // Print JSON Header if JSON output is desired
        if (jsonOutput) {
            outputStream << "{\"version\": 0,\n \"slugs\": \"0.0.1\",\n\n";

            // print names of variables
            bool first = true;
            outputStream << " \"variables\": [";
            for (unsigned int i=0; i<variables.size(); i++) {
                if (doesVariableInheritType(i, Pre)) {
                    if (first) {
                        first = false;
                    } else {
                        outputStream << ", ";
                    }
                    outputStream << "\"" << variableNames[i] << "\"";
                }
            }
            outputStream << "],\n\n \"nodes\": {\n";
        }

        // Extract strategy
        while (todoList.size()>0) {
            std::pair<size_t, unsigned int> current = todoList.front();
            todoList.pop_front();
            unsigned int stateNum = lookupTableForPastStates[current];
            BF currentPossibilities = bfsUsedInTheLookupTable[stateNum];

            /*{
                std::ostringstream filename;
                filename << "/tmp/state" << stateNum << ".dot";
                BF_newDumpDot(*this,currentPossibilities,"PreInput PreOutput PostInput PostOutput",filename.str());
            }*/

            // Print state information
            if (jsonOutput) {
                outputStream << "\"" << stateNum << "\": {\n\t\"rank\": " << current.second << ",\n\t\"state\": [";
            } else {
                outputStream << "State " << stateNum << " with rank " << current.second << " -> <";
            }

            bool first = true;
            for (unsigned int i=0;i<variables.size();i++) {
                if (doesVariableInheritType(i,Pre)) {
                    if (first) {
                        first = false;
                    } else {
                        outputStream << ", ";
                    }
                    if (!jsonOutput) outputStream << variableNames[i] << ":";
                    outputStream << (((currentPossibilities & variables[i]).isFalse())?"0":"1");
                }
            }
            if (jsonOutput) {
                outputStream << "],\n";  // end of state list
                // start list of successors
                outputStream << "\t\"trans\": [";
            } else {
                outputStream << ">\n\tWith successors : ";
            }
            first = true;

            // Compute successors for all variables that allow these
            currentPossibilities &= positionalStrategiesForTheIndividualGoals[current.second];
            BF remainingTransitions =
                    (oneStepRecovery)?
                    currentPossibilities:
                    (currentPossibilities & safetyEnv);

            // Switching goals
            while (!(remainingTransitions.isFalse())) {
                BF newCombination = determinize(remainingTransitions,postVars);

                // Jump as much forward  in the liveness guarantee list as possible ("stuttering avoidance")
                unsigned int nextLivenessGuarantee = current.second;
                bool firstTry = true;
                while (((nextLivenessGuarantee != current.second) || firstTry) && !((livenessGuarantees[nextLivenessGuarantee] & newCombination).isFalse())) {
                    nextLivenessGuarantee = (nextLivenessGuarantee + 1) % livenessGuarantees.size();
                    firstTry = false;
                }

                // Mark which input has been captured by this case
                BF inputCaptured = newCombination.ExistAbstract(varCubePostOutput);
                newCombination = newCombination.ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);
                remainingTransitions &= !inputCaptured;

                // Search for newCombination
                unsigned int tn;
                std::pair<size_t, unsigned int> target = std::pair<size_t, unsigned int>(newCombination.getHashCode(),nextLivenessGuarantee);
                if (lookupTableForPastStates.count(target)==0) {
                    tn = lookupTableForPastStates[target] = bfsUsedInTheLookupTable.size();
                    bfsUsedInTheLookupTable.push_back(newCombination);
                    todoList.push_back(target);
                } else {
                    tn = lookupTableForPastStates[target];
                }

                // Print
                if (first) {
                    first = false;
                } else {
                    outputStream << ", ";
                }

                outputStream << tn;
            }

            if (jsonOutput) {
                outputStream << "]\n}";
                if (!(todoList.empty())) {
                    outputStream << ",";
                }
                outputStream << "\n\n";
            } else {
                outputStream << "\n";
            }
        }
        if (jsonOutput) {
            // close "nodes" dict and json object
            outputStream << "}}\n";
        }
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XExtractExplicitStrategy<T,oneStepRecovery,jsonOutput>(filenames);
    }
};

#endif
