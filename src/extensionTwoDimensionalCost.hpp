#ifndef __EXTENSION_TWO_DIMENSIONAL_COST_HPP
#define __EXTENSION_TWO_DIMENSIONAL_COST_HPP
#include "gr1context.hpp"


class TwoDimensionalCostNotionTuple {
private:
    double combinedCost;
    double actionCost;
    int waitingCost;
    bool preference;
public:
    /**
     * @brief A tuple class for keeping track of transitions that are winning with a certain cost
     * @param _combinedCost The linear combination of action and waiting cost according to the linear preference factors
     * @param _actionCost
     * @param _waitingCost
     * @param _preference
     */
    TwoDimensionalCostNotionTuple(double _combinedCost, int _waitingCost, double _actionCost, bool _preference)
        : combinedCost(_combinedCost), actionCost(_actionCost), waitingCost(_waitingCost), preference(_preference) {
        assert(waitingCost >= 0);
    }

    bool operator <(const TwoDimensionalCostNotionTuple &other) const {
        assert(other.preference==preference);
        if (combinedCost < other.combinedCost) return true;
        if (combinedCost > other.combinedCost) return false;
        if (preference) {
            // Waiting preferred
            if (waitingCost < other.waitingCost) return true;
            if (waitingCost > other.waitingCost) return false;
            if (actionCost < other.actionCost) return true;
            if (actionCost > other.actionCost) return false;
            return false;
        } else {
            // Action preferred
            if (actionCost < other.actionCost) return true;
            if (actionCost > other.actionCost) return false;
            if (waitingCost < other.waitingCost) return true;
            if (waitingCost > other.waitingCost) return false;
            return false;
        }
    }
    bool operator <=(const TwoDimensionalCostNotionTuple &other) const {
        assert(other.preference==preference);
        if (*this < other) return true;
        if (*this == other) return true;
        return false;
    }
    bool operator ==(const TwoDimensionalCostNotionTuple &other) const {
        assert(other.preference==preference);
        if (other.actionCost != actionCost) return false;
        if (other.waitingCost != waitingCost) return false;
        return true;
    }
    friend std::ostream& operator<< (std::ostream& stream, TwoDimensionalCostNotionTuple const& tuple) {
        stream << "(" << tuple.waitingCost << ",";
        if (tuple.actionCost==std::numeric_limits<double>::infinity())
            stream << "infty)";
        else
            stream << tuple.actionCost << ")";
        return stream;
    }

    double getCombinedCost() { return combinedCost; }
    double getActionCost() { return actionCost; }
    int getWaitingCost() { return waitingCost; }
    bool getPreference() { return preference; }

};


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
    using T::varCubePostOutput;
    using T::winningPositions;
    using T::varCubePreInput;
    using T::varCubePreOutput;
    using T::realizable;
    using T::checkRealizability;
    using T::computeVariableInformation;

    SlugsVectorOfVarBFs assumptionCounterPreVars{CurrentLivenessAssumptionCounterPre,this};
    SlugsVectorOfVarBFs assumptionCounterPostVars{CurrentLivenessAssumptionCounterPost,this};
    SlugsVectorOfVarBFs IsInftyCostPreVars{IsInftyCostPre,this};
    SlugsVectorOfVarBFs IsInftyCostPostVars{IsInftyCostPost,this};

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

        // Then, assign to every transition that has no cost so far the cost 0
        BF noCostAssigned = mgr.constantTrue();
        for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
            noCostAssigned &= !it->second;
        }
        if (transitionCosts.count(0.0)==0) {
            transitionCosts[0.0] = noCostAssigned;
        } else {
            transitionCosts[0.0] |= noCostAssigned;
        }

        // Now, allocate additional variables
        BF preTransitionalStateEncoding = mgr.constantTrue();
        for (unsigned int i=1;i<livenessAssumptions.size()+1;i = i << 1) {
            std::ostringstream livenessAssumptionCounterVar;
            livenessAssumptionCounterVar << "_l_a_c_v_" << i;
            addVariable(CurrentLivenessAssumptionCounterPre,livenessAssumptionCounterVar.str());
            preTransitionalStateEncoding &= ! variables.back();
            livenessAssumptionCounterVar << "'";
            addVariable(CurrentLivenessAssumptionCounterPost,livenessAssumptionCounterVar.str());
        }
        addVariable(IsInftyCostPre,"_is_infty_cost_Pre");
        addVariable(IsInftyCostPost,"_is_infty_cost_Post");
        computeVariableInformation();


