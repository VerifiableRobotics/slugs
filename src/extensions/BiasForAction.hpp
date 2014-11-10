#ifndef __EXTENSION_BIAS_FOR_ACTION_HPP
#define __EXTENSION_BIAS_FOR_ACTION_HPP

#include "gr1context.hpp"
#include <string>

/**
 * A class that modifies the core implementation of the GR(1) synthesis algorithm to make sure that in
 * the strategy that we extract from the setting, moves that bring us closer to the goal without relying on
 * the environment are preferred over those that do not.
 */
template<class T> class XBiasForAction : public T {
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
    XBiasForAction<T>(std::list<std::string> &filenames) : T(filenames) {}

public:

    /**
     * @brief Modified basic synthesis algorithm - adds to 'strategyDumpingData' transitions
     * that represent "a bias for action"
     */
    void computeWinningPositions() {

       BFFixedPoint nu2(mgr.constantTrue());
       for (;!nu2.isFixedPointReached();) {

           strategyDumpingData.clear();
           BF nextContraintsForGoals = mgr.constantTrue();

           for (unsigned int j=0;j<livenessGuarantees.size();j++) {

                BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));

                BFFixedPoint mu1(mgr.constantFalse());
                for (;!mu1.isFixedPointReached();) {

                    livetransitions |= mu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                    // Added a bias for action here.
                    BFFixedPoint mu1b(mu1.getValue());
                    for (;!mu1b.isFixedPointReached();) {
                        BF foundPaths = livetransitions | (mu1b.getValue().SwapVariables(varVectorPre,varVectorPost));
                        foundPaths &= safetySys;
                        strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths));
                        mu1b.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput));
                    }

                    // Iterate over the liveness assumptions. Store the positions that are found to be winning for *any*
                    // of them into the variable 'goodForAnyLivenessAssumption'.
                    BF goodForAnyLivenessAssumption = mu1b.getValue();
                    for (unsigned int i=0;i<livenessAssumptions.size();i++) {

                        BF foundPaths = mgr.constantTrue();

                        BFFixedPoint nu0(mgr.constantTrue());
                        for (;!nu0.isFixedPointReached();) {

                            foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                            foundPaths &= safetySys;

                            nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput));
                        }

                       goodForAnyLivenessAssumption |= nu0.getValue();
                       strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths));
                    }

                    mu1.update(goodForAnyLivenessAssumption);
                }

                nextContraintsForGoals &= mu1.getValue();
            }

            nu2.update(nextContraintsForGoals);

        }
        winningPositions = nu2.getValue();
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XBiasForAction<T>(filenames);
    }
};

#endif

