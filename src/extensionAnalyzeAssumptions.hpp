#ifndef __EXTENSION_ANALYZE_ASSUMPTIONS_HPP
#define __EXTENSION_ANALYZE_ASSUMPTIONS_HPP

#include "gr1context.hpp"
#include <string>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

/**
 * A class for analyzing which assumptions are actually needed, and which assumptions help with
 * reducing reactive distances to a goal
 */
template<class T> class XAnalyzeAssumptions : public T {
protected:

    using T::initSys;
    using T::initEnv;
    using T::safetyEnv;
    using T::safetySys;
    using T::lineNumberCurrentlyRead;
    using T::addVariable;
    using T::parseBooleanFormula;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::mgr;
    using T::variables;
    using T::variableNames;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePre;
    using T::varCubePostInput;
    using T::varCubePostOutput;
    using T::varCubePreInput;
    using T::varCubePreOutput;

    std::vector<BF> safetyEnvParts;
    std::vector<std::string> safetyEnvPartNames;
    std::vector<std::string> livenessEnvPartNames;

    XAnalyzeAssumptions<T>(std::list<std::string> &filenames) : T(filenames) {}

    /**
     * @brief Reads input file. Assumptionally stores the safety assumptions one-by-one in
     *        order to allow a later analysis.
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
            if (currentLine.substr(0,2)=="##") {
                currentPropertyName = currentLine.substr(2,std::string::npos);
                boost::trim(currentPropertyName);
            } else if ((currentLine.length()>0) && (currentLine[0]!='#')) {
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
                        addVariable(PreInput,currentLine);
                        addVariable(PostInput,currentLine+"'");
                    } else if (readMode==1) {
                        addVariable(PreOutput,currentLine);
                        addVariable(PostOutput,currentLine+"'");
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
                        BF newSafetyEnvPart = parseBooleanFormula(currentLine,allowedTypes);
                        safetyEnvParts.push_back(newSafetyEnvPart);
                        safetyEnv &= newSafetyEnvPart;
                        if (currentPropertyName.length()>0) {
                            safetyEnvPartNames.push_back(currentPropertyName);
                        } else {
                            std::ostringstream propertyName;
                            propertyName << "Safety Assumption " << safetyEnvParts.size();
                            safetyEnvPartNames.push_back(propertyName.str());
                        }
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
                        if (currentPropertyName.length()>0) {
                            livenessEnvPartNames.push_back(currentPropertyName);
                        } else {
                            std::ostringstream propertyName;
                            propertyName << "Liveness Assumption " << livenessAssumptions.size();
                            livenessEnvPartNames.push_back(propertyName.str());
                        }
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
                    currentPropertyName = ""; // Is always reset after a semantically non-null property line.
                }
            }
        }

        // Check if variable names have been used twice
        std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
        if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

        // Make sure that there is at least one liveness assumption and one liveness guarantee
        // The synthesis algorithm might be unsound otherwise
        if (livenessAssumptions.size()==0) {
            livenessAssumptions.push_back(mgr.constantTrue());
            livenessEnvPartNames.push_back("Automatically added TRUE liveness assumption");
        }
        if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
    }

    // Storage space for a general winning strategy.
    // First index is the liveness guarantee number,
    // the second one is the iteration of the middle fixed point.
    // The strategy is overapproximative in the sense that it may needlessly switch
    // between liveness goals being waiting for. This strictly speaking makes it
    // incorrect, but in this way, we operapproximate any winning strategy that a
    // GR(1) tool could compute. It we obtain with this plugin the information
    // that a liveness assumption is not needed in any strategy, then this thus
    // holds for any order of the liveness assumptions.
    std::vector<std::vector<BF> > overapproximativeWinningStrategy;

    /**
     * @brief This function computes the reactive distances of the positions in the game.
     *        It also stores a general strategy into the WinningStategy Storage
     * @param distanceStorage Where the reactive distances of the positions
     *        that are found to be winning are to be stored
     * @return the winning positions
     */
    BF computeReactiveDistancesAndWinningPositions(std::vector<std::vector<BF> > &distanceStorage) {
        BFFixedPoint nu2(mgr.constantTrue());
        for (;!nu2.isFixedPointReached();) {

            overapproximativeWinningStrategy.clear();
            distanceStorage.clear();
            distanceStorage.resize(livenessGuarantees.size());
            BF nextContraintsForGoals = mgr.constantTrue();

            for (unsigned int j=0;j<livenessGuarantees.size();j++) {

                overapproximativeWinningStrategy.push_back(std::vector<BF>());

                 BF livetransitions = livenessGuarantees[j] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost));

                 BFFixedPoint mu1(mgr.constantFalse());
                 int round = 0;
                 for (;!mu1.isFixedPointReached();) {
                     round++;

                     // Update the set of transitions that lead closer to the goal.
                     livetransitions |= mu1.getValue().SwapVariables(varVectorPre,varVectorPost);

                     // Iterate over the liveness assumptions. Store the positions that are found to be winning for *any*
                     // of them into the variable 'goodForAnyLivenessAssumption'.
                     BF goodForAnyLivenessAssumption = mu1.getValue();
                     BF allFoundPaths = mgr.constantFalse();
                     for (unsigned int i=0;i<livenessAssumptions.size();i++) {

                         // Prepare the variable 'foundPaths' that contains the transitions that stay within the inner-most
                         // greatest fixed point or get closer to the goal. Only used for strategy extraction
                         BF foundPaths = mgr.constantTrue();

                         // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                         BFFixedPoint nu0(mgr.constantTrue());
                         for (;!nu0.isFixedPointReached();) {

                             // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                             foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                             foundPaths &= safetySys;

                             // Update the inner-most fixed point with the result of applying the enforcable predecessor operator
                             nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput));
                         }

                         // Update the set of positions that are winning for some liveness assumption
                         goodForAnyLivenessAssumption |= nu0.getValue();
                         allFoundPaths |= foundPaths;

                     }
                     mu1.update(goodForAnyLivenessAssumption);
                     distanceStorage[j].push_back(mu1.getValue());
                     overapproximativeWinningStrategy.back().push_back(allFoundPaths);
                 }
                 nextContraintsForGoals &= mu1.getValue();
             }
             nu2.update(nextContraintsForGoals);
         }
         return nu2.getValue();
    }

    /** Check the assumptions */
    void execute() {

        // Prepare reference distances
        std::vector<std::vector<BF> > referenceDistances;
        BF referenceWinningPositions = computeReactiveDistancesAndWinningPositions(referenceDistances);
        if (initEnv.Implies((referenceWinningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput).isTrue()) {
            std::cout << "Starting with a realizable specification (in the standard GR(1) semantics).\n\n";
        } else {
            std::cout << "Starting with an unrealizable specification --> Analyzing the assumptions does not make sense.\n";
            return;
        }

        // Compute the set of reachable positions in any strategy that could be the outcome of a
        // GR(1) synthesis process.
        std::vector<BF> goalStrategies;
        for (unsigned int i=0;i<livenessGuarantees.size();i++) {
            BF casesCovered = mgr.constantFalse();
            BF strategy = mgr.constantFalse();
            for (auto it = overapproximativeWinningStrategy[i].begin();it!=overapproximativeWinningStrategy[i].end();it++) {
                BF newCases = it->ExistAbstract(varCubePostOutput) & !casesCovered;
                strategy |= newCases & *it;
                casesCovered |= newCases;
            }
            goalStrategies.push_back(strategy);
            BF_newDumpDot(*this,strategy,"PreInput PreOutput PostInput PostOutput","/tmp/generalStrategy.dot");
        }

        // Now compute the set of reachable positions
        BF transitionPoints = initEnv & initSys;
        BFFixedPoint mu(initEnv & initSys);
        for (;!mu.isFixedPointReached();) {
            BF all = mu.getValue();
            for (unsigned int i=0;i<livenessGuarantees.size();i++) {
                BFFixedPoint muInner(transitionPoints);
                for (;!muInner.isFixedPointReached();) {
                    BF nextTransitions = muInner.getValue() & goalStrategies[i];
                    transitionPoints |= (nextTransitions & livenessGuarantees[i]).ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);
                    muInner.update(muInner.getValue() | nextTransitions.ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost));
                }
                all |= muInner.getValue();
            }
            mu.update(all);
        }
        BF reachablePositionsInMostGeneralStrategy = mu.getValue();
        BF_newDumpDot(*this,mu.getValue(),NULL,"/tmp/testing.dot");
        BF_newDumpDot(*this,transitionPoints,NULL,"/tmp/tps.dot");



        // Now go through the safety assumptions and compute the reactive distances.
        BF oldSafetyEnv = safetyEnv;
        for (unsigned int i=0;i<safetyEnvParts.size();i++) {
            safetyEnv = mgr.constantTrue();
            for (unsigned int j=0;j<safetyEnvParts.size();j++) {
                if (i!=j) safetyEnv &= safetyEnvParts[j];
            }
            std::cout << "\"" << safetyEnvPartNames[i] << "\" ";

            std::vector<std::vector<BF> > newDistances;
            BF newWinningPositions = computeReactiveDistancesAndWinningPositions(newDistances);

            if (initEnv.Implies((newWinningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput).isTrue()) {
                // Not crucially needed
                if (newWinningPositions < referenceWinningPositions) {
                    if ((newWinningPositions & initEnv) < (referenceWinningPositions & initEnv)) {
                        std::cout << "makes more positions winning, which helps for satisfying the GR(1) robotics semantics.\n";
                    } else {
                        std::cout << "makes more positions winning, but without any effect of the GR(1) robotics semantics.\n";
                    }
                } else {
                    // Redundant?
                    if (oldSafetyEnv==safetyEnv) {
                        std::cout << "is redundant and implied by the other safety assumptions.\n";
                    } else {
                        // Compare reactive distances
                        bool foundANeed = false;
                        for (unsigned int j=0;j<livenessGuarantees.size();j++) {
                            bool neededHere = false;
                            bool neededInMostGeneralStrategy = false;

                            assert(referenceDistances[j].size()<=newDistances[j].size());
                            for (size_t k=0;k<newDistances[j].size();k++) {
                                BF ref = referenceDistances[j][std::min(referenceDistances[j].size()-1,k)];
                                if (ref!=newDistances[j][k]) {
                                    assert(newDistances[j][k]<=ref);
                                    neededHere = true;
                                    if (!(ref & !newDistances[j][k] & reachablePositionsInMostGeneralStrategy).isFalse()) {
                                        neededInMostGeneralStrategy = true;
                                    }
                                }
                            }
                            if (neededHere) {
                                if (!foundANeed) {
                                    std::cout << "makes reaching the system goals ";
                                    foundANeed = true;
                                } else {
                                    std::cout << ", ";
                                }
                                std::cout << j;
                                if (!neededInMostGeneralStrategy) std::cout << " (but not in most general stategy)";
                            }
                        }
                        if (foundANeed) {
                            std::cout << " easier for the system.\n";
                        } else {
                            std::cout << "is superfluous.\n";
                        }
                    }
                }
            } else {
                std::cout << "is crucially needed.\n";
            }
        }
        safetyEnv = oldSafetyEnv;

        // Now go through the liveness assumptions and compute the reactive distances.
        std::vector<BF> oldLivenessAssumptions = livenessAssumptions;
        for (unsigned int i=0;i<oldLivenessAssumptions.size();i++) {

            //if (i==1) throw "Aaargh!";

            safetyEnv = mgr.constantTrue();
            livenessAssumptions.clear();
            for (unsigned int j=0;j<oldLivenessAssumptions.size();j++) {
                if (i!=j) livenessAssumptions.push_back(oldLivenessAssumptions[j]);
            }
            std::cout << "\"" << livenessEnvPartNames.at(i) << "\" ";

            std::vector<std::vector<BF> > newDistances;
            BF newWinningPositions = computeReactiveDistancesAndWinningPositions(newDistances);

            if (initEnv.Implies((newWinningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput).isTrue()) {
                // Not crucially needed
                if (newWinningPositions < referenceWinningPositions) {
                    if ((newWinningPositions & initEnv) < (referenceWinningPositions & initEnv)) {
                        std::cout << "makes more positions winning, which helps for satisfying the GR(1) robotics semantics.\n";
                    } else {
                        std::cout << "makes more positions winning, but without any effect of the GR(1) robotics semantics.\n";
                    }
                } else {

                    // Compare reactive distances
                    bool foundANeed = false;
                    for (unsigned int j=0;j<livenessGuarantees.size();j++) {
                        bool neededHere = false;
                        bool neededInMostGeneralStrategy = false;

                        assert(referenceDistances[j].size()<=newDistances[j].size());
                        for (size_t k=0;k<newDistances[j].size();k++) {
                            BF ref = referenceDistances[j][std::min(referenceDistances[j].size()-1,k)];
                            if (ref!=newDistances[j][k]) {
                                assert(newDistances[j][k]<=ref);
                                neededHere = true;
                                if (!(ref & !newDistances[j][k] & reachablePositionsInMostGeneralStrategy).isFalse()) {
                                    neededInMostGeneralStrategy = true;
                                }
                            }
                        }
                        if (neededHere) {
                            if (!foundANeed) {
                                std::cout << "makes reaching the system goals ";
                                foundANeed = true;
                            } else {
                                std::cout << ", ";
                            }
                            std::cout << j;
                            if (!neededInMostGeneralStrategy) std::cout << " (but not in most general stategy)";
                        }
                    }
                    if (foundANeed) {
                        std::cout << " easier for the system.\n";
                    } else {
                        std::cout << "is superfluous.\n";
                    }

                }
            } else {
                std::cout << "is crucially needed.\n";
            }
        }
    }


public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XAnalyzeAssumptions<T>(filenames);
    }
};


#endif
