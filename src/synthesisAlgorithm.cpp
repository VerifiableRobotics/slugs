#include "gr1context.hpp"

/**
 * @brief Compute the winning positions. Stores the information that are later needed to
 *        construct an implementation in case of realizability to the variable "strategyDumpingData".
 *        The algorithm implemented is transition-based, and not state-based (as the one in the GR(1) synthesis paper).
 *        So it rather computes:
 *
 *        nu Z. /\_j mu Y. \/_i nu X. cox( transitionTo(Z) /\ G^sys_j \/ transitionTo(Y) \/ (!G^env_i /\ transitionTo(X))
 *
 *        where the cox operator computes from which positions the system player can enforce that certain *transitions* are taken.
 *        the transitionTo function takes a set of target positions and computes the set of transitions that are allowed for
 *        the system and that lead to the set of target positions
 */
 void GR1Context::computeWinningPositions() {

    // The greatest fixed point - called "Z" in the GR(1) synthesis paper
    BFFixedPoint nu2(mgr.constantTrue());

    // Iterate until we have found a fixed point
    for (;!nu2.isFixedPointReached();) {

        // To extract a strategy in case of realizability, we need to store a sequence of 'preferred' transitions in the
        // game structure. These preferred transitions only need to be computed during the last execution of the outermost
        // greatest fixed point. Since we don't know which one is the last one, we store them in every iteration,
        // so that after the last iteration, we obtained the necessary data. Before any new iteration, we need to
        // clear the old data, though.
        strategyDumpingData.clear();

        // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
        // goal. Then, after we have iterated over the goals, we can update nu2.
        BF nextContraintsForGoals = mgr.constantTrue();
        for (unsigned int j=0;j<livenessGuarantees.size();j++) {

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

                    // Dump the paths that we just wound into 'strategyDumpingData' - store the current goal long
                    // with the BDD
                    strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths));
                }

                // Update the moddle fixed point
                mu1.update(goodForAnyLivenessAssumption);
            }

            // Update the set of positions that are winning for any goal for the outermost fixed point
            nextContraintsForGoals &= mu1.getValue();
        }

        // Update the outer-most fixed point
        nu2.update(nextContraintsForGoals);

    }

    // We found the set of winning positions
    winningPositions = nu2.getValue();
}


void GR1Context::checkRealizability() {

    computeWinningPositions();

    // Check if for every possible environment initial position the system has a good system initial position
    BF result;
    result = initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput);

    // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
    if (!result.isConstant()) {
        BF_newDumpDot(*this,result,NULL,"/tmp/isRealizable.dot");
        throw "Internal error: Could not establish realizability/unrealizability of the specification.";
    }

    // Return the result in Boolean form.
    realizable = result.isTrue();
}
