#ifndef __EXTENSION_WEAKEN_SAFETY_ASSUMPTIONS_HPP
#define __EXTENSION_WEAKEN_SAFETY_ASSUMPTIONS_HPP

#include "gr1context.hpp"
#include <map>

template<class T> class XComputeWeakenedSafetyAssumptions : public T {
protected:
    // Inherit stuff that we actually use here.
    using T::realizable;
    using T::variables;
    using T::variableTypes;
    using T::safetyEnv;
    using T::checkRealizability;
    using T::determinize;
    using T::mgr;
    using T::variableNames;

    XComputeWeakenedSafetyAssumptions<T>(std::list<std::string> &filenames) : T(filenames) {}
public:

    void execute() {
        // Try to make the safety assumptions deal with as few variables as possible.

        BF currentAssumptions = safetyEnv;
        for (VariableType t : {PostInput, PreOutput, PreInput}) {
            for (uint i=0;i<variables.size();i++) {
                if (variableTypes[i]==t) {
                    safetyEnv = currentAssumptions.ExistAbstractSingleVar(variables[i]);
                    checkRealizability();
                    if (realizable) {
                        currentAssumptions = safetyEnv;
                    }
                }
            }
        }

        // Translate to CNF. Sort by size
        std::map<uint,std::vector<std::pair<std::vector<int>,BF> > > clauses; // clauses as cube (-1,0,1) over *all* variables & corresponding BF
        while (!(currentAssumptions.isTrue())) {
            BF thisClause = determinize(!currentAssumptions,variables);

            std::vector<int> thisClauseInt;
            uint elementsInThisClause = 0;
            for (uint i=0;i<variables.size();i++) {
                BF thisTry = thisClause.ExistAbstractSingleVar(variables[i]);
                if ((thisTry & currentAssumptions).isFalse()) {
                    thisClauseInt.push_back(0);
                    thisClause = thisTry;
                } else if ((thisClause & variables[i]).isFalse()) {
                    thisClauseInt.push_back(1);
                    elementsInThisClause++;
                } else {
                    thisClauseInt.push_back(-1);
                    elementsInThisClause++;
                }
            }
            clauses[elementsInThisClause].push_back(std::pair<std::vector<int>,BF>(thisClauseInt,!thisClause));
            currentAssumptions |= thisClause;
        }

        // Remove as many clauses from the list as possible, until the specification becomes unrealizable
        BF clausesFoundSoFar = mgr.constantTrue();
        std::set<std::vector<int> > clausesFoundSoFarInt;

        // Start with the longest clauses
        for (auto it = clauses.rbegin();it!=clauses.rend();it++) {
            for (auto it2 = it->second.begin();it2!=it->second.end();it2++) {

                // First: Compute the impact of the rest of the clauses
                BF restOfTheClauses = mgr.constantTrue();
                for (auto it3 = it2+1;it3!=it->second.end();it3++) {
                    restOfTheClauses &= it3->second;
                }
                auto it3 = it;
                it3++;
                for (;it3!=clauses.rend();it3++) {
                    for (auto it4 = it3->second.begin();it4!=it3->second.end();it4++) {
                        restOfTheClauses &= it4->second;
                    }
                }

                // Then see if we achieve realizability only with the clauses that we previously found to be essential and
                // the remaining clauses
                safetyEnv = restOfTheClauses & clausesFoundSoFar;
                checkRealizability();
                if (realizable) {
                    it2->second = mgr.constantTrue();
                } else {
                    clausesFoundSoFar &= it2->second;
                    clausesFoundSoFarInt.insert(it2->first);
                }
            }
        }

        // Print the final set of clauses to stdout
        std::cout << "# Simplified safety assumption CNF clauses:\n";
        for (auto it = clausesFoundSoFarInt.begin();it!=clausesFoundSoFarInt.end();it++) {
            for (uint i=0;i<variables.size();i++) {
                if ((*it)[i]!=0) {
                    if ((*it)[i]<0) {
                        std::cout << "!";
                    }
                    std::cout << variableNames[i] << " ";
                }
            }
            std::cout << "\n";
        }
        std::cout << "# End of list\n";
#ifndef NDEBUG
        safetyEnv = clausesFoundSoFar;
        checkRealizability();
        if (!realizable) throw "Internal Error: The specification after simplification should be realizable.";
#endif
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XComputeWeakenedSafetyAssumptions<T>(filenames);
    }
};

#endif
