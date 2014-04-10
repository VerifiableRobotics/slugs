#ifndef __EXTENSION_ANALYZE_SAFETY_LIVENESS_INTERACTION_HPP
#define __EXTENSION_ANALYZE_SAFETY_LIVENESS_INTERACTION_HPP

#include "gr1context.hpp"

/**
 * A class that checks how assumptions and guarantees interplay.
 */
template<class T> class XAnalyzeSafetyLivenessInteraction : public T {
protected:

    using T::safetyEnv;
    using T::safetySys;
    using T::mgr;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::computeWinningPositions;
    using T::initEnv;
    using T::initSys;
    using T::winningPositions;
    using T::varCubePreOutput;
    using T::varCubePreInput;
    using T::addVariable;
    using T::variables;
    using T::computeVariableInformation;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostInput;
    using T::varCubePostOutput;

    // Constructor
    XAnalyzeSafetyLivenessInteraction<T>(std::list<std::string> &filenames) : T(filenames) {}


    void execute() {

        std::cout << "Realizability matrix when activating/deactivating some specification parts:\n\n";
        std::cout << "+-------------------+-------------------++------------------------------------------+\n";
        std::cout << "| Assumptions       | Guarantees        || Results                                  |\n";
        std::cout << "+--------+----------+--------+----------++---------------------+--------------------+\n";
        std::cout << "| Safety | Liveness | Safety | Liveness || Classical semantics | Robotics semantics |\n";
        std::cout << "+--------+----------+--------+----------++---------------------+--------------------+\n";

        std::vector<bool> resultsClassicalSemantics;
        std::vector<bool> resultsRoboticsSemantics;

        BF overallSafetyEnv = safetyEnv;
        BF overallSafetySys = safetySys;
        std::vector<BF> overallLivenessAssumptions = livenessAssumptions;
        std::vector<BF> overallLivenessGuarantees = livenessGuarantees;


        for (unsigned int i=0;i<16;i++) {
            bool hasSafetyAssumptions = (i & 8)>0;
            bool hasLivenessAssumptions = (i & 4)>0;
            bool hasSafetyGuarantees = (i & 2)>0;
            bool hasLivenessGuarantees = (i & 1)>0;
            if (hasSafetyAssumptions) {
                safetyEnv = overallSafetyEnv;
            } else {
                safetyEnv = mgr.constantTrue();
            }
            if (hasSafetyGuarantees) {
                safetySys = overallSafetySys;
            } else {
                safetySys = mgr.constantTrue();
            }
            if (hasLivenessAssumptions) {
                livenessAssumptions = overallLivenessAssumptions;
            } else {
                livenessAssumptions.clear();
                livenessAssumptions.push_back(mgr.constantTrue());
            }
            if (hasLivenessGuarantees) {
                livenessGuarantees = overallLivenessGuarantees;
            } else {
                livenessGuarantees.clear();
                livenessGuarantees.push_back(mgr.constantTrue());
            }

            computeWinningPositions();

            bool isWinningTraditionalSemantics = initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput).isTrue();
            bool isWinningRoboticsSemantics = (initEnv & initSys).Implies(winningPositions).UnivAbstract(varCubePreOutput).UnivAbstract(varCubePreInput).isTrue();

            resultsClassicalSemantics.push_back(isWinningTraditionalSemantics);
            resultsRoboticsSemantics.push_back(isWinningRoboticsSemantics);

            // Print a line
            std::cout << "| " << (hasSafetyAssumptions?"   +   ":"   -   ");
            std::cout << "| " << (hasLivenessAssumptions?"    +    ":"    -    ");
            std::cout << "| " << (hasSafetyGuarantees?"   +   ":"   -   ");
            std::cout << "| " << (hasLivenessGuarantees?"    +    ":"    -    ");
            std::cout << "|| " << (isWinningTraditionalSemantics?"         yes        ":"         no         ");
            std::cout << "| " << (isWinningRoboticsSemantics?"        yes        ":"        no         ");
            std::cout << "|\n";

        }
        std::cout << "+--------+----------+--------+----------++---------------------+--------------------+\n\n";


        // Does Non-strict implication semantics make a difference?
        if (!resultsClassicalSemantics[15] || !resultsRoboticsSemantics[15]) {

            // Add an extra guarantees-already-violated variable
            unsigned int varGuaFailPre = addVariable(PreOutput,"safetyGuaranteesFailed");
            unsigned int varGuaFailPost = addVariable(PostOutput,"safetyGuaranteesFailed");

            computeVariableInformation();

            // Construct new spec
            safetyEnv = overallSafetyEnv;
            safetySys = (!variables[varGuaFailPost]) ^ ((!overallSafetySys) | variables[varGuaFailPre]);
            livenessAssumptions = overallLivenessAssumptions;
            livenessGuarantees = overallLivenessGuarantees;
            livenessGuarantees.push_back(!variables[varGuaFailPost]);

            computeWinningPositions();

            bool isWinningTraditionalSemantics = initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput).isTrue();
            bool isWinningRoboticsSemantics = (initEnv & initSys).Implies(winningPositions).UnivAbstract(varCubePreOutput).UnivAbstract(varCubePreInput).isTrue();


            std::cout << "With a non-strict implication,\n";
            if (isWinningTraditionalSemantics & !resultsClassicalSemantics[15]) {
                std::cout << "- the specification becomes realizable in the non-robotics semantics,\n";
            } else {
                std::cout << "- nothing changes in the non-robotics semantics,\n";
            }
            if (isWinningRoboticsSemantics & !resultsRoboticsSemantics[15]) {
                std::cout << "- and the specification becomes realizable in the robotics semantics.\n\n";
            } else {
                std::cout << "- and nothing changes in the robotics semantics, either.\n\n";
            }
        }

        // Revert to old specification (without the extra bit for taking care of the strict implication)
        safetySys = overallSafetySys;

        // How long does it take to lose/win if the safety-only spec can be won by driving the other player
        // into a deadlock?
        if (!resultsClassicalSemantics[10]) {

            // Losing for the system - the environment can try to falsify now.
            winningPositions = mgr.constantFalse();
            unsigned int round = 0;
            int roundFoundInitPos = -1;
            BF oldWinningPositions = !winningPositions;
            while ((winningPositions!=oldWinningPositions) && (roundFoundInitPos==-1)) {
                oldWinningPositions = winningPositions;
                winningPositions |= (safetyEnv & (winningPositions.SwapVariables(varVectorPre,varVectorPost) | !safetySys).UnivAbstract(varCubePostOutput)).ExistAbstract(varCubePostInput);
                round++;
                if ((roundFoundInitPos==-1) &&
                        ((!initSys) | (initEnv & winningPositions)).UnivAbstract(varCubePreOutput).ExistAbstract(varCubePreInput).isTrue()) {
                    roundFoundInitPos = round;
                }
            }

            if (roundFoundInitPos>-1) {
                std::cout << "Note that with only the safety assumptions and guarantees, the environment can falsify the system's specification within " << roundFoundInitPos << " steps.\n";
                if ((initEnv & initSys & winningPositions).isFalse()) {
                    std::cout << "However, there is no corresponding strategy that starts from a position that satisfies all initialization constraints\n";
                }
            } else {
                std::cout << "Note that with only the safety assumptions and guarantees, the environment can't falsify the system's specification.\n";
            }
        }

        // How long does it take to win if the safety-only spec can be won by driving the other player into a deadlock?
        if (resultsClassicalSemantics[10]) {

            // Winning for the system - the System can try to falsify now.
            winningPositions = mgr.constantFalse();
            unsigned int round = 0;
            int roundFoundInitPos = -1;
            BF oldWinningPositions = !winningPositions;
            while (winningPositions!=oldWinningPositions) {
                oldWinningPositions = winningPositions;
                winningPositions |= (((safetySys & winningPositions.SwapVariables(varVectorPre,varVectorPost)) | (!safetyEnv))).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput);
                round++;
                if ((roundFoundInitPos==-1) &&
                        initEnv.Implies((winningPositions & initSys).ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput).isTrue()) {
                    roundFoundInitPos = round;
                }
            }

            if (roundFoundInitPos>-1) {
                std::cout << "Note that with only the safety assumptions and guarantees, the system can falsify the environment's specification within " << roundFoundInitPos << " steps.\n";
            } else {
                std::cout << "Note that with only the safety assumptions and guarantees, the system can't falsify the environment's specification.\n";
            }
        }



    }


public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XAnalyzeSafetyLivenessInteraction<T>(filenames);
    }


};




#endif
