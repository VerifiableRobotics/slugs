#ifndef __EXTENSION_OPTIMISTIC_RECOVERY_HPP
#define __EXTENSION_OPTIMISTIC_RECOVERY_HPP

#include "gr1context.hpp"

/**
  "Optimistic recovery" plugin. It lets the generated implementation also have transitions to non-winning states for
  next inputs that only allow such transitions. However, such transitions are only added if in the resulting strategy,
  there is some path to the winning states left along with the system guarantees are not violated.
 */

template<class T> class XOptimisticRecovery : public T {

protected:

    using T::computeWinningPositions;
    using T::winningPositions;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::safetySys;
    using T::varCubePost;
    using T::strategyDumpingData;
    using T::livenessGuarantees;

    // Constructor
    XOptimisticRecovery<T>(std::list<std::string> &filenames) : T(filenames) {}

public:
    void computeWinningPositions() {

        // First do the regular fixpoint computation
        T::computeWinningPositions();

        // Now add additional recovery transition...
        BFFixedPoint mu(winningPositions);
        while (!mu.isFixedPointReached()) {
            BF additionalTransitions = mu.getValue().SwapVariables(varVectorPre,varVectorPost) ;
            additionalTransitions &= safetySys;
            BF additionalStates = additionalTransitions.ExistAbstract(varCubePost);
            mu.update(additionalStates);

            // Add recovery transitions for every liveness guarantee
            for (unsigned int j=0;j<livenessGuarantees.size();j++) {
                strategyDumpingData.push_back(std::pair<unsigned int,BF>(j,additionalTransitions));
            }
        }

    }


    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XOptimisticRecovery<T>(filenames);
    }

};

#endif
