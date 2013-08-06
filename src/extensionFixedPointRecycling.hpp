#ifndef __EXTENSION_FIXED_POINT_RECYCLING_HPP
#define __EXTENSION_FIXED_POINT_RECYCLING_HPP

#include "gr1context.hpp"

/**
 * Modifies the realizability checking part to recycle the innermost fixed points
 * whenever possible during the game solving process.
 *
 * The idea is from the paper "An improved algorithm for the evaluation of fixpoint expressions"
 * by A. Browne, E.M. Clarke, S. Jha, D.E. Long, and W. Marrerob
 *
 */
template<class T> class XFixedPointRecycling : public T {
protected:

    // Inherited stuff used
    using T::mgr;
    using T::strategyDumpingData;
    using T::livenessGuarantees;
    using T::livenessAssumptions;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::varCubePostInput;
    using T::safetyEnv;
    using T::safetySys;
    using T::winningPositions;

    // Constructor
    XFixedPointRecycling<T>(std::list<std::string> &filenames) : T(filenames) {}

    // Modified synthesis function
    void computeWinningPositions() {

        // The greatest fixed point - called "Z" in the GR(1) synthesis paper
        BFFixedPoint nu2(mgr.constantTrue());

        //std::vector<std::vector<BF>> recyclingInnermostFixedPoints;
        //
        //std::vector<std::vector<BF>> recyclingInnermostFixedPoints[livenessGuarantees.size()][livenessAssumptions.size()];
        unsigned int m = livenessAssumptions.size();
        unsigned int n = livenessGuarantees.size();
        std::vector<std::vector<BF>> recyclingInnermostFixedPoints(n);
        for (unsigned int i = 0; i < n; i++) {
            recyclingInnermostFixedPoints[i].resize(m);
        }
        
        bool firstOutermost = true;

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

                bool firstMiddle = true;

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
                        BFFixedPoint nu0((firstMiddle && (!firstOutermost))?recyclingInnermostFixedPoints[j][i] & nu2.getValue():nu2.getValue());
                        for (;!nu0.isFixedPointReached();) {

                            // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                            foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                            foundPaths &= safetySys;

                            // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                            nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput));
                        }

                        // Update the set of positions that are winning for some liveness assumption
                        goodForAnyLivenessAssumption |= nu0.getValue();

                        recyclingInnermostFixedPoints[j][i] = nu0.getValue();

                        // Dump the paths that we just wound into 'strategyDumpingData' - store the current goal long
                        // with the BDD
                        strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths));
                    }

                    // Update the moddle fixed point
                    mu1.update(goodForAnyLivenessAssumption);

                    firstMiddle = false;
                }

                // Update the set of positions that are winning for any goal for the outermost fixed point
                nextContraintsForGoals &= mu1.getValue();
            }

            // Update the outer-most fixed point
            nu2.update(nextContraintsForGoals);
            firstOutermost = false;

        }

        // We found the set of winning positions
        winningPositions = nu2.getValue();
    }








public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XFixedPointRecycling<T>(filenames);
    }
};



#endif



