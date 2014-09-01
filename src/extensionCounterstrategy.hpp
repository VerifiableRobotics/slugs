#ifndef __EXTENSION_COUNTERSTRATEGY_HPP
#define __EXTENSION_COUNTERSTRATEGY_HPP

#include "gr1context.hpp"
#include <string>
#include "boost/tuple/tuple.hpp"

/**
 * A class that computes a counterstrategy for an unrealizable specification
 *
 * Includes support for the special robotics semantic
 */
template<class T, bool specialRoboticsSemantics> class XCounterStrategy : public T {
protected:

    // Inherited stuff used
    using T::mgr;
    using T::livenessGuarantees;
    using T::livenessAssumptions;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::varCubePostInput;
    using T::varCubePreOutput;
    using T::varCubePreInput;
    using T::safetyEnv;
    using T::safetySys;
    using T::initEnv;
    using T::initSys;
    using T::winningPositions;
    using T::realizable;

    std::vector<boost::tuple<unsigned int, unsigned int,BF> > strategyDumpingData;
    
    // Constructor
    XCounterStrategy<T,specialRoboticsSemantics>(std::list<std::string> &filenames) : T(filenames) {}

public:

    /**
     * @brief Countersynthesis algorithm - adds to 'strategyDumpingData' transitions
     * that represent a winning strategy for the environment.
     * The algorithm implemented is transition-based, and not state-based.
     * So it computes:
     *
     * mu Z. \/_j nu Y. /\_i mu X. cox( (transitionTo(Z) \/ !G^sys_j) /\ transitionTo(Y) /\ (transitionTo(X) \/ G^env_i))
     *
     * where the cox operator computes from which positions the environment player can enforce that certain *transitions* are taken.
     * the transitionTo function takes a set of target positions and computes the set of transitions that are allowed for
     * the environment and that lead to the set of target positions
     **/
    
 void computeWinningPositions() {

    // The greatest fixed point - called "Z" in the GR(1) synthesis paper
    BFFixedPoint mu2(mgr.constantFalse());

    // Iterate until we have found a fixed point
    for (;!mu2.isFixedPointReached();) {

        // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
        // goal. Then, after we have iterated over the goals, we can update mu2.
        BF nextContraintsForGoals = mgr.constantFalse();
        for (unsigned int j=0;j<livenessGuarantees.size();j++) {

            // To extract a counterstrategy in case of unrealizability, we need to store a sequence of 'preferred' transitions in the
            // game structure. These preferred transitions only need to be computed during the last execution of the middle
            // greatest fixed point. Since we don't know which one is the last one, we store them in every iteration,
            // so that after the last iteration, we obtained the necessary data. Before any new iteration, we need to
            // store the old date so that it can be discarded if needed
            std::vector<boost::tuple<unsigned int, unsigned int,BF> > strategyDumpingDataOld = strategyDumpingData;

            // Start computing the transitions that lead closer to the goal and lead to a position that is not yet known to be losing (for the environment).
            // Start with the ones that actually represent reaching the goal (which is a transition in this implementation as we can have
            // nexts in the goal descriptions).
            BF livetransitions = (!livenessGuarantees[j]) | (mu2.getValue().SwapVariables(varVectorPre,varVectorPost));

            // Compute the middle least-fixed point (called 'Y' in the GR(1) paper)
            BFFixedPoint nu1(mgr.constantTrue());
            for (;!nu1.isFixedPointReached();) {

                // New middle iteration has begun -> revert to the data before the first iteration.
                strategyDumpingData = strategyDumpingDataOld;

                // Update the set of transitions that lead closer to the goal.
                livetransitions &= nu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                // Iterate over the liveness assumptions. Store the positions that are found to be winning for *all*
                // of them into the variable 'goodForAnyLivenessAssumption'.
                BF goodForAllLivenessAssumptions = nu1.getValue();
                for (unsigned int i=0;i<livenessAssumptions.size();i++) {

                    // Prepare the variable 'foundPaths' that contains the transitions that stay within the inner-most
                    // greatest fixed point or get closer to the goal. Only used for counterstrategy extraction
                    BF foundPaths = mgr.constantFalse();

                    // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                    BFFixedPoint mu0(mgr.constantFalse());
                    for (;!mu0.isFixedPointReached();) {

                        // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                        foundPaths = livetransitions & (mu0.getValue().SwapVariables(varVectorPre,varVectorPost) | (livenessAssumptions[i]));
                        foundPaths = (safetyEnv & safetySys.Implies(foundPaths)).UnivAbstract(varCubePostOutput);

                        // Dump the paths that we just found into 'strategyDumpingData' - store the current goal
                        // with the BDD
                        strategyDumpingData.push_back(boost::make_tuple(i,j,foundPaths));

                        // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                        mu0.update(foundPaths.ExistAbstract(varCubePostInput));
                    }

                    // Update the set of positions that are winning for some liveness assumption
                    goodForAllLivenessAssumptions &= mu0.getValue();

                }

                // Update the middle fixed point
                nu1.update(goodForAllLivenessAssumptions);
            }

            // Update the set of positions that are winning for some environment goal for the outermost fixed point
            nextContraintsForGoals |= nu1.getValue();
        }

        // Update the outer-most fixed point
        mu2.update(nextContraintsForGoals);

    }

    // We found the set of winning positions
    winningPositions = mu2.getValue();
}

void checkRealizability() {

    computeWinningPositions();

    // Check if for every possible environment initial position the system has a good system initial position
    BF result;
    if (specialRoboticsSemantics) {
        result = (initEnv & initSys & winningPositions).ExistAbstract(varCubePreOutput).ExistAbstract(varCubePreInput);
    } else {
        result = (initEnv & (initSys.Implies(winningPositions)).UnivAbstract(varCubePreOutput)).ExistAbstract(varCubePreInput);
    }

    // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
    if (!result.isConstant()) throw "Internal error: Could not establish realizability/unrealizability of the specification.";

    // Return the result in Boolean form.
    realizable = !result.isTrue();
}


    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XCounterStrategy<T,specialRoboticsSemantics>(filenames);
    }
};

#endif