#define MK_COST_TUPLE(x,y) TwoDimensionalCostNotionTuple(preferenceFactorWaiting*x+preferenceFactorAction*y,x,y,waitingPreferred)

        // ===================================
        // Computation of the winning strategy
        // ===================================
        strategyDumpingData.clear();
        for (unsigned int livenessGoal=0;livenessGoal<livenessGuarantees.size();livenessGoal++) {
            std::cerr << "Processing liveness objective number " << livenessGoal << std::endl;

            // Prepare TODO list
            std::set<TwoDimensionalCostNotionTuple> todo;
            for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                todo.insert(MK_COST_TUPLE(0,it->first));
            }
            // Ensure that the first infty case is included
            todo.insert(MK_COST_TUPLE(1,std::numeric_limits<double>::infinity()));

            BF positionsAlreadyFoundToBeWinning = mgr.constantFalse();
            BF transitionsAlreadyFoundToBeWinning = mgr.constantFalse();

            std::map<TwoDimensionalCostNotionTuple,BF> winningTransitionsFound;
            std::map<TwoDimensionalCostNotionTuple,BF> winningPositionsFound;

            while (todo.size()!=0) {

                // Get new element
                TwoDimensionalCostNotionTuple currentTuple = *(todo.begin());
                todo.erase(currentTuple);
                std::cerr << "Working on Cost Tuple: " << currentTuple << std::endl;

                // Inft-cost or normal cost?
                if (currentTuple.getActionCost()==std::numeric_limits<double>::infinity()) {
                    // Infty Cost case
                    if (currentTuple.getWaitingCost()!=0) {

                    }
                    // Add next infty-cost tuple.
                } else {
                    // Finite Cost case

                    if (currentTuple.getWaitingCost()==0) {

                        // Help me debug!
                        std::ostringstream filename;
                        filename << "/tmp/guarantee" << livenessGoal << "waiting0action" << currentTuple;

                        // Waiting cost 0
                        //
                        // Start with listing the goal transitions...
                        BF allowedEndingTransitions = mgr.constantFalse();
                        for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                            if (MK_COST_TUPLE(0,it->first)<=currentTuple) {
                                allowedEndingTransitions |= it->second & safetySys & livenessGuarantees[livenessGoal];
                            }
                        }

                        //BF_newDumpDot(*this,allowedEndingTransitions,NULL,filename.str()+"allowed1.dot");

                        // .... and add the allowed transitions to positions that are already know to be winning ...
                        for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                            double allowedActionCost = currentTuple.getActionCost() - it->first;
                            // Numeric correction
                            allowedActionCost = std::nextafter(allowedActionCost,std::numeric_limits<double>::max());
                            std::cerr << "Allowed Action Cost: " << allowedActionCost << std::endl;
                            auto winningStateFinder = winningPositionsFound.upper_bound(MK_COST_TUPLE(currentTuple.getWaitingCost(),allowedActionCost));
                            if (winningStateFinder != winningPositionsFound.begin()) {
                                // The upper bound is not quite what we need, rather the element before it
                                winningStateFinder--;
                                std::cerr << "Found some transitions...\n";
                                allowedEndingTransitions |= safetySys & winningStateFinder->second.SwapVariables(varVectorPre,varVectorPost) & it->second;
                            }
                        }

                        //BF_newDumpDot(*this,allowedEndingTransitions,NULL,filename.str()+"allowed2.dot");


                        // Waiting cost 0
                        BF winningPositionsOld = mgr.constantTrue();
                        BF winningPositionsNew = mgr.constantFalse();
                        BF oldPositionsWinning = positionsAlreadyFoundToBeWinning;
                        while (winningPositionsNew!=winningPositionsOld) {
                            winningPositionsOld = winningPositionsNew;
                            BF newWinningTransitions = (allowedEndingTransitions | (transitionCosts[0.0] & safetySys & winningPositionsNew.SwapVariables(varVectorPre,varVectorPost)));
                            winningPositionsNew = newWinningTransitions.ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput);
                            strategyDumpingData.push_back(std::pair<int,BF>(livenessGoal,newWinningTransitions & preTransitionalStateEncoding));
                            transitionsAlreadyFoundToBeWinning |= preTransitionalStateEncoding & newWinningTransitions;
                            //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,NULL,filename.str()+"winningPosOld.dot");
                            positionsAlreadyFoundToBeWinning |= preTransitionalStateEncoding & winningPositionsNew;
                            //BF_newDumpDot(*this,newWinningTransitions,NULL,filename.str()+"winningTrans.dot");
                            //BF_newDumpDot(*this,winningPositionsNew,NULL,filename.str()+"winningPosNew.dot");
                        }

                        //BF_newDumpDot(*this,winningPositionsNew,NULL,filename.str()+"wpnew.dot");
                        //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,NULL,filename.str()+"positionsalreadyfoundtobewinning.dot");

                        if (oldPositionsWinning != positionsAlreadyFoundToBeWinning) {
                            // Add new elements to the TODO list
                            for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                                // No need to consider the cost-0 case again
                                if (it->first != 0) {
                                    std::cerr << "Insert " << it->first+currentTuple.getActionCost() << std::endl;
                                    todo.insert(MK_COST_TUPLE(currentTuple.getWaitingCost(),it->first+currentTuple.getActionCost()));
                                    todo.insert(MK_COST_TUPLE(currentTuple.getWaitingCost()+1,currentTuple.getActionCost()));
                                    todo.insert(MK_COST_TUPLE(currentTuple.getWaitingCost()+1,it->first+currentTuple.getActionCost()));
                                }
                            }
                        }

                        winningTransitionsFound[currentTuple] = transitionsAlreadyFoundToBeWinning;
                        winningPositionsFound[currentTuple] = positionsAlreadyFoundToBeWinning;

                    } else {
                        // Waiting cost > 0

                        // Take not too costly winning transitions and compute a suitable SCC


                    }
                }

            }

            // Let's dump the strategy data!
            unsigned int i = 0;
            for (auto it = strategyDumpingData.begin();it!=strategyDumpingData.end();it++) {
                if (it->first == livenessGoal) {
                    std::ostringstream filename2;
                    filename2 << "/tmp/guarantee" << livenessGoal << "winningTransitions" << i++ << ".dot";
                    BF_newDumpDot(*this,it->second,NULL,filename2.str());
                }
            }

        } // Iteration through all liveness goals completed


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

