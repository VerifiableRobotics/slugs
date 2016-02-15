#ifndef __EXTENSION_COOPERATIVE_GR1_STRATEGY_HPP
#define __EXTENSION_COOPERATIVE_GR1_STRATEGY_HPP

#include "gr1context.hpp"
#include <string>

/**
 * A class that makes the synthesized strategy more "cooperative".
 *
 * This is a variant that works for full GR1 and takes into account liveness
 * assumptions for cooperation.
 */
template<class T> class XCooperativeGR1Strategy : public T {
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
    using T::varCubePost;
    using T::safetyEnv;
    using T::safetySys;
    using T::winningPositions;

    // Constructor
    XCooperativeGR1Strategy<T>(std::list<std::string> &filenames) : T(filenames) {}

public:

    /**
     * @brief Modified basic synthesis algorithm - adds to 'strategyDumpingData' transitions
     * that represent "a bias for action"
     */
    void computeWinningPositions() {

        // Compute system transitions that are not environment dead-ends.
        // The system is only allowed to use these.
        BF nonDeadEndSafetySys = safetySys & safetyEnv.ExistAbstract(varCubePost).SwapVariables(varVectorPre,varVectorPost);
        // BF_newDumpDot(*this,nonDeadEndSafetySys,"Pre Post","/tmp/nonDeadEndSys.dot");
        // BF_newDumpDot(*this,safetyEnv.ExistAbstract(varCubePost).SwapVariables(varVectorPre,varVectorPost),"Pre Post","/tmp/notDeadEndSafetyEnv.dot");

        // Copy all liveness assumptions into liveness guarantees - we are
        // them seperately at the end of a "guarantee cycle".
        for (auto it = livenessAssumptions.begin();it!=livenessAssumptions.end();it++) {
            livenessGuarantees.push_back(*it);
        }
        std::vector<BF> transitionsTowardsLivenessAssumption(livenessAssumptions.size());

        // The greatest fixed point - called "Z" in the GR(1) synthesis paper
        BFFixedPoint nu2(mgr.constantTrue());
        unsigned int nu2Nr = 0;

        // Iterate until we have found a fixed point
        for (;!nu2.isFixedPointReached();) {
            nu2Nr++;

            // To extract a strategy in case of realizability, we need to store a sequence of 'preferred' transitions in the
            // game structure. These preferred transitions only need to be computed during the last execution of the outermost
            // greatest fixed point. Since we don't know which one is the last one, we store them in every iteration,
            // so that after the last iteration, we obtained the necessary data. Before any new iteration, we need to
            // clear the old data, though.
            strategyDumpingData.clear();

            // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
            // goal. Then, after we have iterated over the goals, we can update nu2.
            BF nextContraintsForGoals = mgr.constantTrue();
            for (unsigned int j=0;j<livenessGuarantees.size()-livenessAssumptions.size();j++) {

                // Start computing the transitions that lead closer to the goal and lead to a position that is not yet known to be losing.
                // Start with the ones that actually represent reaching the goal (which is a transition in this implementation as we can have
                // nexts in the goal descriptions).
                BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));

                // Compute the middle least-fixed point (called 'Y' in the GR(1) paper)
                BFFixedPoint mu1(mgr.constantFalse());
                unsigned int mu1Nr = 0;
                for (;!mu1.isFixedPointReached();) {
                    mu1Nr++;

                    // Update the set of transitions that lead closer to the goal.
                    livetransitions |= mu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                    // { std::ostringstream os;
                    // os << "/tmp/livetransitions" << nu2Nr << "-" << j << "-" << mu1Nr << ".dot";
                    // BF_newDumpDot(*this,livetransitions,"Pre Post",os.str()); }

                    // Iterate over the liveness assumptions. Store the positions that are found to be winning for *any*
                    // of them into the variable 'goodForAnyLivenessAssumption'.
                    BF goodForAnyLivenessAssumption = mu1.getValue();
                    for (unsigned int i=0;i<livenessAssumptions.size();i++) {

                        // Prepare the variable 'foundPaths' that contains the transitions that stay within the inner-most
                        // greatest fixed point or get closer to the goal. Only used for strategy extraction
                        BF foundPaths = mgr.constantTrue();

                        // Allocate space for storing the transitions that allow the satisfaction of
                        // the liveness assumptions
                        std::vector<BF> livenessAssumptionProgressPaths;

                        // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                        BFFixedPoint nu0(mgr.constantTrue());
                        unsigned int nu0Nr = 0;
                        for (;!nu0.isFixedPointReached();) {

                            nu0Nr++;

                            // Cooperation: Compute set of states from which a live transition can be
                            // reached (somehow)
                            BFFixedPoint muCoop(mgr.constantFalse());
                            unsigned int muCoopNr = 0;
                            livenessAssumptionProgressPaths.clear();
                            livenessAssumptionProgressPaths.push_back(livetransitions);
                            for (;!muCoop.isFixedPointReached();) {
                                muCoopNr++;
                                foundPaths = livetransitions | ((nu0.getValue() & muCoop.getValue()).SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                                foundPaths &= nonDeadEndSafetySys & safetyEnv;

                                // { std::ostringstream os;
                                // os << "/tmp/foundPaths" << nu2Nr << "-" << j << "-" << mu1Nr << "-" << i << "-" << nu0Nr << "--" << muCoopNr << ".dot";
                                // BF_newDumpDot(*this,foundPaths,"Pre Post",os.str()); }

                                livenessAssumptionProgressPaths.push_back(foundPaths);
                                muCoop.update((safetyEnv & foundPaths).ExistAbstract(varCubePost));

                                // { std::ostringstream os;
                                // os << "/tmp/muCoop" << nu2Nr << "-" << j << "-" << mu1Nr << "-" << i << "-" << nu0Nr << "--" << muCoopNr << ".dot";
                                // BF_newDumpDot(*this,muCoop.getValue(),"Pre Post",os.str()); }
                            }

                            // { std::ostringstream os;
                            // os << "/tmp/foundPathsFinal" << nu2Nr << "-" << j << "-" << mu1Nr << "-" << i << ".dot";
                            // BF_newDumpDot(*this,foundPaths,"Pre Post",os.str()); }


                            // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                            nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput));

                            // { std::ostringstream os;
                            // os << "/tmp/nu0-" << nu2Nr << "-" << j << "-" << mu1Nr << "-" << i << "-" << nu0Nr << ".dot";
                            // BF_newDumpDot(*this,nu0.getValue(),"Pre Post",os.str()); }

                        }

                        // Update the set of positions that are winning for some liveness assumption
                        goodForAnyLivenessAssumption |= nu0.getValue();

                        // Dump the paths that we just wound into 'strategyDumpingData' - store the current goal long
                        // with the BDD
                        for (auto it = livenessAssumptionProgressPaths.begin();it!=livenessAssumptionProgressPaths.end();it++) {
                            strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths & *it));
                        }
                    }

                    // Update the moddle fixed point
                    mu1.update(goodForAnyLivenessAssumption);

                    // { std::ostringstream os;
                    // os << "/tmp/mu1-" << nu2Nr << "-" << j << "-" << mu1Nr << ".dot";
                    // BF_newDumpDot(*this,mu1.getValue(),"Pre Post",os.str()); }
                }

                // Update the set of positions that are winning for any goal for the outermost fixed point
                nextContraintsForGoals &= mu1.getValue();
            }

            // Now iterate over the liveness assumptions
            // and compute a sub-strategy to ensure that there is always some way in which the
            // liveness assumptions can be satisfied
            for (unsigned int j=livenessGuarantees.size()-livenessAssumptions.size();j<livenessGuarantees.size();j++) {

                BF currentLivenessAssumption = livenessGuarantees[j];

                // Collect a set of paths to satisfy this liveness assumption, while the system can
                // always enforce to land in a winning state afterwards
                BFFixedPoint mu1(mgr.constantFalse());
                unsigned int mu1Nr = 0;
                BF goodTransitions = currentLivenessAssumption & nonDeadEndSafetySys & nextContraintsForGoals & safetyEnv & nextContraintsForGoals.SwapVariables(varVectorPre,varVectorPost);

                for (;!mu1.isFixedPointReached();) {
                    mu1Nr++;

                    // { std::ostringstream os;
                    // os << "/tmp/fairtransitions-" << nu2Nr << "-" << j << "-" << mu1Nr << ".dot";
                    // BF_newDumpDot(*this,goodTransitions,"Pre Post",os.str()); }

                    // { std::ostringstream os;
                    // os << "/tmp/fairtransitionCL-" << nu2Nr << "-" << j << "-" << mu1Nr << ".dot";
                    // BF_newDumpDot(*this,currentLivenessAssumption,"Pre Post",os.str()); }

                    // { std::ostringstream os;
                    // os << "/tmp/fairmu-" << nu2Nr << "-" << j << "-" << mu1Nr << ".dot";
                    // BF_newDumpDot(*this,mu1.getValue(),"Pre Post",os.str()); }


                    // Dump good transitions
                    strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,goodTransitions));

                    // From which states can we enforce that the environment can enforce a
                    // goodTransition? We cannot offer losing transitions.

                    BF winningStates = goodTransitions.ExistAbstract(varCubePost);
                    goodTransitions |= winningStates.SwapVariables(varVectorPre,varVectorPost) & safetyEnv & nonDeadEndSafetySys & nextContraintsForGoals;
                    mu1.update(winningStates | mu1.getValue());
                }
                nextContraintsForGoals &= mu1.getValue();
                transitionsTowardsLivenessAssumption[j-(livenessGuarantees.size()-livenessAssumptions.size())] = goodTransitions;

                // Backup transitions not towards the goal
                strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,nonDeadEndSafetySys & nu2.getValue().SwapVariables(varVectorPre,varVectorPost)));
            }


            // Update the outer-most fixed point
            nu2.update(nextContraintsForGoals);

            // { std::ostringstream os;
            // os << "/tmp/nu2-" << nu2Nr << ".dot";
            // BF_newDumpDot(*this,nu2.getValue(),"Pre Post",os.str()); }
        }

        // Modify the additional liveness guarantees to be satisfied once the environment deviated
        // from the paths towards the environment liveness goal.
        for (unsigned int j=livenessGuarantees.size()-livenessAssumptions.size();j<livenessGuarantees.size();j++) {
            livenessGuarantees[j] |= !transitionsTowardsLivenessAssumption[j-(livenessGuarantees.size()-livenessAssumptions.size())];
        }

        // We found the set of winning positions
        winningPositions = nu2.getValue();

    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XCooperativeGR1Strategy<T>(filenames);
    }
};

#endif
