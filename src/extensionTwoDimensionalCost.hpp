#ifndef __EXTENSION_TWO_DIMENSIONAL_COST_HPP
#define __EXTENSION_TWO_DIMENSIONAL_COST_HPP

#include "gr1context.hpp"

template<class T> class XTwoDimensionalCost : public T {
protected:

    // Inherited stuff used
    using T::mgr;
    using T::initEnv;
    using T::initSys;
    using T::safetyEnv;
    using T::safetySys;
    using T::addVariable;
    using T::parseBooleanFormula;
    using T::livenessGuarantees;
    using T::livenessAssumptions;
    using T::variableNames;
    using T::variables;
    using T::variableTypes;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::strategyDumpingData;
    using T::varCubePostInput;
    using T::winningPositions;
    using T::varCubePreInput;
    using T::varCubePreOutput;
    using T::realizable;
    using T::checkRealizability;

    // Own variables local to this plugin
    // SlugsVectorOfVarBFs preMotionStateVars{PreMotionState,this};
    // SlugsVectorOfVarBFs postMotionStateVars{PostMotionState,this};
    // SlugsVarCube varCubePostMotionState{PostMotionState,this};
    // SlugsVarCube varCubePostControllerOutput{PostMotionControlOutput,this};

    // Cost preference data
    double preferenceFactorWaiting;
    double preferenceFactorAction;
    bool waitingPreferred;

    // Cost Information
    std::map<double,BF> transitionCosts;

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XTwoDimensionalCost<T>(filenames);
    }

    XTwoDimensionalCost<T>(std::list<std::string> &filenames): T(filenames) {}

    void init(std::list<std::string> &filenames) {

        // Load input file
        T::init(filenames);

        // Now load the cost file
        if (filenames.size()==0) {
            std::cerr << "Error: Not enought file names given. Cannot read cost relation map.\n";
            throw "Terminated.";
        }
        std::string inFileName = filenames.front();
        filenames.pop_front();
        std::ifstream inFile(inFileName.c_str());
        if (inFile.fail()) throw "Error: Cannot open input file";

        // Load preference data
        std::string preferenceData;
        std::getline(inFile,preferenceData);
        std::istringstream is(preferenceData);
        is >> preferenceFactorWaiting;
        is >> preferenceFactorAction;
        char c;
        is >> c;
        waitingPreferred = c == '<';
        if (is.fail() || ((c!='<') && (c!='>'))) {
            std::cerr << "Error: Assume that the first line in the cost file contains factors for waiting and action cost and either '<' or '>', all seperated by spaces.\n";
            throw "Terminated.";
        }

        // Load the cost tuples
        std::set<VariableType> allowedTypes;
        allowedTypes.insert(PreInput);
        allowedTypes.insert(PreOutput);
        allowedTypes.insert(PostInput);
        allowedTypes.insert(PostOutput);

        std::string nextLine;
        while (std::getline(inFile,nextLine)) {
            if (nextLine.size()>0) {
                if (nextLine[0]=='#') {
                    // Pass
                } else {
                    unsigned int spaceIndex = nextLine.find(" ");
                    if (spaceIndex==std::string::npos) {
                        std::cerr << "Error: Cannot read line'" << nextLine << "' from the input file - expecting space-separated cost value (floating point number) in front of the line.\n";
                        throw "Terminated.";
                    }
                    std::istringstream is2(nextLine.substr(0,spaceIndex));
                    double value;
                    is2 >> value;
                    if (is2.fail()) {
                        std::cerr << "Error: Cannot read line'" << nextLine << "' from the input file - expecting space-separated cost value (floating point number) in front of the line (case 2).\n";
                        throw "Terminated.";
                    }
                    BF newBF = parseBooleanFormula(nextLine.substr(spaceIndex+1),allowedTypes);
                    if (transitionCosts.count(value)==0) {
                        transitionCosts[value] = newBF;
                    } else {
                        transitionCosts[value] |= newBF;
                    }
                }
            }
        }
    }

    /**
     * @brief Compute the actual cost-optimal strategy by the construction from the paper
     */
    void computeCostOptimalStrategy() {

        // First, check if the costly transitions made sense.
        for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
            for (auto it2 = it;it2!=transitionCosts.end();) {
                it2++;
                if (it2!=transitionCosts.end()) {
                    if (!(it->second & it2->second).isFalse()) {
                        std::cerr << "Error in the cost file: there is some overlap between the costly transitions for the cost values of '" << it->first << "' and '" << it2->first << "'. Check '/tmp/slugs_cost_conflict.dot for a BDD that represents these overlaps.\n";
                        BF_newDumpDot(*this,it->second & it2->second,NULL,"/tmp/slugs_cost_conflict.dot");
                        throw "Terminated.";
                    }
                }
            }
        }

        // Now, allocate additional variables



    }

    /**
     * @brief This function orchestrates the execution of slugs when this plugin is used.
     */
    void execute() {
        //addAutomaticallyGeneratedLivenessAssumption();
        checkRealizability();
        if (realizable) {
            std::cerr << "RESULT: Specification is realizable.\n";
            computeCostOptimalStrategy();
        } else {
            std::cerr << "RESULT: Specification is unrealizable.\n";
        }
    }

};







#endif

