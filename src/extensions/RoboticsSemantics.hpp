#ifndef __EXTENSION_ROBOTICS_SEMANTICS_HPP
#define __EXTENSION_ROBOTICS_SEMANTICS_HPP

#include "gr1context.hpp"

/**
 * A specification should only be called realizable if
 * for every initial state that is admissable by the environment initialization assumptions, we can satisfy the specification from
 * there. In classical GR(1) synthesis, it is only required that for every input variable valuation, there is one output
 * variable valuation that is is allowed.
 */
template<class T> class XRoboticsSemantics : public T {
protected:
    // Inherit stuff that we actually use here.
    using T::computeWinningPositions;
    using T::initEnv;
    using T::initSys;
    using T::winningPositions;
    using T::varCubePreOutput;
    using T::varCubePreInput;
    using T::realizable;

    XRoboticsSemantics<T>(std::list<std::string> &filenames) : T(filenames) {}
public:

    void checkRealizability() {
        computeWinningPositions();

        // Check if for every possible environment initial position the system has a good system initial position
        BF result;
        result = (initEnv & initSys).Implies(winningPositions).UnivAbstract(varCubePreOutput).UnivAbstract(varCubePreInput);

        // Check if the result is well-defind. Might fail after an incorrect modification of the above algorithm
        if (!result.isConstant()) throw "Internal error: Could not establish realizability/unrealizability of the specification.";

        // Return the result in Boolean form.
        realizable = result.isTrue();
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XRoboticsSemantics<T>(filenames);
    }
};

#endif
