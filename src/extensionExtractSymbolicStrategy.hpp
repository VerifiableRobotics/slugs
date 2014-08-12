#ifndef __EXTENSION_EXTRACT_SYMBOLIC_STRATEGY_HPP
#define __EXTENSION_EXTRACT_SYMBOLIC_STRATEGY_HPP

#include "gr1context.hpp"
#include <string>

/**
 * An extension that triggers that a symbolic strategy to be extracted.
 */
template<class T, bool oneStepRecovery> class XExtractSymbolicStrategy : public T {
protected:
    // New variables
    std::string outputFilename;

    // Inherited stuff used
    using T::mgr;
    using T::winningPositions;
    using T::initSys;
    using T::initEnv;
    using T::preVars;
    using T::livenessGuarantees;
    using T::strategyDumpingData;
    using T::variables;
    using T::safetyEnv;
    using T::variableTypes;
    using T::realizable;
    using T::postVars;
    using T::varCubePre;
    using T::variableNames;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::addVariable;
    using T::computeVariableInformation;

    std::vector<int> counterVarNumbers;
    int goalTransitionSelectorVar;

    XExtractSymbolicStrategy<T,oneStepRecovery>(std::list<std::string> &filenames): T(filenames) {}

    void init(std::list<std::string> &filenames) {
        T::init(filenames);
        if (filenames.size()==0) {
            std::cerr << "Error: Need a file name for extracting a symbolic strategy.\n";
            throw "Please adapt the parameters.";
        } else {
            outputFilename = filenames.front();
            filenames.pop_front();
        }

    }

public:

    void execute() {
        T::execute();
        if (realizable) {
            if (outputFilename=="") {
                throw "Internal Error.";
            } else {
                computeAndPrintSymbolicStrategy(outputFilename);
            }
        }
    }

    /**
     * @brief Compute and print out (to stdout) a symbolic strategy that is winning for
     *        the system.
     *        This function requires that the realizability of the specification has already been
     *        detected and that the variables "strategyDumpingData" and
     *        "winningPositions" have been filled by the synthesis algorithm with meaningful data.
     * @param outputStream - Where the strategy shall be printed to.
     */
    void computeAndPrintSymbolicStrategy(std::string filename) {

        // We don't want any reordering from this point onwards, as
        // the BDD manipulations from this point onwards are 'kind of simple'.
        mgr.setAutomaticOptimisation(false);

        // Prepare initial to-do list from the allowed initial states
        BF init = (oneStepRecovery)?(winningPositions & initSys):(winningPositions & initSys & initEnv);

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

            // std::ostringstream gsName;
            // gsName << "/tmp/generalStrategy" << i << ".dot";
            // BF dump = variables[4] & !variables[6]& !variables[8] & strategy;
            // BF_newDumpDot(*this,dump,"PreInput PreOutput PostInput PostOutput",gsName.str().c_str());
        }

        // Allocate counter variables
        for (unsigned int i=1;i<=livenessGuarantees.size();i = i << 1) {
            std::ostringstream os;
            os << "_jx_b" << counterVarNumbers.size();
            counterVarNumbers.push_back(addVariable(SymbolicStrategyCounterVar,os.str()));
        }
        goalTransitionSelectorVar = addVariable(SymbolicStrategyCounterVar,"strat_type");
        computeVariableInformation();

        BF combinedStrategy = mgr.constantFalse();
        for (unsigned int i=0;i<livenessGuarantees.size();i++) {
            BF thisEncoding = mgr.constantTrue();
            for (unsigned j=0;j<counterVarNumbers.size();j++) {
                if (i&(1 << j)) {
                    thisEncoding &= variables[counterVarNumbers[j]];
                } else {
                    thisEncoding &= !variables[counterVarNumbers[j]];
                }
            }
            combinedStrategy |= thisEncoding & positionalStrategiesForTheIndividualGoals[i] &
                ((!variables[goalTransitionSelectorVar]) | livenessGuarantees[i]);
        }

        std::ostringstream fileExtraHeader;
        fileExtraHeader << "# This file is a BDD exported by the SLUGS\n#\n# This BDD is a strategy.\n";
        fileExtraHeader << "#\n# This header contains extra information used by LTLMoP's BDDStrategy.\n";
        fileExtraHeader << "# Currently, the only metadata is 1) the total number of system goals\n";
        fileExtraHeader << "# and 2) the mapping between variable numbers and proposition names.\n#\n";
        fileExtraHeader << "# Some special variables are also added:\n";
        fileExtraHeader << "#       - `_jx_b*` are used as a binary vector (b0 is LSB) to indicate\n";
        fileExtraHeader << "#         the index of the currently-pursued goal.\n";
        fileExtraHeader << "#       - `strat_type` is a binary variable used to indicate whether we are\n";
        fileExtraHeader << "#          moving closer to the current goal (0) or transitioning to the next goal (1)\n#\n";
        fileExtraHeader << "# Num goals: " << livenessGuarantees.size() << "\n";
        fileExtraHeader << "# Variable names:\n";
        for (unsigned int i=0;i<variables.size();i++) {
            fileExtraHeader << "#\t" << i << ": " << variableNames[i] << "\n";
        }
        fileExtraHeader << "#\n# For information about the DDDMP format, please see:\n";
        fileExtraHeader << "#    http://www.cs.uleth.ca/~rice/cudd_docs/dddmp/dddmpAllFile.html#dddmpDump.c\n#\n";
        fileExtraHeader << "# For information about how this file is generated, please see the SLUGS source.\n#\n";

        // BF dump = variables[4] & !variables[6]& !variables[8] & combinedStrategy;
        // BF_newDumpDot(*this,dump,"SymbolicStrategyCounterVar PreInput PreOutput PostInput PostOutput","/tmp/writtenBDD.dot");

        mgr.writeBDDToFile(filename.c_str(),fileExtraHeader.str(),combinedStrategy,variables,variableNames);

    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XExtractSymbolicStrategy<T,oneStepRecovery>(filenames);
    }
};

#endif

