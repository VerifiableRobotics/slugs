#ifndef __EXTENSION_SYNTHESIS_PROCESS_PROFILING_HPP
#define __EXTENSION_SYNTHESIS_PROCESS_PROFILING_HPP

#include "gr1context.hpp"

#include <chrono>

enum XSynthesisProfiling_VariableOrderChoice { dynamic, staticCoupled, staticNonCoupled };

/**
 * A plugin for profiling the realizability checking performance. Allowed to choose between
 * using automatic variable reordering, not using that, and using a obviously suboptimal one
 * variable ordering.
 *
 * Also prints profiling information (numbers of iterations in the fixpoint evaluations, overall computation time)
 * of the realizability checking step.
 */
template<class T> class XSynthesisProfiling : public T {
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
    using T::addVariable;
    using T::strategyDumpingData;

    // Own variables
    XSynthesisProfiling_VariableOrderChoice reorderingChoice;


    XSynthesisProfiling<T>(std::list<std::string> &filenames) : T(filenames) {

    }
public:


    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XSynthesisProfiling<T>(filenames);
    }

    /**
     * @brief Overridden initialization function -- Instantiates the variables in a possibly different order than normal
     * @param filenames
     */
    void init(std::list<std::string> &filenames) {

        // Start (time) measurement for profiling
        auto measurementStart = std::chrono::high_resolution_clock::now();

        if (filenames.size()==0) {
            throw "Error: Cannot load SLUGS input file - there has been no input file name given!";
        }

        std::string inFileName = filenames.front();
        filenames.pop_front();

        // Reordering options
        if (filenames.size()==0) throw "Error: Expected a variable order choice after the input file name. \nThis can either be 'REORD' to keep automatic variable reodering enables, 'STATIC' for a simple static ordering, or 'BULKGROUPED' for having first the predecessor BDD variables, which are then followed by the successor variables.";
        if (filenames.size()>1) throw "Error: Too many options provided!";
        std::string reorderingType = *(filenames.begin());

        if (reorderingType=="REORD") reorderingChoice = dynamic;
        else if (reorderingType=="STATIC") reorderingChoice = staticCoupled;
        else if (reorderingType=="BULKGROUPED") reorderingChoice = staticNonCoupled;
        else throw "Error: Variable order choice not recognized. Run the tool without one to see the available options.";

        filenames.pop_front();

        // Disable automatic reordering if wanted
        if (reorderingChoice!=dynamic) mgr.setAutomaticOptimisation(false);

        // Open input file or produce error message if that does not work
        std::ifstream inFile(inFileName.c_str());
        if (inFile.fail()) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot open input file '" << inFileName << "'";
            throw errorMessage.str();
        }

        // Prepare safety and initialization constraints
        initEnv = mgr.constantTrue();
        initSys = mgr.constantTrue();
        safetyEnv = mgr.constantTrue();
        safetySys = mgr.constantTrue();

        // Read the input file to memory first
        std::list<std::string> inputFileLines;
        {
            std::string currentLine;
            while (std::getline(inFile,currentLine)) {
                inputFileLines.push_back(currentLine);
            }
        }

        // Pass 1: Allocate variables in the order specified.
        std::list<std::pair<VariableType,std::string> > delayedBFVariableInstantiation;

        int readMode = -1;
        std::string currentLine;
        lineNumberCurrentlyRead = 0;
        for (std::string currentLine : inputFileLines) {
            lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                if (currentLine[0]=='[') {
                    if (currentLine=="[INPUT]") {
                        readMode = 0;
                    } else if (currentLine=="[OUTPUT]") {
                        readMode = 1;
                    } else {
                        readMode = 2;
                    }
                } else {
                    if (readMode==0) {
                        addVariable(PreInput,currentLine);
                        if (reorderingChoice != staticNonCoupled)
                            addVariable(PostInput,currentLine+"'");
                        else
                            delayedBFVariableInstantiation.push_back(std::pair<VariableType,std::string>(PostInput,currentLine+"'"));
                    } else if (readMode==1) {
                        addVariable(PreOutput,currentLine);
                        if (reorderingChoice != staticNonCoupled)
                            addVariable(PostOutput,currentLine+"'");
                        else
                            delayedBFVariableInstantiation.push_back(std::pair<VariableType,std::string>(PostOutput,currentLine+"'"));
                    }
                }
            }
        }

        // Add the delayed variables
        for (auto it : delayedBFVariableInstantiation) {
            addVariable(it.first,it.second);
        }

        // Pass 2: Read the specification parts
        // The readmode variable stores in which chapter of the input file we are
        readMode = -1;
        lineNumberCurrentlyRead = 0;
        for (std::string currentLine : inputFileLines) {
            lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                if (currentLine[0]=='[') {
                    if (currentLine=="[INPUT]") {
                        readMode = 0;
                    } else if (currentLine=="[OUTPUT]") {
                        readMode = 1;
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
                        // Already added the variables above
                    } else if (readMode==1) {
                        // Already added the variables above
                    } else if (readMode==2) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        initEnv &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==3) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        initSys &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==4) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        safetyEnv &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==5) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        safetySys &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==6) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostOutput);
                        allowedTypes.insert(PostInput);
                        livenessAssumptions.push_back(parseBooleanFormula(currentLine,allowedTypes));
                    } else if (readMode==7) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        livenessGuarantees.push_back(parseBooleanFormula(currentLine,allowedTypes));
                    } else {
                        std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                        throw "Found a line in the specification file that has no proper categorial context.";
                    }
                }
            }
        }

        // BF_newDumpDot(*this,initEnv,NULL,"/tmp/initEnv.dot");

        // Check if variable names have been used twice
        std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
        if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

        // Make sure that there is at least one liveness assumption and one liveness guarantee
        // The synthesis algorithm might be unsound otherwise
        if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
        if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());

        // End (time) measurement
        auto measurementEnd = std::chrono::high_resolution_clock::now();
        std::cerr << "Parsing time: " << std::chrono::duration<double, std::milli>(measurementEnd-measurementStart).count() << " ms\n";
    }


    void computeWinningPositions() {

        auto measurementStart = std::chrono::high_resolution_clock::now();

        // The greatest fixed point - called "Z" in the GR(1) synthesis paper
        BFFixedPoint nu2(mgr.constantTrue());
        unsigned int nofNu2Iterations = 0;

        // Iterate until we have found a fixed point
        for (;!nu2.isFixedPointReached();) {

            nofNu2Iterations++;

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

                unsigned int nofMu1Iterations = 0;

                // Start computing the transitions that lead closer to the goal and lead to a position that is not yet known to be losing.
                // Start with the ones that actually represent reaching the goal (which is a transition in this implementation as we can have
                // nexts in the goal descriptions).
                BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));

                // Compute the middle least-fixed point (called 'Y' in the GR(1) paper)
                BFFixedPoint mu1(mgr.constantFalse());
                for (;!mu1.isFixedPointReached();) {

                    nofMu1Iterations++;

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

                        unsigned int nofNu0Iterations = 0;

                        for (;!nu0.isFixedPointReached();) {

                            nofNu0Iterations++;

                            // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                            foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));

                            // Compute the set of states/next input combinations
                            // for which the system can behave correctly
                            BF goodStatesPlusNextInput = foundPaths.AndAbstract(safetySys,varCubePostOutput);

                            // Now compute the good states: those for which for all
                            // allowed inputs we reach a combination in "goodStatesPlusNextInput"
                            // The double-negation comes from the fact that there is no "OrUnivAbstract" function.
                            BF goodStates = !((!goodStatesPlusNextInput).AndAbstract(safetyEnv,varCubePostInput));

                            // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                            nu0.update(goodStates);
                        }

                        std::cerr << "# Inner iterations for outermost iteration " << nofNu2Iterations << ", liveness guarantee " << j << ", middle iteration " << nofMu1Iterations << ", and liveness assumption " << i << ": " << nofNu0Iterations << "\n";


                        // Update the set of positions that are winning for some liveness assumption
                        goodForAnyLivenessAssumption |= nu0.getValue();

                        // Dump the paths that we just wound into 'strategyDumpingData' - store the current goal long
                        // with the BDD
                        strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,foundPaths & safetySys));
                    }


                    // Update the moddle fixed point
                    mu1.update(goodForAnyLivenessAssumption);

                    }

                std::cerr << "# Middle iterations for outermost iteration " << nofNu2Iterations << " and liveness guarantee " << j << ": " << nofMu1Iterations << std::endl;

                // Update the set of positions that are winning for any goal for the outermost fixed point
                nextContraintsForGoals &= mu1.getValue();
            }

            // Update the outer-most fixed point
            nu2.update(nextContraintsForGoals);

        }

        // We found the set of winning positions
        winningPositions = nu2.getValue();

        std::cerr << "# Outer-most iterations: " << nofNu2Iterations << std::endl;

        auto measurementEnd = std::chrono::high_resolution_clock::now();
        std::cerr << "Realizability checking time: " << std::chrono::duration<double, std::milli>(measurementEnd-measurementStart).count() << " ms\n";
    }


};












#endif
