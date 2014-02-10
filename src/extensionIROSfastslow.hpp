#ifndef __EXTENSION_IROSFS_HPP
#define __EXTENSION_IROSFS_HPP

#include "gr1context.hpp"
#include <string>
#include "boost/tuple/tuple.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>

/**
 * A class that computes a counterstrategy for an unrealizable specification
 *
 * Includes support for the special robotics semantic
 */


//typedef enum { PreInput, PreOutputF, PreOutputS, PostInput, PostOutputF, PostOutputS } VariableTypeFS;

template<class T> class XIROSFS : public T {
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
    using T:: preVars;
    using T:: postVars;
    using T:: postInputVars;
    using T:: varCubePre;
    using T:: varCubePost;
    using T:: varCubePostOutput;
    using T:: varCubePreOutput;
    using T:: varCubePreInput;
    using T:: varCubePostInput;
    
    
    std::vector<std::pair<unsigned int,BF> > strategyDumpingData;
    BFVarCube varCubePostOutputF;
    BFVarCube varCubePostOutputS;
    BFVarCube varCubePreOutputF;
    BFVarCube varCubePreOutputS;
    std::vector<BF> postOutputFVars;
    std::vector<BF> postOutputSVars;
    std::vector<BF> preOutputFVars;
    std::vector<BF> preOutputSVars;
    std::vector<BF> preInputVars;
    BFBddVarVector varVectorPostOutputS;;
    BFBddVarVector varVectorPreOutputS;
    
    
    // Constructor
    XIROSFS<T>(std::list<std::string> &filenames) : T(filenames) {}
    /**
 * @brief Constructor that reads the problem instance from file and prepares the BFManager, the BFVarCubes, and the BFVarVectors
 * @param inFile the input filename
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
    lineNumberCurrentlyRead = 0;
    while (std::getline(inFile,currentLine)) {
        lineNumberCurrentlyRead++;
        boost::trim(currentLine);
        if ((currentLine.length()>0) && (currentLine[0]!='#')) {
            if (currentLine[0]=='[') {
                if (currentLine=="[INPUT]") {
                    readMode = 0;
                } else if (currentLine=="[OUTPUT_F]") {
                    readMode = 1;
                } else if (currentLine=="[OUTPUT_S]") {
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
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine);
                    variableTypes.push_back(PreInput);
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine+"'");
                    variableTypes.push_back(PostInput);
                } else if (readMode==1) {
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine);
                    variableTypes.push_back(PreOutputF);
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine+"'");
                    variableTypes.push_back(PostOutputF);
                } else if (readMode==8) {
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine);
                    variableTypes.push_back(PreOutputS);
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine+"'");
                    variableTypes.push_back(PostOutputS);
                } else if (readMode==2) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    initEnv &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==3) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutputF);
                    allowedTypes.insert(PreOutputS);
                    initSys &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==4) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutputF);
                    allowedTypes.insert(PreOutputS);
                    allowedTypes.insert(PostInput);
                    safetyEnv &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==5) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutputF);
                    allowedTypes.insert(PreOutputS);
                    allowedTypes.insert(PostInput);
                    allowedTypes.insert(PostOutputF);
                    allowedTypes.insert(PostOutputS);
                    safetySys &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==6) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutputF);
                    allowedTypes.insert(PreOutputS);
                    allowedTypes.insert(PostOutputF);
                    allowedTypes.insert(PostOutputS);
                    allowedTypes.insert(PostInput);
                    livenessAssumptions.push_back(parseBooleanFormula(currentLine,allowedTypes));
                } else if (readMode==7) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutputF);
                    allowedTypes.insert(PreOutputS);
                    allowedTypes.insert(PostInput);
                    allowedTypes.insert(PostOutputF);
                    allowedTypes.insert(PostOutputS);
                    livenessGuarantees.push_back(parseBooleanFormula(currentLine,allowedTypes));
                } else {
                    std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                    throw "Found a line in the specification file that has no proper categorial context.";
                }
            }
        }
    }

    // Compute VarVectors and VarCubes
    std::vector<BF> preOutputFVars;
    std::vector<BF> preOutpuSVars;
    std::vector<BF> preOutputVars;  
    std::vector<BF> postOutputVars;
    std::vector<BF> preInputVars;
    for (unsigned int i=0;i<variables.size();i++) {
        switch (variableTypes[i]) {
        case PreInput:
            preVars.push_back(variables[i]);
            preInputVars.push_back(variables[i]);
            break;
        case PreOutputF:
            preVars.push_back(variables[i]);
            preOutputFVars.push_back(variables[i]);
            break;
        case PreOutputS:
            preVars.push_back(variables[i]);
            preOutputSVars.push_back(variables[i]);
            break;
        case PostInput:
            postVars.push_back(variables[i]);
            postInputVars.push_back(variables[i]);
            break;
        case PostOutputF:
            postVars.push_back(variables[i]);
            postOutputFVars.push_back(variables[i]);
            break;
        case PostOutputS:
            postVars.push_back(variables[i]);
            postOutputSVars.push_back(variables[i]);
            break;
        default:
            throw "Error: Found a variable of unknown type";
        }
    }
    varVectorPre = mgr.computeVarVector(preVars);
    varVectorPost = mgr.computeVarVector(postVars);
    varVectorPostOutputS = mgr.computeVarVector(postOutputSVars);
    varVectorPreOutputS = mgr.computeVarVector(preOutputSVars);
    
    varCubePostInput = mgr.computeCube(postInputVars);
    varCubePreInput = mgr.computeCube(preInputVars);

    varCubePostOutputF = mgr.computeCube(postOutputFVars);
    varCubePostOutputS = mgr.computeCube(postOutputSVars);    
    postOutputVars.insert(postOutputVars.end(),postOutputFVars.begin(),postOutputFVars.end());
    postOutputVars.insert(postOutputVars.end(),postOutputSVars.begin(),postOutputSVars.end());
    varCubePostOutput = mgr.computeCube(preOutputVars);
     
    varCubePreOutputF = mgr.computeCube(preOutputFVars);
    varCubePreOutputS = mgr.computeCube(preOutputSVars);
    preOutputVars.insert(preOutputVars.end(),preOutputFVars.begin(),preOutputFVars.end());
    preOutputVars.insert(preOutputVars.end(),preOutputSVars.begin(),preOutputSVars.end());
    varCubePreOutput = mgr.computeCube(preOutputVars);
    
    varCubePre = mgr.computeCube(preVars);
    varCubePost = mgr.computeCube(postVars);
    

    // Check if variable names have been used twice
    std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
    if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

    // Make sure that there is at least one liveness assumption and one liveness guarantee
    // The synthesis algorithm might be unsound otherwise
    if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
    if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
}

    
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
    
    BF safeStates = safetySys.ExistAbstract(varCubePostInput).ExistAbstract(varCubePostOutputS).ExistAbstract(varCubePostOutputF);
    BF safeNext = (safetySys.ExistAbstract(varCubePreInput).ExistAbstract(varCubePreOutputS).ExistAbstract(varCubePreOutputF)).SwapVariables(varVectorPost,varVectorPre);
    
    BF newSafetySys = safetySys & (safeStates & safeNext).SwapVariables(varVectorPre,varVectorPost).SwapVariables(varVectorPostOutputS,varVectorPreOutputS);
    
    //BF sameVarS = mgr.constantTrue();
    //BF sameVarF = mgr.constantTrue();
    
    
    //for (unsigned int i1=0;i1<postOutputSVars.size();i1++)
    //{
    //    sameVarS = sameVarS & postOutputSVars[i1].Implies(preOutputSVars[i1]) & !postOutputSVars[i1].Implies(!preOutputSVars[i1]);
    //}
    //for (unsigned int i2=0;i2<postOutputFVars.size();i2++)
    //{
    //    sameVarF = sameVarF & postOutputFVars[i2].Implies(preOutputFVars[i2]) & !postOutputFVars[i2].Implies(!preOutputFVars[i2]);
    //}
                        
    //BF newSafetySys = safetySys & (sameVarS.Implies((safeStates & safeNext).SwapVariables(varVectorPre,varVectorPost))).UnivAbstract(varCubePostOutputF);       

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

                    // Inner-most greatest fixed point. The corresponding variable in the paper would be 'X'.
                    BFFixedPoint nu0(mgr.constantTrue());
                    for (;!nu0.isFixedPointReached();) {

                        // Compute a set of paths that are safe to take - used for the enforceable predecessor operator ('cox')
                        foundPaths = livetransitions | (nu0.getValue().SwapVariables(varVectorPre,varVectorPost) & !(livenessAssumptions[i]));
                        foundPaths &= newSafetySys;

                        // Update the inner-most fixed point with the result of applying the enforcable *FS* predecessor operator
                         
                        /*
                        //Read the new enforceable predecessor operator starting at the update statement***
                        
                        //And there is a transition to the "to" (foundPaths) set, i.e. a slow move as well as the (pre-determined) fast move
                        BF exy1 = (foundPaths).SwapVariables(varVectorPre,varVectorPost) & safetySys.ExistAbstract(varCubePostOutputF);        
                        
                        
                        //Such that the slowSame move is safe...
                        BF exy2 = (sameVarS.Implies((safeStates & safeNext).SwapVariables(varVectorPre,varVectorPost))).UnivAbstract(varCubePostOutputF);        
                        
                        
                        //If both types are changing, check the intermediate state (i.e. the "sameVarS" state):
                        
                        //There exists a "fast" move...
                        BF fs1 = (exy1 & exy2).ExistAbstract(varCubePostOutputF);
                        
                        
                        //If only one type of controller changes, use old enforceable predecessor operator
                        BF fs2 = (foundPaths).SwapVariables(varVectorPre,varVectorPost) & (safetySys & (sameVarS)).ExistAbstract(varCubePostOutputF).ExistAbstract(varCubePostOutputF);
        
                        
                        ////For all environment moves, there is either a move that changes only slow or only fast, 
                        //or one that changes both actions but has a safe intermediate state
                        nu0.update(safetyEnv.Implies(fs1 | (fs2)).UnivAbstract(varCubePostInput));
                        */
                        
                        nu0.update(safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutputF).ExistAbstract(varCubePostOutputS).UnivAbstract(varCubePostInput));
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
}


