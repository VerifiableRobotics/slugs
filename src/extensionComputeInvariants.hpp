#ifndef __EXTENSION_COMPUTE_INVARIANTS_HPP
#define __EXTENSION_COMPUTE_INVARIANTS_HPP

#include "gr1context.hpp"
#include <string>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <functional>
#include "subprocess.hpp"

#ifdef CUDD
#include <cuddInt.h>
#endif

/**
 * A class for computing invariants implied by the maximally permissive strategy (and probably later a bit more)
 */
template<class T> class XComputeInvariants : public T {
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
    using T::computeWinningPositions;
    using T::winningPositions;

    std::string lpSolver;

    XComputeInvariants<T>(std::list<std::string> &filenames) : T(filenames) {
    }

    void init(std::list<std::string> &filenames) {
        T::init(filenames);

        if (filenames.size()==0) {
            throw SlugsException(false,"Error: Need file name of linear programming solver to analyze invariants.");
        }
        lpSolver = filenames.front();
        filenames.pop_front();
        std::cerr << "Using LP Solver: " << lpSolver << std::endl;

    }

    /** Execute the analysis */
    void execute() {

#ifdef USE_CUDD

        // 1. Perform realizability checking
        computeWinningPositions();
#ifndef NDEBUG
        BF_newDumpDot(*this,winningPositions,NULL,"/tmp/winningpositions.dot");
#endif

        // Compute which states need to be covered
        BF statesReachableByTransitions = (safetyEnv & safetySys).ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);
        statesReachableByTransitions |= (initEnv & initSys);
        BF toBeCovered = statesReachableByTransitions & !winningPositions;
#ifndef NDEBUG
        BF_newDumpDot(*this,toBeCovered,NULL,"/tmp/tobecovered.dot");
#endif

        // Prepare encoding -- Get Support
        std::vector<unsigned int> relevantCUDDVars;
        std::vector<std::string> relevantCUDDVarNames;
        std::vector<unsigned int> cuddIndexToRelevantVarNumberMapper;

        // Destroy intemediate data structures eagerly.
        {

            std::set<int> varsInSupport;
            std::set<DdNode *> done;

            std::function<void(DdNode*)> getSupportRecurse;

            getSupportRecurse = [&varsInSupport,&done,&getSupportRecurse](DdNode *current) {
                if (Cudd_IsConstant(current)) return;
                current = Cudd_Regular(current);
                if (done.count(current)>0) return;
                done.insert(current);
                varsInSupport.insert(current->index);
                getSupportRecurse(Cudd_T(current));
                getSupportRecurse(Cudd_E(current));
            };
            getSupportRecurse(winningPositions.getCuddNode());
            getSupportRecurse(toBeCovered.getCuddNode());
        }


        // Encode!
        subprocess::popen cmd(lpSolver, {});
        cmd.close(); // Closes only inStream

        std::ostringstream buffer;
        buffer << cmd.stdout().rdbuf();

        int retVal = cmd.wait();
        if (retVal!=0) {
            std::ostringstream error;
            error << cmd.stderr().rdbuf();
            throw SlugsException(false,error.str());
        }

        std::cerr << "Encoding finished: " << buffer.str() << std::endl;





    /*
    max: 143 x + 60 y;

        120 x + 210 y <= 15000;
        110 x + 30 y <= 4000;
        x + y <= 75;
    */

#else
        std::cerr << "This plugin is only supported for a CUDD backend.\n";
#endif


    }




public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XComputeInvariants<T>(filenames);
    }
};


#endif
