#ifndef __EXTENSION_INCOMPLETE_INFORMATION_ESTIMATOR_SYNTHESIS_HPP
#define __EXTENSION_INCOMPLETE_INFORMATION_ESTIMATOR_SYNTHESIS_HPP

#include "gr1context.hpp"

/**
 * An extension that allows to compute an estimator from the description of
 * a domain that can be used for synthesis under incomplete information
 */
template<class T> class XIncompleteInformationEstimatorSynthesis : public T {

    using T::initEnv;
    using T::initSys;
    using T::safetyEnv;
    using T::safetySys;
    using T::mgr;
    using T::addVariable;
    using T::lineNumberCurrentlyRead;
    using T::parseBooleanFormula;
    using T::variableNames;
    using T::variables;
    using T::varCubePre;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::doesVariableInheritType;
    using T::varCubePost;
    using T::preVars;

    // Variables local to this plugin
    BF strategy;
    SlugsVarCube varCubeUnobservables{UnobservablePre,UnobservablePost,this};
    SlugsVarCube varCubeEstimationPost{EstimationPost,this};
    SlugsVarCube varCubeEverythingButEstimatorPreAndObservables{UnobservablePre,UnobservablePost,EstimationPost,ControllablePost,this};

protected:

    XIncompleteInformationEstimatorSynthesis<T>(std::list<std::string> &filenames) : T(filenames) {}

    /**
     * @brief Reads input file in the special input format
     * @param filenames
     */
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

        // The readmode variable stores in which chapter of the input file we are
        int readMode = -1;
        std::string currentLine;
        std::string currentPropertyName = "";
        lineNumberCurrentlyRead = 0;
        while (std::getline(inFile,currentLine)) {
            lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                if (currentLine[0]=='[') {
                    if (currentLine=="[OBSERVABLE_INPUT]") {
                        readMode = 0;
                    } else if (currentLine=="[UNOBSERVABLE_INPUT]") {
                        readMode = 1;
                    } else if (currentLine=="[CONTROLLABLE_INPUT]") {
                        readMode = 7;
                    } else if (currentLine=="[OUTPUT]") {
                        readMode = 2;
                    } else if (currentLine=="[ENV_INIT]") {
                        readMode = 3;
                    } else if (currentLine=="[SYS_INIT]") {
                        readMode = 4;
                    } else if (currentLine=="[ENV_TRANS]") {
                        readMode = 5;
                    } else if (currentLine=="[SYS_TRANS]") {
                        readMode = 6;
                    } else {
                        std::cerr << "Sorry. Didn't recognize category " << currentLine << "\n";
                        throw "Aborted.";
                    }
                } else {
                    if (readMode==0) {
                        addVariable(ObservablePre,currentLine);
                        addVariable(ObservablePost,currentLine+"'");
                    } else if (readMode==1) {
                        addVariable(UnobservablePre,currentLine);
                        addVariable(UnobservablePost,currentLine+"'");
                    } else if (readMode==2) {
                        addVariable(EstimationPre,currentLine);
                        addVariable(EstimationPost,currentLine+"'");
                    } else if (readMode==3) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(ObservablePre);
                        allowedTypes.insert(ControllablePre);
                        allowedTypes.insert(UnobservablePre);
                        initEnv &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==4) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(ObservablePre);
                        allowedTypes.insert(ControllablePre);
                        allowedTypes.insert(UnobservablePre);
                        allowedTypes.insert(EstimationPre);
                        initSys &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==5) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(ObservablePre);
                        allowedTypes.insert(ControllablePre);
                        allowedTypes.insert(UnobservablePre);
                        allowedTypes.insert(EstimationPre);
                        allowedTypes.insert(ObservablePost);
                        allowedTypes.insert(UnobservablePost);
                        allowedTypes.insert(ControllablePost);
                        BF newSafetyEnvPart = parseBooleanFormula(currentLine,allowedTypes);
                        safetyEnv &= newSafetyEnvPart;
                    } else if (readMode==6) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(ObservablePre);
                        allowedTypes.insert(ControllablePre);
                        allowedTypes.insert(UnobservablePre);
                        allowedTypes.insert(EstimationPre);
                        allowedTypes.insert(ObservablePost);
                        allowedTypes.insert(UnobservablePost);
                        allowedTypes.insert(EstimationPost);
                        allowedTypes.insert(ControllablePost);
                        safetySys &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==7) {
                        addVariable(ControllablePre,currentLine);
                        addVariable(ControllablePost,currentLine+"'");
                    } else {
                        std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                        throw "Found a line in the specification file that has no proper categorial context.";
                    }
                    currentPropertyName = ""; // Is always reset after a semantically non-null property line.
                }
            }
        }

        // Check if variable names have been used twice
        std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
        if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");
    }

    //===========================================
    // Writing out the estimator
    //===========================================
    std::map<int,int> varIndicesByNodeIndex;

    int recursePrintBFAsSlugsExpression(BF bf,std::map<size_t,int> &nodes,std::vector<std::string> &parts) {

        std::map<size_t,int>::iterator it = nodes.find(bf.getHashCode());
        if (it!=nodes.end()) {
            return it->second;
        } else {
            if (bf.isFalse()) {
                parts.push_back("0");
                nodes[bf.getHashCode()] = parts.size()-1;
                return parts.size()-1;
            } else if (bf.isTrue()) {
                parts.push_back("1");
                nodes[bf.getHashCode()] = parts.size()-1;
                return parts.size()-1;
            } else {
                assert(!(bf.isConstant()));
                int varNumber = varIndicesByNodeIndex.at(bf.readNodeIndex());
                BF trueSucc = (bf & variables[varNumber]).ExistAbstractSingleVar(variables[varNumber]);
                BF falseSucc = (bf & !variables[varNumber]).ExistAbstractSingleVar(variables[varNumber]);
                int qTrue = recursePrintBFAsSlugsExpression(trueSucc,nodes,parts);
                int qFalse = recursePrintBFAsSlugsExpression(falseSucc,nodes,parts);
                std::ostringstream os;
                os << "| & " << variableNames[varNumber] << " ? " << qTrue << " & ! " << variableNames[varNumber] << " ? " << qFalse;
                parts.push_back(os.str());
                nodes[bf.getHashCode()] = parts.size()-1;
                return parts.size()-1;
            }
        }
    }

    void printBFAsSlugsExpression(BF bf) {

        // Create Variable Name list
        for (unsigned int i=0;i<variables.size();i++) {
            varIndicesByNodeIndex[variables[i].readNodeIndex()] = i;
        }

        // Allocate Node counters
        std::map<size_t,int> nodes;
        std::vector<std::string> parts;

        // Recurse
        int part = recursePrintBFAsSlugsExpression(bf,nodes,parts);
        assert(part==parts.size()-1);
        std::cout << "$ " << parts.size();
        for (auto it = parts.begin();it!=parts.end();it++) {
            std::cout << " " << *it;
        }
    }


    /** Compute the Estimator **/
    void execute() {

        // Compute reachable states
        BF reachableStates = initSys & initEnv;
        BF oldReachableStates = mgr.constantFalse();
        BF allSafetyConstraints = safetyEnv & safetySys;

        BF_newDumpDot(*this,reachableStates,NULL,"/tmp/reachStart.dot");
        BF_newDumpDot(*this,initEnv,NULL,"/tmp/initEnvStart.dot");
        BF_newDumpDot(*this,initSys,NULL,"/tmp/initSysStart.dot");


        // Classical algorithm
        while (reachableStates!=oldReachableStates) {
            std::cerr << ".";
            oldReachableStates = reachableStates;
            BF newStates = reachableStates.AndAbstract(allSafetyConstraints,varCubePre).SwapVariables(varVectorPre,varVectorPost);
            //assert(newStates == (reachableStates & safetyEnv & safetySys).ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost));
            reachableStates |= newStates;
        }

        BF_newDumpDot(*this,reachableStates,NULL,"/tmp/reachEnd.dot");

        if (reachableStates.isFalse()) {
            std::cerr << "=========\nWARNING!\n=========\nThere is no initial state in the estimator model.\n\n";
        }

        // Focussed algorithm
        /*while (reachableStates!=oldReachableStates) {
            std::cerr << ".";
            oldReachableStates = reachableStates;
            BF oldInnerReachableStates;
            do {
                std::cerr << "x";
                oldInnerReachableStates = reachableStates;
                BF newStates = reachableStates.AndAbstract(allSafetyConstraints,varCubePre).SwapVariables(varVectorPre,varVectorPost);
                BF ored = (reachableStates | newStates);
                BF newReachable = ored.optimizeRestrict(oldReachableStates | !ored);
                if (newReachable==reachable) && (ored!=reachable) {
                    reachableStates = ored;
                } else {
                    reachableStates = newReachable;
                }
            } while (reachableStates != oldInnerReachableStates);

            //assert(newStates == (reachableStates & safetyEnv & safetySys).ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost));
        }*/

        // Compute the Estimator
        strategy = ( (!reachableStates)  | (!safetyEnv) | safetySys).UnivAbstract(varCubeUnobservables);
        sleep(10);

        mgr.setReorderingMaxBlowup(1.03f);

        BF_newDumpDot(*this,strategy,NULL,
                      "/tmp/stratNoMinMax.dot");
        // BF_newDumpDot(*this,(!reachableStates) | (!safetyEnv) | safetySys,NULL,
        //              "/tmp/prestrat.dot");

        // Fix the estimator's behavior for variables starting with "min_" or "max_".
        std::cerr << "!";
        for (int i=variables.size()-1;i>=0;i--) {
            std::string variableName = variableNames[i];
            if (doesVariableInheritType(i,EstimationPre)) {

                // The post variable comes after the pre variables
                assert(doesVariableInheritType(i+1,EstimationPost));
                assert(variableNames[i+1]==variableName+"'");

                BF preVar = variables[i];
                BF postVar = variables[i+1];

                if (variableName.substr(0,4)=="min_") {
                    // Try to maximize variables
                    BF smod = strategy & postVar;
                    // BF_newDumpDot(*this,smod,NULL,"/tmp/smod.dot");
                    strategy &= (!(smod.ExistAbstract(varCubeEstimationPost))) | postVar;
                    // BF_newDumpDot(*this,postVar,NULL,"/tmp/postvar.dot");
                    // BF_newDumpDot(*this,(!(smod.ExistAbstract(varCubeEstimationPost))) | postVar,NULL,"/tmp/conjunct.dot");
                } else if (variableName.substr(0,4)=="max_") {
                    // Try to minimize variables
                    BF smod = strategy & !postVar;
                    strategy &= (!(smod.ExistAbstract(varCubeEstimationPost))) | !postVar;
                }
            }
        }

        // Test estimator!
#ifndef NDEBUG
        for (unsigned int i=0;i<variables.size();i++) {
            std::string variableName = variableNames[i];
            if ((variableName.substr(0,4)=="min_") || (variableName.substr(0,4)=="max_")) {
                if (doesVariableInheritType(i,EstimationPost)) {
                    std::cerr << "Note: Checking determinism for " << variableName << std::endl;
                    BF overlap1 = (strategy & variables[i]).ExistAbstract(varCubeEstimationPost);
                    BF overlap2 = (strategy & !variables[i]).ExistAbstract(varCubeEstimationPost);
                    assert((overlap1 & overlap2).isFalse());
                }
            }
        }
#endif

        BF_newDumpDot(*this,strategy,NULL,"/tmp/strat.dot");

        // Print strategy for the estimator
        std::cerr << "...computed belief space abstraction...\n";
        std::cout << "#=======================================================\n";
        std::cout << "#= Belief Space Abstraction                            =\n";
        std::cout << "#=======================================================\n";
        std::cout << "# --- End of the specification for the report\n";
        std::cout << "[SYS_TRANS]\n";
        BF_newDumpDot(*this,strategy,NULL,"/tmp/estimatorGuarantees.dot");
        printBFAsSlugsExpression(strategy);
        std::cout << std::endl;

        // Compute assumptions for the environment:
        // 1. New reachable states
        BF combined = safetyEnv & safetySys & strategy;
        std::cerr << "...computing assumptions...\n";
        reachableStates = initSys & initEnv;
        BF_newDumpDot(*this,reachableStates,NULL,"/tmp/reachable0.dot");
        oldReachableStates = mgr.constantFalse();
        while (reachableStates!=oldReachableStates) {
            oldReachableStates = reachableStates;
            reachableStates |= (reachableStates & combined).ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);
        }
        BF_newDumpDot(*this,reachableStates,NULL,"/tmp/reachable2.dot");
        BF environmentAssumption = (reachableStates & safetyEnv).ExistAbstract(varCubeEverythingButEstimatorPreAndObservables);
        std::cout << "[ENV_TRANS]\n";
        printBFAsSlugsExpression(environmentAssumption);
        BF_newDumpDot(*this,environmentAssumption,NULL,"/tmp/envass.dot");
        std::cout << std::endl;


        // Perform some checks
        BF deadEnds = (!(environmentAssumption.ExistAbstract(varCubePost))) & reachableStates;
        if (!(deadEnds.isFalse())) {
            std::cerr << "============================================================\n";
            std::cerr << "WARNING! There are environment dead-ends in the abstraction!\n";
            std::cerr << "============================================================\n";
            BF_newDumpDot(*this,deadEnds,NULL,"/tmp/envDeadEnd.dot");
            BF_newDumpDot(*this,weakDeterminize(deadEnds,preVars),NULL,"/tmp/envDeadEndDet.dot");
        }
    }

    BF weakDeterminize(BF in, std::vector<BF> vars) {
        for (auto it = vars.begin();it!=vars.end();it++) {
            BF check = in.UnivAbstractSingleVar(*it);
            if (!(check.isFalse())) {
                in = check;
            } else {
                BF res = in & !(*it);
                if (res.isFalse()) {
                    in = in & *it;
                } else {
                    in = res;
                }
            }
        }
        return in;
    }

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XIncompleteInformationEstimatorSynthesis<T>(filenames);
    }


};

#endif