void checkRealizability() {

    computeWinningPositions();

    // Check if for every possible environment initial position the system has a good system initial position
    BF result;
    result = initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreOutputF).ExistAbstract(varCubePreOutputS)).UnivAbstract(varCubePreInput);

    // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
    if (!result.isConstant()) {
        SlugsException e(false);
        e << "Internal error: Could not establish realizability/unrealizability of the specification.";
        throw e;
    }

    // Return the result in Boolean form.
    realizable = result.isTrue();
}


void getVariableTypes(std::vector<std::string> &types) const {
        types.push_back("PreInput");
        types.push_back("PreOutputF");
        types.push_back("PreOutputS");
        types.push_back("PostInput");
        types.push_back("PostOutputF");
        types.push_back("PostOutputS");
    }

void getVariableNumbersOfType(std::string typeString, std::vector<unsigned int> &nums) const {
        VariableType type;
        if (typeString=="PreInput") type = PreInput;
        else if (typeString=="PreOutputF") type = PreOutputF;
        else if (typeString=="PreOutputS") type = PreOutputS;
        else if (typeString=="PostInput") type = PostInput;
        else if (typeString=="PostOutputF") type = PostOutputF;
        else if (typeString=="PostOutputS") type = PostOutputS;
        else throw "Cannot detect variable type for BDD dumping";
        for (unsigned int i=0;i<variables.size();i++) {
            if (variableTypes[i] == type) nums.push_back(i);
        }
    }
    
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XIROSFS(filenames);
    }
};

#endif

