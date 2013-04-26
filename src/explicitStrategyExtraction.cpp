//=====================================================================
// Explicit-state strategy extraction (LTLMop compatible)
//=====================================================================
#include "gr1context.hpp"

#include <vector>
#include <map>
#include <list>
#include <sstream>

/**
 * @brief A function that takes a BF "in" over the set of variables "var" and returns a new BF over the same variables
 *        that only represents one concrete variable valuation in "in" to the variables in "var"
 * @param in a BF to determinize
 * @param vars the care set of variables
 * @return the determinized BF
 */
BF determinize(BF in, std::vector<BF> vars) {
    for (auto it = vars.begin();it!=vars.end();it++) {
        BF res = in & !(*it);
        if (res.isFalse()) {
            in = in & *it;
        } else {
            in = res;
        }
    }
    return in;
}

/**
 * @brief Compute and print out (to stdout) an explicit-state strategy that is winning for
 *        the system. The output is compatible with the old JTLV output of LTLMoP.
 *        This function requires that the realizability of the specification has already been
 *        detected and that the variables "strategyDumpingData" and
 *        "winningPositions" have been filled by the synthesis algorithm with meaningful data.
 * @param outputStream - Where the strategy shall be printed to.
 */
void GR1Context::computeAndPrintExplicitStateStrategy(std::ostream &outputStream) {

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
    BF todoInit = (winningPositions & initSys);
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
        outputStream << "State " << stateNum << " with rank " << current.second << " -> <";
        bool first = true;
        for (unsigned int i=0;i<variables.size();i++) {
            if (variableTypes[i] < PostInput) {
                if (first) {
                    first = false;
                } else {
                    outputStream << ", ";
                }
                outputStream << variableNames[i] << ":";
                outputStream << (((currentPossibilities & variables[i]).isFalse())?"0":"1");
            }
        }

        outputStream << ">\n\tWith successors : ";
        first = true;

        // Compute successors for all variables that allow these
        currentPossibilities &= positionalStrategiesForTheIndividualGoals[current.second];
        BF goalSwitchingTransitions = (currentPossibilities & livenessGuarantees[current.second]).ExistAbstract(varCubePre);

        // Switching goals
        while (!(goalSwitchingTransitions.isFalse())) {
            BF newCombination = determinize(goalSwitchingTransitions,postVars);

            // Jump as much forward  in the liveness guarantee list as possible ("stuttering avoidance")
            unsigned int nextLivenessGuarantee = (current.second + 1) % livenessGuarantees.size();
            while ((nextLivenessGuarantee != current.second) && !((livenessGuarantees[nextLivenessGuarantee] & newCombination).isFalse()))
                nextLivenessGuarantee = (nextLivenessGuarantee + 1) % livenessGuarantees.size();

            // Mark which input has been captured by this case
            BF inputCaptured = newCombination.ExistAbstract(varCubePostOutput);
            newCombination = newCombination.SwapVariables(varVectorPre,varVectorPost);
            currentPossibilities &= !inputCaptured;
            goalSwitchingTransitions &= !inputCaptured;

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

        BF nongoalSwitchingTransitions = currentPossibilities.ExistAbstract(varCubePre);

        while (!(nongoalSwitchingTransitions.isFalse())) {
            // Get new input
            BF newCombination = determinize(nongoalSwitchingTransitions,postVars);
            BF inputCaptured = newCombination.ExistAbstract(varCubePostOutput);
            newCombination = newCombination.SwapVariables(varVectorPre,varVectorPost);
            nongoalSwitchingTransitions &= !inputCaptured;

            // Search for newCombination
            unsigned int tn;
            std::pair<size_t, unsigned int> target = std::pair<size_t, unsigned int>(newCombination.getHashCode(),current.second);
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
        outputStream << "\n";
    }
}
