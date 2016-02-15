#ifndef __EXTENSION_INTERLEAVE_HPP
#define __EXTENSION_INTERLEAVE_HPP

#include "gr1context.hpp"
#include <string>
#include "boost/tuple/tuple.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>

/**
 * A class that analyzes if a specification stays realizable if the value of some output variables
 * have to be set before the input can be read (in a computation cycle).
 *
 * This plugin is a part of the specification report generator
 */


//typedef enum { PreInput, PreOutputB, PreOutputA, PostInput, PostOutputB, PostOutputA } VariableTypeBA;

template<class T> class XInterleave : public T {
protected:

    // Inherited stuff used
    using T::mgr;
    using T::livenessGuarantees;
    using T::livenessAssumptions;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::safetyEnv;
    using T::safetySys;
    using T::initEnv;
    using T::initSys;
    using T::winningPositions;
    using T::realizable;
    using T::lineNumberCurrentlyRead;
    using T::variables;
    using T::variableNames;
    using T::variableTypes;
    using T::parseBooleanFormulaRecurse;
    using T::parseBooleanFormula;
    using T::preVars;
    using T::postVars;
    using T::postInputVars;
    using T::varCubePre;
    using T::varCubePost;
    using T::varCubePostOutput;
    using T::varCubePreOutput;
    using T::varCubePreInput;
    using T::varCubePostInput;
    using T::checkRealizability;
    using T::computeVariableInformation;
    using T::doesVariableInheritType;

    std::vector<std::pair<unsigned int,BF> > strategyDumpingData;
    SlugsVarCube varCubePostOutputB {PostOutputBefore,this};
    SlugsVarCube varCubePostOutputA {PostOutputAfter,this};
    SlugsVarCube varCubePreOutputB {PreOutputBefore,this};
    SlugsVarCube varCubePreOutputA {PreOutputAfter,this};
    SlugsVectorOfVarBFs postOutputBVars {PostOutputBefore,this};
    SlugsVectorOfVarBFs postOutputAVars {PostOutputAfter,this};
    SlugsVectorOfVarBFs preOutputBVars {PreOutputBefore,this};
    SlugsVectorOfVarBFs preOutputAVars {PreOutputAfter,this};

    // Constructor
    XInterleave<T>(std::list<std::string> &filenames) : T(filenames) {}

public:



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
    void computeWinningPositions() {

        // The greatest fixed point - called "Z" in the GR(1) synthesis paper
        BFFixedPoint nu2(mgr.constantTrue());

        //BF safeStates = safetySys.ExistAbstract(varCubePostInput).ExistAbstract(varCubePostOutputA).ExistAbstract(varCubePostOutputB);
        //BF safeNext = (safetySys.ExistAbstract(varCubePreInput).ExistAbstract(varCubePreOutputA).ExistAbstract(varCubePreOutputB)).SwapVariables(varVectorPost,varVectorPre);
        //
        //BF newSafetySys = safetySys & (safeStates & safeNext).SwapVariables(varVectorPre,varVectorPost).SwapVariables(varVectorPostOutputA,varVectorPreOutputA);
        //
        assert(((const BFVarCube&)varCubePostOutputA).size()+((const BFVarCube&)varCubePostOutputB).size()==((const BFVarCube&)varCubePostOutput).size());


        // Iterate until we have found a fixed point
        for (; !nu2.isFixedPointReached();) {

            // To extract a strategy in case of realizability, we need to store a sequence of 'preferred' transitions in the
            // game structure. These preferred transitions only need to be computed during the last execution of the outermost
            // greatest fixed point. Since we don't know which one is the last one, we store them in every iteration,
            // so that after the last iteration, we obtained the necessary data. Before any new iteration, we need to
            // clear the old data, though.
            strategyDumpingData.clear();

            // Iterate over all of the liveness guarantees. Put the results into the variable 'nextContraintsForGoals' for every
            // goal. Then, after we have iterated over the goals, we can update nu2.
            BF nextContraintsForGoals = mgr.constantTrue();
            for (unsigned int j=0; j<livenessGuarantees.size(); j++) {

                // Start computing the transitions that lead closer to the goal and lead to a position that is not yet known to be losing.
                // Start with the ones that actually represent reaching the goal (which is a transition in this implementation as we can have
                // nexts in the goal descriptions).
                BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));

                // Compute the middle least-fixed point (called 'Y' in the GR(1) paper)
                BFFixedPoint mu1(mgr.constantFalse());
                for (; !mu1.isFixedPointReached();) {

                    // Update the set of transitions that lead closer to the goal.
                    livetransitions |= mu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                    // Iterate over the liveness assumptions. Store the positions that are found to be winning for *any*
                    // of them into the variable 'goodForAnyLivenessAssumption'.
                    BF goodForAnyLivenessAssumption = mu1.getValue();
                    for (unsigned int i=0; i<livenessAssumptions.size(); i++) {

                        // Prepare the variable 'foundPaths' that contains the transitions that stay within the inner-most
                        // greatest fixed point or get closer to the goal. Only used for strategy extraction
                        BF foundPaths = mgr.constantTrue();

                        // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                        BFFixedPoint nu0(mgr.constantTrue());
                        for (; !nu0.isFixedPointReached();) {

                            // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                            foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));

                            // Update the inner-most fixed point with the result of applying the enforcable *BA* predecessor operator
                            // Exists setting to "before" outputs such that for all settings to inputs, exists setting to "after" outputs that enforces successor

                            nu0.update((safetyEnv.Implies(safetySys & foundPaths)).ExistAbstract(varCubePostOutputA).UnivAbstract(varCubePostInput).ExistAbstract(varCubePostOutputB));
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
        BF_newDumpDot(*this,winningPositions,NULL,"/tmp/wp.dot");
    }

    void execute() {

        // Default Ordering
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,PreOutput)) {
                variableTypes[i] = PreOutputAfter;
            }
            if (doesVariableInheritType(i,PostOutput)) {
                variableTypes[i] = PostOutputAfter;
            }
        }
        computeVariableInformation();

        std::cerr << "A: " << varCubePostOutputA.size() << std::endl;
        std::cerr << "B: " << varCubePostOutputB.size() << std::endl;

        checkRealizability();
        if (realizable) {
            std::cout << "Starting point is a realizable specification.\n\n";

            // Try to "pre" Some variables
            bool foundOne;
            for (unsigned int i=0;i<variables.size();i++) {
                if (doesVariableInheritType(i,PreOutput)) {
                    variableTypes[i] = PreOutputBefore;
                    assert(variableTypes[i+1]==PostOutputAfter);
                    variableTypes[i+1] = PostOutputBefore;
                    computeVariableInformation();
                    checkRealizability();
                    if (!realizable) {
                        variableTypes[i] = PreOutputAfter;
                        variableTypes[i+1] = PostOutputAfter;
                    } else {
                        foundOne = true;
                    }
                }

            }

            if (foundOne) {
                std::cout << "The specification stays realizable when the values of the following output signals have to be set before reading the input:\n";
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i,PreOutputBefore)) {
                        std::cout << "- " << variableNames[i] << std::endl;
                    }
                }
            } else {
                std::cout << "The specification becomes unrealizable when some output variable has to be set before the inputs are present.\n";
            }

        } else {
            std::cout << "Starting point is an unrealizable specification --> no analysis to be performed.\n";
        }

    }


    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XInterleave(filenames);
    }
};

#endif

