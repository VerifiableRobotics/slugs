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
    TwoDimensionalCostNotionTuple(double waitingCostWeight, double actionCostWeight, int _waitingCost, double _actionCost, bool _preference)
        :  actionCost(_actionCost), waitingCost(_waitingCost), preference(_preference) {
        if (actionCostWeight>0) {
            if (actionCost==std::numeric_limits<double>::infinity()) {
                combinedCost = std::numeric_limits<double>::infinity();
            } else {
                combinedCost = waitingCostWeight*waitingCost + actionCostWeight*actionCost;
            }
        } else {
            combinedCost = waitingCostWeight*waitingCost;
        }
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
            //if (actionCost > other.actionCost) return false;
            return false;
        } else {
            // Action preferred
            if (actionCost < other.actionCost) return true;
            if (actionCost > other.actionCost) return false;
            if (waitingCost < other.waitingCost) return true;
            //if (waitingCost > other.waitingCost) return false;
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
        assert(other.combinedCost==combinedCost);
        return true;
    }
    friend std::ostream& operator<< (std::ostream& stream, TwoDimensionalCostNotionTuple const& tuple) {
        stream << "(" << tuple.combinedCost << ":" << tuple.waitingCost << ",";
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


template<class T, bool isSysInitRoboticsSemantics, bool isSimpleRecovery> class XTwoDimensionalCost : public T {
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
    using T::varCubePre;
    using T::varCubePreOutput;
    using T::realizable;
    using T::checkRealizability;
    using T::computeVariableInformation;
    using T::lineNumberCurrentlyRead;
    using T::doesVariableInheritType;

    SlugsVectorOfVarBFs assumptionCounterPreVars{CurrentLivenessAssumptionCounterPre,this};
    SlugsVectorOfVarBFs assumptionCounterPostVars{CurrentLivenessAssumptionCounterPost,this};
    SlugsVarCube assumptionCounterPreCube{CurrentLivenessAssumptionCounterPre,this};
    SlugsVarCube addedHelperBitsInImplementationPreCube{CurrentLivenessAssumptionCounterPre,IsInftyCostPre,this};
    SlugsVectorOfVarBFs addedHelperBitsInImplementationPreVars{CurrentLivenessAssumptionCounterPre,IsInftyCostPre,this};

    SlugsVectorOfVarBFs transitiveClosureIntermediateVars{TransitiveClosureIntermediateVariables,this};
    SlugsVarVector transitiveClosureIntermediateVarVector{TransitiveClosureIntermediateVariables,this};
    SlugsVarCube transitiveClosureIntermediateVarCube{TransitiveClosureIntermediateVariables,this};

    // These VarVectors must be singleton and should be sets!
    SlugsVectorOfVarBFs isInftyCostPreVars{IsInftyCostPre,this};
    SlugsVectorOfVarBFs isInftyCostPostVars{IsInftyCostPost,this};


    // Cost preference data
    double preferenceFactorWaiting;
    double preferenceFactorAction;
    bool waitingPreferred;

    // Cost Information
    std::map<double,BF> transitionCosts;

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XTwoDimensionalCost<T,isSysInitRoboticsSemantics,isSimpleRecovery>(filenames);
    }

    XTwoDimensionalCost<T,isSysInitRoboticsSemantics,isSimpleRecovery>(std::list<std::string> &filenames): T(filenames) {}

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

        // For correct error messages when reading the transition costs to come
        lineNumberCurrentlyRead = 1;

        // Load the cost tuples
        std::set<VariableType> allowedTypes;
        allowedTypes.insert(PreInput);
        allowedTypes.insert(PreOutput);
        allowedTypes.insert(PostInput);
        allowedTypes.insert(PostOutput);

        std::string nextLine;
        while (std::getline(inFile,nextLine)) {
            lineNumberCurrentlyRead++;
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

        // In the following, "livenessGuarantees" will be used as they are -- so conjunct them with
        // the winning positions as post states
        for (unsigned int i=0;i<livenessGuarantees.size();i++) {
            livenessGuarantees[i] &= winningPositions.SwapVariables(varVectorPre,varVectorPost);;
        }

        // First, check if the costly transitions made sense.
        for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
            for (auto it2 = it;it2!=transitionCosts.end();) {
                it2++;
                if (it2!=transitionCosts.end()) {
                    if (!(it->second & it2->second).isFalse()) {
                        std::cerr << "Error in the cost file: there is some overlap between the costly transitions for the cost values of '" << it->first << "' and '" << it2->first << "'. Check '/tmp/slugs_cost_conflict.dot for a BDD that represents these overlaps.\n";
                        BF_newDumpDot(*this,it->second & it2->second,"Pre Post","/tmp/slugs_cost_conflict.dot");
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

        // Now, allocate additional variables for the strategy structure
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

        // ...and we need additional variables for transitive closure computation....
        computeVariableInformation();
        unsigned int nofNonClosureVars = variables.size();
        for (unsigned int i=0;i<nofNonClosureVars;i++) {
            std::cerr << "Var " << i << std::endl;
            std::cerr << "VarName " << variableNames[i] << std::endl;
            if (doesVariableInheritType(i,Pre)) {
                addVariable(TransitiveClosureIntermediateVariables,variableNames[i]+"__trans");
            }
        }
        computeVariableInformation(); // Need to call twice here so that the "doesVariableInheritType" above works

        // Some sanity checks, mostly for ensuring that future changes do not break things here.
        assert(transitiveClosureIntermediateVarCube.size()==transitiveClosureIntermediateVarVector.size());
        assert(transitiveClosureIntermediateVarCube.size()==transitiveClosureIntermediateVars.size());
        assert(transitiveClosureIntermediateVarCube.size()==varVectorPre.size());
        assert(varVectorPost.size()==varVectorPre.size());

        // =====================================
        // Compute encoding for non-transitional
        // states
        // =====================================
        std::vector<BF> nonTransitionalStatePreEncoding;
        std::vector<BF> nonTransitionalStatePostEncoding;
        for (unsigned int i=0;i<livenessAssumptions.size();i++) {
            BF thisCombinationPre = mgr.constantTrue();
            BF thisCombinationPost = mgr.constantTrue();
            unsigned int bitnum = 0;
            for (unsigned int j=1;j<livenessAssumptions.size()+1;j = j << 1) {
                if ((i+1) & j) {
                    thisCombinationPre &= assumptionCounterPreVars[bitnum];
                    thisCombinationPost &= assumptionCounterPostVars[bitnum];
                } else {
                    thisCombinationPre &= !(assumptionCounterPreVars[bitnum]);
                    thisCombinationPost &= !(assumptionCounterPostVars[bitnum]);
                }
                bitnum++;
            }
            std::ostringstream encodingName;
            encodingName << "/tmp/nonTransitionalState" << i;
            //BF_newDumpDot(*this,thisCombinationPre,"Pre Post",encodingName.str()+"pre.dot");
            //BF_newDumpDot(*this,thisCombinationPost,"Pre Post",encodingName.str()+"post.dot");
            nonTransitionalStatePreEncoding.push_back(thisCombinationPre);
            nonTransitionalStatePostEncoding.push_back(thisCombinationPost);
        }

        std::cerr << "TODO: Disallow the system to set the current assumption being waited for to some illegal value.\n";

        //BF_newDumpDot(*this,safetySys,"Pre Post","/tmp/safetySys.dot");


#define MK_COST_TUPLE(x,y) TwoDimensionalCostNotionTuple(preferenceFactorWaiting,preferenceFactorAction,x,y,waitingPreferred)

        // ===================================
        // Computation of the winning strategy
        // ===================================
        strategyDumpingData.clear();
        for (unsigned int livenessGoal=0;livenessGoal<livenessGuarantees.size();livenessGoal++) {
            // std::cerr << "Processing liveness objective number " << livenessGoal << std::endl;

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
                /*std::cerr << "Content of todo: ";
                for (auto it = todo.begin();it!=todo.end();it++) std::cerr << *it << " ";
                std::cerr << std::endl;*/
                todo.erase(currentTuple);
                /*std::cerr << "Content of todo: ";
                for (auto it = todo.begin();it!=todo.end();it++) std::cerr << *it << " ";
                std::cerr << std::endl;
                std::cerr << "Working on Cost Tuple: " << currentTuple << std::endl;*/
                assert(todo.count(currentTuple)==0);

                BF positionsWinningBeforeProcessingThisCostTuple =
                   (winningPositionsFound.empty())?(mgr.constantFalse()):(winningPositionsFound.rbegin()->second);

                // Inft-cost or normal cost?
                if (currentTuple.getActionCost()==std::numeric_limits<double>::infinity()) {
                    // Infty Cost case
                    if (currentTuple.getWaitingCost()==0) {
                        // With an infinite action cost, the waiting cost needs to be at least one.
                    } else {

                        // ================== INFTY ACTION COST ===============================
                        // Help me debug!
                        //std::ostringstream filename;
                        //filename << "/tmp/guarantee" << livenessGoal << "waiting0action" << currentTuple;

                        // Compute the Escape Transitions, consisting of:
                        // 1. Goal transitions
                        BF allowedEscapeTransitions = safetySys & livenessGuarantees[livenessGoal];

                        // 2. Transitions that are already known to be winning "cheaply"
                        // -- abstract from the pre assumption that is being waited for
                        auto winningTransitionFinder = winningTransitionsFound.upper_bound(MK_COST_TUPLE(currentTuple.getWaitingCost()-1,currentTuple.getActionCost()));
                        if (winningTransitionFinder != winningTransitionsFound.begin()) {
                            // The upper bound is not quite what we need, rather the element before it
                            winningTransitionFinder--;
                            //std::cerr << "Found some transitions (2) ...\n";
                            allowedEscapeTransitions |= winningTransitionFinder->second.ExistAbstract(assumptionCounterPreCube).ExistAbstractSingleVar(isInftyCostPreVars[0]);
                            //BF_newDumpDot(*this,winningTransitionFinder->second,"Pre Post",filename.str()+"allowedEscapeTransitionAdditionNonAbstract.dot");
                            //BF_newDumpDot(*this,winningTransitionFinder->second.ExistAbstract(assumptionCounterPreCube).ExistAbstractSingleVar(isInftyCostPreVars[0]),"Pre Post",filename.str()+"allowedEscapeTransitionAddition.dot");

                        }

                        //BF_newDumpDot(*this,allowedEscapeTransitions,"Pre Post",filename.str()+"allowedEscapeTransitions.dot");

                        // 3. Iterate over the assumptions and find SCCs
                        for (unsigned int livenessAssumption = 0;livenessAssumption<livenessAssumptions.size();livenessAssumption++) {
                            BF winningSCCStates = !positionsAlreadyFoundToBeWinning;
                            BF oldWinningSCCStates = mgr.constantFalse();
                            BF transitions = mgr.constantFalse();
                            while (winningSCCStates != oldWinningSCCStates) {
                                oldWinningSCCStates = winningSCCStates;
                                transitions = nonTransitionalStatePreEncoding[livenessAssumption] & isInftyCostPreVars[0] & ((!safetyEnv) | (safetySys & (allowedEscapeTransitions | ((!livenessAssumptions[livenessAssumption]) & winningSCCStates.SwapVariables(varVectorPre,varVectorPost) & nonTransitionalStatePostEncoding[livenessAssumption] & isInftyCostPostVars[0]))));
                                winningSCCStates &= transitions.ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput);
                            }

                            // Compute transitive closure of the transitions, but exclude states that were previously winning.
                            //BF_newDumpDot(*this,transitions,"Pre Post",filename.str()+"transitiveBeforeClosure.dot");

                            // For computing the transitive closure, only the internal transitions are to be used.
                            BF transitiveClosureInternaltransitions = winningSCCStates & winningSCCStates.SwapVariables(varVectorPre,varVectorPost);
                            transitiveClosureInternaltransitions &= nonTransitionalStatePreEncoding[livenessAssumption] & ((!safetyEnv) | (safetySys & (((!livenessAssumptions[livenessAssumption]) & winningSCCStates.SwapVariables(varVectorPre,varVectorPost) & nonTransitionalStatePreEncoding[livenessAssumption] & isInftyCostPreVars[0])))) & nonTransitionalStatePostEncoding[livenessAssumption] & isInftyCostPostVars[0];
                            BF transitiveClosureInternaltransitionsOld = mgr.constantFalse();
                            while (transitiveClosureInternaltransitions!=transitiveClosureInternaltransitionsOld) {
                                transitiveClosureInternaltransitionsOld = transitiveClosureInternaltransitions;
                                transitiveClosureInternaltransitions |= (transitiveClosureInternaltransitions.SwapVariables(varVectorPost,transitiveClosureIntermediateVarVector)
                                        & transitiveClosureInternaltransitions.SwapVariables(varVectorPre,transitiveClosureIntermediateVarVector)).ExistAbstract(transitiveClosureIntermediateVarCube);
                            }
                            //BF_newDumpDot(*this,transitiveClosureInternaltransitions,"Pre Post",filename.str()+"transitiveClosure.dot");

                            // Prepare to cut out all transitions that are not somehow reversible
                            BF reversibleTransitions = transitiveClosureInternaltransitions &  transitiveClosureInternaltransitions.SwapVariables(varVectorPre,varVectorPost);
                            //BF_newDumpDot(*this,reversibleTransitions,"Pre Post",filename.str()+"reversibletransitions.dot");

                            BF finalTransitions = nonTransitionalStatePreEncoding[livenessAssumption] & isInftyCostPreVars[0] & (
                                (!safetyEnv)
                              | (safetySys
                                  & ((allowedEscapeTransitions & ((!nonTransitionalStatePostEncoding[livenessAssumption]) | livenessGuarantees[livenessGoal] | !isInftyCostPostVars[0]))
                                    | ((!livenessAssumptions[livenessAssumption]) & winningSCCStates.SwapVariables(varVectorPre,varVectorPost) & reversibleTransitions & isInftyCostPostVars[0]))));
                            BF finalSCCStates = winningSCCStates & finalTransitions.ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput);

                            //BF_newDumpDot(*this,finalSCCStates,"Pre Post",filename.str()+"finalSCCStates.dot");
                            //BF_newDumpDot(*this,finalTransitions,"Pre Post",filename.str()+"finalTransitions.dot");

                            // Compute the winning transitions
                            strategyDumpingData.push_back(std::pair<int,BF>(livenessGoal,finalTransitions));
                            // std::cerr << "Added Strategy dumping data " << strategyDumpingData.size() << " at line " << __LINE__ << std::endl;

                            //BF_newDumpDot(*this,winningSCCStates,"Pre Post",filename.str()+"winningSCCStates.dot");

                            transitionsAlreadyFoundToBeWinning |= finalTransitions;
                            positionsAlreadyFoundToBeWinning |= finalSCCStates;

                            winningTransitionsFound[currentTuple] = transitionsAlreadyFoundToBeWinning;
                            winningPositionsFound[currentTuple] = positionsAlreadyFoundToBeWinning;
                            //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"overallWinningStatesAfterSCCAdding.dot");

                        }

                        //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"wpnewTwo.dot");
                        //BF_newDumpDot(*this,transitionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"winningTrasitionsFoundNew.dot");

                        // Solve a reachability game "on top" in order to find states that are only winning
                        // because of the new SCC
                        BF allowedEndingTransitions = transitionsAlreadyFoundToBeWinning;
                        BF winningPositionsOld = mgr.constantTrue();
                        BF winningPositionsNew = mgr.constantFalse();
                        BF oldPositionsWinning = positionsAlreadyFoundToBeWinning;
                        while (winningPositionsNew!=winningPositionsOld) {
                            winningPositionsOld = winningPositionsNew;
                            BF newWinningTransitions = (isInftyCostPreVars[0]) & safetySys & (allowedEndingTransitions | (winningPositionsNew.SwapVariables(varVectorPre,varVectorPost)));
                            winningPositionsNew = ((!safetyEnv) | newWinningTransitions.ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                            strategyDumpingData.push_back(std::pair<int,BF>(livenessGoal,newWinningTransitions & winningPositionsNew & preTransitionalStateEncoding));
                            // std::cerr << "Added Strategy dumping data " << strategyDumpingData.size() << " at line " << __LINE__ << std::endl;
                            transitionsAlreadyFoundToBeWinning |= newWinningTransitions & winningPositionsNew & preTransitionalStateEncoding;
                            //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,NULL,filename.str()+"winningPosOld.dot");
                            positionsAlreadyFoundToBeWinning |= winningPositionsNew & preTransitionalStateEncoding;
                            //BF_newDumpDot(*this,newWinningTransitions,NULL,filename.str()+"winningTrans.dot");
                            //BF_newDumpDot(*this,winningPositionsNew,NULL,filename.str()+"winningPosNew.dot");
                        }

                        //BF_newDumpDot(*this,winningPositionsNew,"Pre Post",filename.str()+"wpnew2.dot");
                        //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"positionsalreadyfoundtobewinning2.dot");
                        //BF_newDumpDot(*this,transitionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"transitionsalreadyfoundtobewinning2.dot");

                        winningTransitionsFound[currentTuple] = transitionsAlreadyFoundToBeWinning;
                        winningPositionsFound[currentTuple] = positionsAlreadyFoundToBeWinning;


                        // Add new potential elements to the TODO list.
                        if (positionsWinningBeforeProcessingThisCostTuple != positionsAlreadyFoundToBeWinning) {
                            //BF_newDumpDot(*this,(!positionsWinningBeforeProcessingThisCostTuple) & positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"winningPositionsAdded.dot");
                            // Add new elements to the TODO list
                            // std::cerr << "Inserting new elements into the TODO list...\n";
                            assert(currentTuple.getActionCost()==std::numeric_limits<double>::infinity());
                            todo.insert(MK_COST_TUPLE(currentTuple.getWaitingCost()+1,currentTuple.getActionCost()));
                        }

                        // =========================== INFTY COST END =========================

                    } // If waiting cost>0

                } else {

                    // Finite Cost case

                    // Help me debug!
                    // std::ostringstream filename;
                    // filename << "/tmp/guarantee" << livenessGoal << "waiting0action" << currentTuple;

                    // Waiting cost 0
                    //
                    // Start with listing the goal transitions...
                    BF allowedEndingTransitions = mgr.constantFalse();
                    for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                        if (MK_COST_TUPLE(0,it->first)<=currentTuple) {
                            allowedEndingTransitions |= it->second & safetySys & livenessGuarantees[livenessGoal];
                        }
                    }

                    //BF_newDumpDot(*this,allowedEndingTransitions,"Pre Post",filename.str()+"allowed1.dot");

                    // .... and add the allowed transitions to positions that are already know to be winning ...
                    for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                        double allowedActionCost = currentTuple.getActionCost() - it->first;
                        // Numeric correction
                        allowedActionCost = std::nextafter(allowedActionCost,std::numeric_limits<double>::max());
                        // std::cerr << "Allowed Action Cost: " << allowedActionCost << std::endl;
                        auto winningStateFinder = winningPositionsFound.upper_bound(MK_COST_TUPLE(currentTuple.getWaitingCost(),allowedActionCost));
                        if (winningStateFinder != winningPositionsFound.begin()) {
                            // The upper bound is not quite what we need, rather the element before it
                            winningStateFinder--;
                            // std::cerr << "Found some transitions...\n";
                            allowedEndingTransitions |= safetySys & winningStateFinder->second.SwapVariables(varVectorPre,varVectorPost) & it->second;
                            //BF_newDumpDot(*this,winningStateFinder->second,"Pre Post",filename.str()+"winningStateFinder.dot");
                        }
                    }

                    //BF_newDumpDot(*this,allowedEndingTransitions,"Pre Post",filename.str()+"allowed2.dot");


                    // Reachability game
                    BF winningPositionsOld = mgr.constantTrue();
                    BF winningPositionsNew = mgr.constantFalse();
                    BF oldPositionsWinning = positionsAlreadyFoundToBeWinning;
                    while (winningPositionsNew!=winningPositionsOld) {
                        winningPositionsOld = winningPositionsNew;
                        BF newWinningTransitions = preTransitionalStateEncoding & !isInftyCostPreVars[0] & safetySys & (allowedEndingTransitions | (transitionCosts[0.0] & winningPositionsNew.SwapVariables(varVectorPre,varVectorPost)));
                        winningPositionsNew = ((!safetyEnv) | newWinningTransitions.ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                        strategyDumpingData.push_back(std::pair<int,BF>(livenessGoal,newWinningTransitions));
                        // std::cerr << "Added Strategy dumping data " << strategyDumpingData.size() << " at line " << __LINE__ << std::endl;
                        transitionsAlreadyFoundToBeWinning |= newWinningTransitions;
                        //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,NULL,filename.str()+"winningPosOld.dot");
                        positionsAlreadyFoundToBeWinning |= winningPositionsNew;
                        //BF_newDumpDot(*this,newWinningTransitions,NULL,filename.str()+"winningTrans.dot");
                        //BF_newDumpDot(*this,winningPositionsNew,NULL,filename.str()+"winningPosNew.dot");
                    }

                    //BF_newDumpDot(*this,winningPositionsNew,"Pre Post",filename.str()+"wpnew.dot");
                    //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"positionsalreadyfoundtobewinning.dot");

                    winningTransitionsFound[currentTuple] = transitionsAlreadyFoundToBeWinning;
                    winningPositionsFound[currentTuple] = positionsAlreadyFoundToBeWinning;

                    if (currentTuple.getWaitingCost()>0) {

                        // Waiting cost > 0
                        // Take not too costly winning transitions and compute a suitable SCC,
                        // all transitions that we found for the waiting cost=0 are ok if reached before
                        // the SCC.

                        // Compute the Escape Transitions, consisting of:
                        // 1. Goal transitions
                        BF allowedEscapeTransitions = mgr.constantFalse();
                        for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                            if (MK_COST_TUPLE(currentTuple.getWaitingCost()-1,it->first)<=currentTuple) {
                                allowedEscapeTransitions |= it->second & safetySys & livenessGuarantees[livenessGoal];
                            }
                        }

                        // 2. Transitions that are already known to be winning "cheaply"
                        // -- abstract from the pre assumption that is being waited for
                        auto winningTransitionFinder = winningTransitionsFound.upper_bound(MK_COST_TUPLE(currentTuple.getWaitingCost()-1,currentTuple.getActionCost()));
                        if (winningTransitionFinder != winningTransitionsFound.begin()) {
                            // The upper bound is not quite what we need, rather the element before it
                            winningTransitionFinder--;
                            // std::cerr << "Found some transitions (2) ...\n";
                            allowedEscapeTransitions |= winningTransitionFinder->second.ExistAbstract(assumptionCounterPreCube).ExistAbstractSingleVar(isInftyCostPreVars[0]);
                            //BF_newDumpDot(*this,winningTransitionFinder->second,"Pre Post",filename.str()+"allowedEscapeTransitionAdditionNonAbstract.dot");
                            //BF_newDumpDot(*this,winningTransitionFinder->second.ExistAbstract(assumptionCounterPreCube).ExistAbstractSingleVar(isInftyCostPreVars[0]),"Pre Post",filename.str()+"allowedEscapeTransitionAddition.dot");

                        }

                        //BF_newDumpDot(*this,allowedEscapeTransitions,"Pre Post",filename.str()+"allowedEscapeTransitions.dot");

                        // 3. Iterate over the assumptions and find SCCs
                        for (unsigned int livenessAssumption = 0;livenessAssumption<livenessAssumptions.size();livenessAssumption++) {
                            BF cheapTransitions = transitionCosts[0.0];
                            BF winningSCCStates = !positionsAlreadyFoundToBeWinning;
                            BF oldWinningSCCStates = mgr.constantFalse();
                            BF transitions = mgr.constantFalse();
                            while (winningSCCStates != oldWinningSCCStates) {
                                oldWinningSCCStates = winningSCCStates;
                                transitions = nonTransitionalStatePreEncoding[livenessAssumption] & !isInftyCostPreVars[0] & ((!safetyEnv) | (safetySys & (allowedEscapeTransitions | ((!livenessAssumptions[livenessAssumption]) & winningSCCStates.SwapVariables(varVectorPre,varVectorPost) & cheapTransitions & nonTransitionalStatePostEncoding[livenessAssumption] & !isInftyCostPostVars[0]))));
                                winningSCCStates &= transitions.ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput);
                            }

                            // Compute transitive closure of the transitions, but exclude states that were previously winning.
                            //BF_newDumpDot(*this,transitions,"Pre Post",filename.str()+"transitiveBeforeClosure.dot");

                            // For computing the transitive closure, only the internal transitions are to be used.
                            BF transitiveClosureInternaltransitions = (!winningPositionsFound[currentTuple]) & winningSCCStates & winningSCCStates.SwapVariables(varVectorPre,varVectorPost);
                            transitiveClosureInternaltransitions &= nonTransitionalStatePreEncoding[livenessAssumption] & ((!safetyEnv) | (safetySys & (((!livenessAssumptions[livenessAssumption]) & winningSCCStates.SwapVariables(varVectorPre,varVectorPost) & cheapTransitions & nonTransitionalStatePreEncoding[livenessAssumption] & !isInftyCostPreVars[0])))) & nonTransitionalStatePostEncoding[livenessAssumption] & !isInftyCostPostVars[0];
                            BF transitiveClosureInternaltransitionsOld = mgr.constantFalse();
                            while (transitiveClosureInternaltransitions!=transitiveClosureInternaltransitionsOld) {
                                transitiveClosureInternaltransitionsOld = transitiveClosureInternaltransitions;
                                transitiveClosureInternaltransitions |= (transitiveClosureInternaltransitions.SwapVariables(varVectorPost,transitiveClosureIntermediateVarVector)
                                        & transitiveClosureInternaltransitions.SwapVariables(varVectorPre,transitiveClosureIntermediateVarVector)).ExistAbstract(transitiveClosureIntermediateVarCube);
                            }
                            //BF_newDumpDot(*this,transitiveClosureInternaltransitions,"Pre Post",filename.str()+"transitiveClosure.dot");

                            // Prepare to cut out all transitions that are not somehow reversible
                            BF reversibleTransitions = transitiveClosureInternaltransitions &  transitiveClosureInternaltransitions.SwapVariables(varVectorPre,varVectorPost);
                            //BF_newDumpDot(*this,reversibleTransitions,"Pre Post",filename.str()+"reversibletransitions.dot");

                            BF finalTransitions = nonTransitionalStatePreEncoding[livenessAssumption] & !isInftyCostPreVars[0] & (
                                (!safetyEnv)
                              | (safetySys
                                  & ((allowedEscapeTransitions & ((!nonTransitionalStatePostEncoding[livenessAssumption]) | livenessGuarantees[livenessGoal] | isInftyCostPostVars[0]))
                                    | ((!livenessAssumptions[livenessAssumption]) & winningSCCStates.SwapVariables(varVectorPre,varVectorPost) & reversibleTransitions & cheapTransitions & !isInftyCostPostVars[0]))));
                            BF finalSCCStates = winningSCCStates & finalTransitions.ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput);

                            //BF_newDumpDot(*this,finalSCCStates,"Pre Post",filename.str()+"finalSCCStates.dot");
                            //BF_newDumpDot(*this,finalTransitions,"Pre Post",filename.str()+"finalTransitions.dot");

                            // Compute the winning transitions
                            strategyDumpingData.push_back(std::pair<int,BF>(livenessGoal,finalTransitions));
                            // std::cerr << "Added Strategy dumping data " << strategyDumpingData.size() << " at line " << __LINE__ << std::endl;

                            //BF_newDumpDot(*this,winningSCCStates,"Pre Post",filename.str()+"winningSCCStates.dot");

                            transitionsAlreadyFoundToBeWinning |= finalTransitions;
                            positionsAlreadyFoundToBeWinning |= finalSCCStates;

                            winningTransitionsFound[currentTuple] = transitionsAlreadyFoundToBeWinning;
                            winningPositionsFound[currentTuple] = positionsAlreadyFoundToBeWinning;
                            //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"overallWinningStatesAfterSCCAdding.dot");

                        }

                        //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"wpnewTwo.dot");
                        //BF_newDumpDot(*this,transitionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"winningTrasitionsFoundNew.dot");

                        // Solve a reachability game "on top" in order to find states that are only winning
                        // because of the new SCC
                        allowedEndingTransitions = transitionsAlreadyFoundToBeWinning;
                        BF winningPositionsOld = mgr.constantTrue();
                        BF winningPositionsNew = positionsAlreadyFoundToBeWinning;
                        BF oldPositionsWinning = mgr.constantFalse();
                        while (winningPositionsNew!=winningPositionsOld) {
                            winningPositionsOld = winningPositionsNew;
                            BF newWinningTransitions = (!(isInftyCostPreVars[0])) & safetySys & (allowedEndingTransitions | (transitionCosts[0.0] & winningPositionsNew.SwapVariables(varVectorPre,varVectorPost)));
                            winningPositionsNew = ((!safetyEnv) | newWinningTransitions.ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                            strategyDumpingData.push_back(std::pair<int,BF>(livenessGoal,newWinningTransitions & winningPositionsNew & preTransitionalStateEncoding));
                            // std::cerr << "Added Strategy dumping data " << strategyDumpingData.size() << " at line " << __LINE__ << std::endl;
                            transitionsAlreadyFoundToBeWinning |= newWinningTransitions & winningPositionsNew & preTransitionalStateEncoding;
                            //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,NULL,filename.str()+"winningPosOld.dot");
                            positionsAlreadyFoundToBeWinning |= winningPositionsNew & preTransitionalStateEncoding;
                            //BF_newDumpDot(*this,newWinningTransitions,NULL,filename.str()+"winningTrans.dot");
                            //BF_newDumpDot(*this,winningPositionsNew,NULL,filename.str()+"winningPosNew.dot");
                        }

                        //BF_newDumpDot(*this,winningPositionsNew,"Pre Post",filename.str()+"wpnew2.dot");
                        //BF_newDumpDot(*this,positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"positionsalreadyfoundtobewinning2.dot");
                        //BF_newDumpDot(*this,transitionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"transitionsalreadyfoundtobewinning2.dot");

                        winningTransitionsFound[currentTuple] = transitionsAlreadyFoundToBeWinning;
                        winningPositionsFound[currentTuple] = positionsAlreadyFoundToBeWinning;

                    }

                    // Add new potential elements to the TODO list.
                    if (positionsWinningBeforeProcessingThisCostTuple != positionsAlreadyFoundToBeWinning) {
                        //BF_newDumpDot(*this,(!positionsWinningBeforeProcessingThisCostTuple) & positionsAlreadyFoundToBeWinning,"Pre Post",filename.str()+"winningPositionsAdded.dot");
                        // Add new elements to the TODO list
                        // std::cerr << "Inserting new elements into the TODO list...\n";
                        todo.insert(MK_COST_TUPLE(currentTuple.getWaitingCost()+1,currentTuple.getActionCost()));
                        for (auto it = transitionCosts.begin();it!=transitionCosts.end();it++) {
                            // No need to consider the cost-0 case again
                            if (it->first != 0) {
                                // std::cerr << "Insert " << it->first+currentTuple.getActionCost() << std::endl;
                                todo.insert(MK_COST_TUPLE(currentTuple.getWaitingCost(),it->first+currentTuple.getActionCost()));
                                todo.insert(MK_COST_TUPLE(currentTuple.getWaitingCost()+1,it->first+currentTuple.getActionCost()));
                            }
                        }
                    }
                }

            }

            // Let's dump the strategy data!
            /*unsigned int i = 0;
            for (auto it = strategyDumpingData.begin();it!=strategyDumpingData.end();it++) {
                if (it->first == livenessGoal) {
                    std::ostringstream filename2;
                    filename2 << "/tmp/guarantee" << livenessGoal << "winningTransitions" << i++ << ".dot";
                    BF_newDumpDot(*this,it->second,"Pre Post",filename2.str());
                }
            }*/



            // Alter "initSys", so that the system initially uses the best values available
            if (livenessGoal==0) {
                //BF_newDumpDot(*this,initSys,"Pre","/tmp/initSysInitial.dot");
                //BF_newDumpDot(*this,winningPositions,"Pre","/tmp/winningPositions.dot");

                BF initEnvCasesCoveredAlready = mgr.constantFalse();
                BF initSysUpdates = mgr.constantFalse();
                //unsigned int nr = 0;
                for (auto it : winningPositionsFound) {
                    //std::ostringstream theseWinningPos;
                    //theseWinningPos << "/tmp/twp" << nr++;
                    //BF_newDumpDot(*this,it.second,"Pre",(theseWinningPos.str()+".dot").c_str());
                    // Determinize the additional bits of it.second
                    BF newStates = it.second;
                    for (unsigned int j=0; j<addedHelperBitsInImplementationPreVars.size();j++) {
                        BF var = addedHelperBitsInImplementationPreVars[j];
                        newStates &= (!var) | var & !((newStates & !var).ExistAbstractSingleVar(var));
                    }
                    initSysUpdates |= winningPositions & newStates & !initEnvCasesCoveredAlready;
                    if (!isSysInitRoboticsSemantics) {
                        initEnvCasesCoveredAlready |= (winningPositions & newStates & initSys).ExistAbstract(varCubePreOutput);
                    } else {
                        initEnvCasesCoveredAlready |= (winningPositions & newStates).ExistAbstract(addedHelperBitsInImplementationPreCube);
                    }
                }
                initSys &= initSysUpdates;
                //BF_newDumpDot(*this,initSysUpdates,"Pre","/tmp/initSysUpdate.dot");
                //BF_newDumpDot(*this,initEnvCasesCoveredAlready,"Pre","/tmp/initSysCovered.dot");
                //BF_newDumpDot(*this,initSys,"Pre","/tmp/initSysFinal.dot");
                //BF_newDumpDot(*this,(winningPositions & initSys & initEnv),"Pre","/tmp/todolist.dot");
                //BF_newDumpDot(*this,(transitionCosts[0.0] & safetyEnv & safetySys) & variables[8] & variables[10],"Pre Post","/tmp/freeTransitions.dot");
            }

        } // Iteration through all liveness goals completed


    }

    /**
     * @brief Alternative checkRealizability function --
     *        makes sure that a strategy is computed as well if the specification
     *        is realizable.
     */
    void checkRealizability() {
        if (isSysInitRoboticsSemantics) {
            T::computeWinningPositions();
            BF result = (initEnv & initSys).Implies(winningPositions).UnivAbstract(varCubePre);
            if (!result.isConstant()) throw "Internal error: Could not establish realizability/unrealizability of the specification.";
            realizable = result.isTrue();
            if (realizable) {
                computeCostOptimalStrategy();
            }
        } else {
            T::checkRealizability();
            if (realizable) {
                computeCostOptimalStrategy();
            }
        }
    }

};







#endif

