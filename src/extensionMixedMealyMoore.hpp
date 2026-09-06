#ifndef __EXTENSION_MIXED_MEALY_MOORE_HPP
#define __EXTENSION_MIXED_MEALY_MOORE_HPP

#include "gr1context.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>

/**
 * A class that modifies the GR(1) synthesis algorithm to support mixed Mealy/Moore semantics.
 * Output propositions declared under [OUTPUT_MOORE] follow Moore semantics (chosen before environment input),
 * while those under [OUTPUT_MEALY] or [OUTPUT] follow standard Mealy semantics (chosen after environment input).
 * The plugin overrides file parsing to recognize the new sections, and modifies the enforceable predecessor
 * operator and realizability check to respect the different quantification orders.
 */
template<class T> class XMixedMealyMoore : public T {
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
    using T::initEnv;
    using T::initSys;
    using T::realizable;
    using T::winningPositions;
    using T::varCubePreOutput;
    using T::varCubePreInput;

    // Constructor

    SlugsVarCube varCubePostMealyOutput{PostOutputAfter, this};
    SlugsVarCube varCubePostMooreOutput{PostOutputBefore, this};
    SlugsVarCube varCubePreMealyOutput{PreOutputAfter, this};
    SlugsVarCube varCubePreMooreOutput{PreOutputBefore, this};

    SlugsVectorOfVarBFs preMealyVars{PreOutputAfter, this};
    SlugsVectorOfVarBFs postMealyVars{PostOutputAfter, this};
    SlugsVectorOfVarBFs preMooreVars{PreOutputBefore, this};
    SlugsVectorOfVarBFs postMooreVars{PostOutputBefore, this};

    XMixedMealyMoore<T>(std::list<std::string> &filenames) : T(filenames) {
    }

    void init(std::list<std::string> &filenames) {
        if (filenames.size()==0) {
            throw "Error: Cannot load SLUGS input file - there has been no input file name given!";
        }

        std::string inFileName = filenames.front();
        filenames.pop_front();

        std::ifstream inFile(inFileName.c_str());
        if (inFile.fail()) throw "Error: Cannot open input file";

        // Prepare safety and initialization constraints
        initEnv = mgr.constantTrue();
        initSys = mgr.constantTrue();
        safetyEnv = mgr.constantTrue();
        safetySys = mgr.constantTrue();

        int readMode = -1;
        std::string currentLine;
        this->lineNumberCurrentlyRead = 0;
        while (std::getline(inFile,currentLine)) {
            this->lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                if (currentLine[0]=='[') {
                    if (currentLine=="[INPUT]") {
                        readMode = 0;
                    } else if (currentLine=="[OUTPUT_MEALY]" || currentLine=="[OUTPUT]") {
                        readMode = 1;
                    } else if (currentLine=="[OUTPUT_MOORE]") {
                        readMode = 8;
                    } else if (currentLine=="[ENV_INIT]") {
                        readMode = 2;
                    } else if (currentLine=="[SYS_INIT]") {
                        readMode = 3;
                    } else if (currentLine=="[ENV_TRANS]") {
                        readMode = 4;
                    } else if (currentLine=="[SYS_TRANS]") {
                        readMode = 5;
                    } else if (currentLine=="[ENV_LIVENESS]") {
                        readMode = 6;
                    } else if (currentLine=="[SYS_LIVENESS]") {
                        readMode = 7;
                    } else {
                        std::cerr << "Sorry. Didn't recognize category " << currentLine << "\n";
                        throw "Aborted.";
                    }
                } else {
                    if (readMode==0) {
                        this->addVariable(PreInput,currentLine);
                        this->addVariable(PostInput,currentLine+"'");
                    } else if (readMode==1) {
                        this->addVariable(PreOutputAfter,currentLine);
                        this->addVariable(PostOutputAfter,currentLine+"'");
                    } else if (readMode==8) {
                        this->addVariable(PreOutputBefore,currentLine);
                        this->addVariable(PostOutputBefore,currentLine+"'");
                    } else if (readMode==2) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        initEnv &= this->parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==3) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PreOutputBefore);
                        allowedTypes.insert(PreOutputAfter);
                        initSys &= this->parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==4) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PreOutputBefore);
                        allowedTypes.insert(PreOutputAfter);
                        allowedTypes.insert(PostInput);
                        safetyEnv &= this->parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==5) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PreOutputBefore);
                        allowedTypes.insert(PreOutputAfter);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        allowedTypes.insert(PostOutputBefore);
                        allowedTypes.insert(PostOutputAfter);
                        safetySys &= this->parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==6) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PreOutputBefore);
                        allowedTypes.insert(PreOutputAfter);
                        allowedTypes.insert(PostOutput);
                        allowedTypes.insert(PostOutputBefore);
                        allowedTypes.insert(PostOutputAfter);
                        allowedTypes.insert(PostInput);
                        livenessAssumptions.push_back(this->parseBooleanFormula(currentLine,allowedTypes));
                    } else if (readMode==7) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PreOutputBefore);
                        allowedTypes.insert(PreOutputAfter);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        allowedTypes.insert(PostOutputBefore);
                        allowedTypes.insert(PostOutputAfter);
                        livenessGuarantees.push_back(this->parseBooleanFormula(currentLine,allowedTypes));
                    } else {
                        std::cerr << "Error with line " << this->lineNumberCurrentlyRead << "!";
                        throw "Found a line in the specification file that has no proper categorial context.";
                    }
                }
            }
        }

        std::set<std::string> variableNameSet(this->variableNames.begin(),this->variableNames.end());
        if (variableNameSet.size()!=this->variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

        if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
        if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());

        this->computeVariableInformation();
    }


public:

    /**
     * @brief Modified basic synthesis algorithm - Allows some propositions to be
     *        of Moore type
     */
    void computeWinningPositions() {


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
                        BF fineMooreOutputs = mgr.constantTrue();

                        // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                        BFFixedPoint nu0(mgr.constantTrue());
                        for (;!nu0.isFixedPointReached();) {

                            // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                            foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                            foundPaths &= safetySys;

                            // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                            fineMooreOutputs = safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostMealyOutput).UnivAbstract(varCubePostInput);
                            
                            nu0.update(fineMooreOutputs.ExistAbstract(varCubePostMooreOutput));
                        }

                        // Update the set of positions that are winning for some liveness assumption
                        goodForAnyLivenessAssumption |= nu0.getValue();

                        // Determinize the Moore outputs so they only depend on the predecessor state
                        BF detMooreOutputs = fineMooreOutputs;
                        for (size_t v = 0; v < postMooreVars.size(); v++) {
                            BF var = postMooreVars[v];
                            BF canBeFalse = (detMooreOutputs & !var).ExistAbstract(varCubePostMooreOutput);
                            detMooreOutputs = (detMooreOutputs & !var) | (detMooreOutputs & var & !canBeFalse);
                        }

                        // Dump the paths that we just wound into 'strategyDumpingData' - store the current goal long
                        // with the BDD
                        strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths & detMooreOutputs));
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

    void checkRealizability() {

        computeWinningPositions();

        // Check if for every possible environment initial position the system has a good system initial position
        BF result;
        result = initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreMealyOutput)).UnivAbstract(varCubePreInput).ExistAbstract(varCubePreMooreOutput);

        // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
        if (!result.isConstant()) {
            BF_newDumpDot(*this,result,NULL,"/tmp/isRealizable.dot");
            throw "Internal error: Could not establish realizability/unrealizability of the specification.";
        }

        // Return the result in Boolean form.
        realizable = result.isTrue();
    }


    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XMixedMealyMoore<T>(filenames);
    }
};

#endif

