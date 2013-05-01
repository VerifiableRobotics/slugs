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
 */
void GR1Context::computeAndPrintExplicitStateStrategy() {

    // List of states in existance so far. The first map
    // maps from a BF node pointer (for the pre variable valuation) and a goal
    // to a state number. The vector then contains the concrete valuation.
    std::map<std::pair<size_t, uint>, uint > lookupTableForPastStates;
    std::vector<BF> bfsUsedInTheLookupTable;
    std::list<std::pair<size_t, uint> > todoList;

    // Prepare initial to-do list from the allowed initial states
    BF todoInit = (winningPositions & initSys);
    while (!(todoInit.isFalse())) {
        BF concreteState = determinize(todoInit,preVars);
        std::pair<size_t, uint> lookup = std::pair<size_t, uint>(concreteState.getHashCode(),0);
        lookupTableForPastStates[lookup] = bfsUsedInTheLookupTable.size();
        bfsUsedInTheLookupTable.push_back(concreteState);
        todoInit &= !concreteState;
        todoList.push_back(lookup);
    }

    // Prepare positional strategies for the individual goals
    BF positionalStrategiesForTheIndividualGoals[livenessGuarantees.size()];
    for (uint i=0;i<livenessGuarantees.size();i++) {
        BF casesCovered = mgr.constantFalse();
        BF strategy = mgr.constantFalse();
        for (auto it = strategyDumpingData.begin();it!=strategyDumpingData.end();it++) {
            if (it->first == i) {
                BF newCases = it->second.ExistAbstract(varCubePostControllerOutput) & !casesCovered;
                strategy |= newCases & it->second;
                casesCovered |= newCases;
            }
        }
        positionalStrategiesForTheIndividualGoals[i] = strategy;
        //BF_newDumpDot(*this,strategy,"PreInput PreOutput PostInput PostOutput","/tmp/generalStrategy.dot");
    }

    // Extract strategy
    while (todoList.size()>0) {
        std::pair<size_t, uint> current = todoList.front();
        todoList.pop_front();
        uint stateNum = lookupTableForPastStates[current];
        BF currentPossibilities = bfsUsedInTheLookupTable[stateNum];

        /*{
            std::ostringstream filename;
            filename << "/tmp/state" << stateNum << ".dot";
            BF_newDumpDot(*this,currentPossibilities,"PreInput PreOutput PostInput PostOutput",filename.str());
        }*/

        // Print state information
        std::cout << "State " << stateNum << " with rank " << current.second << " -> <";
        bool first = true;
        for (uint i=0;i<variables.size();i++) {
            if (variableTypes[i] < PostInput) {
                if (first) {
                    first = false;
                } else {
                    std::cout << ", ";
                }
                std::cout << variableNames[i] << ":";
                std::cout << (((currentPossibilities & variables[i]).isFalse())?"0":"1");
            }
        }

        std::cout << ">\n\tWith successors : ";
        first = true;

        // Compute successors for all variables that allow these
        currentPossibilities &= positionalStrategiesForTheIndividualGoals[current.second];
        BF goalSwitchingTransitions = (currentPossibilities & livenessGuarantees[current.second]).ExistAbstract(varCubePre);

        // Switching goals
        while (!(goalSwitchingTransitions.isFalse())) {
            BF newCombination = determinize(goalSwitchingTransitions,postVars);
            BF inputCaptured = newCombination.ExistAbstract(varCubePostOutput);
            for  {
            newCombination = newCombination.SwapVariables(varVectorPre,varVectorPost);
            currentPossibilities &= !inputCaptured;
            goalSwitchingTransitions &= !inputCaptured;

            // Search for newCombination
            uint tn;
            std::pair<size_t, uint> target = std::pair<size_t, uint>(newCombination.getHashCode(),(current.second + 1) % livenessGuarantees.size());
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
                std::cout << ", ";
            }
            std::cout << tn;
        }
        }

        BF nongoalSwitchingTransitions = currentPossibilities.ExistAbstract(varCubePre);

        while (!(nongoalSwitchingTransitions.isFalse())) {
            // Get new input
            BF newCombination = determinize(nongoalSwitchingTransitions,postVars);
            BF inputCaptured = newCombination.ExistAbstract(varCubePostOutput);
            newCombination = newCombination.SwapVariables(varVectorPre,varVectorPost);
            nongoalSwitchingTransitions &= !inputCaptured;

            // Search for newCombination
            uint tn;
            std::pair<size_t, uint> target = std::pair<size_t, uint>(newCombination.getHashCode(),current.second);
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
                std::cout << ", ";
            }
            std::cout << tn;
        }
        std::cout << "\n";
    }
}
