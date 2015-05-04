#ifndef __EXTENSION_PERMISSIVE_STRATEGY_HPP__
#define __EXTENSION_PERMISSIVE_STRATEGY_HPP__

#include "gr1context.hpp"

#include <string>

/**
 * An extension to extract an explicit-state permissive strategy, which
 * is also maximal when no liveness assumptions and guarantees are given.
 */
template<class T, bool oneStepRecovery> class XExtractPermissiveExplicitStrategy : public T {
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
    using T::varCubePostInput;
    using T::determinize;
    using T::doesVariableInheritType;
    using T::livenessAssumptions;
    using T::safetySys;

    XExtractPermissiveExplicitStrategy<T,oneStepRecovery>(std::list<std::string> &filenames): T(filenames) {}

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
     *        the system.
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
        BF todoInit = winningPositions;
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
            BF statesCovered = mgr.constantFalse();
            BF stateInputsCovered = mgr.constantFalse();
            BF strategy = mgr.constantFalse();

            // Change: collect transitions as long as a state is not "fixed".
            for (auto it = strategyDumpingData.begin();it!=strategyDumpingData.end();it++) {
                if (it->first == i) {
                    BF newCases = it->second.ExistAbstract(varCubePostOutput) & (!statesCovered);
                    strategy |= (!statesCovered) & it->second;
                    stateInputsCovered |= newCases;
                    statesCovered = ((!safetyEnv) | stateInputsCovered).UnivAbstract(varCubePostInput);
                }
            }
            positionalStrategiesForTheIndividualGoals[i] = strategy;
            //BF_newDumpDot(*this,strategy,"PreInput PreOutput PostInput PostOutput","/tmp/generalStrategy.dot");
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
            outputStream << "State " << stateNum << " with rank " << current.second << "\n";
            bool first = true;
            for (unsigned int i=0;i<variables.size();i++) {
                if (doesVariableInheritType(i,Pre)) {
                    if (first) {
                        first = false;
                    } else {
                        outputStream << ", ";
                    }
                    outputStream << variableNames[i] << ":";
                    outputStream << (((currentPossibilities & variables[i]).isFalse())?"0":"1");
                }
            }

            outputStream << "\n";
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
                // ---- The following line is a modification for the maximally permissive strategy case.
                BF inputCaptured = newCombination;
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
                outputStream << tn << std::endl;
            }
            outputStream << "\n";
        }
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XExtractPermissiveExplicitStrategy<T,oneStepRecovery>(filenames);
    }
};

#endif
