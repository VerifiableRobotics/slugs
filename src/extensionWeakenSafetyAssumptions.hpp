#ifndef __EXTENSION_WEAKEN_SAFETY_ASSUMPTIONS_HPP
#define __EXTENSION_WEAKEN_SAFETY_ASSUMPTIONS_HPP

#include "gr1context.hpp"
#include <map>

/**
 * This extension modifies the execute() function such that rather than synthesizing an implementation,
 * we search for a minimal CNF formula that represents safety assumptions such that for a realizable input specification,
 * the CNF obtained is a safety assumption set that is as-weak-as-possible, but generalizes the one in the input
 * specification, and still leads to a realizable synthesis problem if we replace the safety assumptions in the
 * input specification by our new CNF.
 */
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
        std::vector<VariableType> variableTypesOfInterest;
        variableTypesOfInterest.push_back(PostInput);
        variableTypesOfInterest.push_back(PreOutput);
        variableTypesOfInterest.push_back(PreInput);
        for (auto it = variableTypesOfInterest.begin();it!=variableTypesOfInterest.end();it++) {
            for (unsigned int i=0;i<variables.size();i++) {
                if (variableTypes[i]==*it) {
                    safetyEnv = currentAssumptions.ExistAbstractSingleVar(variables[i]);
                    checkRealizability();
                    if (realizable) {
                        currentAssumptions = safetyEnv;
                    }
                }
            }
        }

        // Translate to CNF. Sort by size
        std::map<unsigned int,std::vector<std::pair<std::vector<int>,BF> > > clauses; // clauses as cube (-1,0,1) over *all* variables & corresponding BF
        while (!(currentAssumptions.isTrue())) {
            BF thisClause = determinize(!currentAssumptions,variables);

            std::vector<int> thisClauseInt;
            unsigned int elementsInThisClause = 0;
            for (unsigned int i=0;i<variables.size();i++) {
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

        // Remove literals from the clauses whenever they are redundant. They are redundant if the
        // overall assumptions do not change when removing a literal
        std::set<std::vector<int> > clausesFoundSoFarIntAfterFiltering;
        BF clausesSoFar = mgr.constantTrue();
        while (clausesFoundSoFarInt.size()>0) {
            std::vector<int> thisClause = *(clausesFoundSoFarInt.begin());
            clausesFoundSoFarInt.erase(clausesFoundSoFarInt.begin());

            // Compute BF for the rest of the clauses
            BF nextClauses = mgr.constantTrue();
            for (auto it = clausesFoundSoFarInt.begin();it!=clausesFoundSoFarInt.end();it++) {
                BF thisClauseBF = mgr.constantFalse();
                for (unsigned int i=0;i<variables.size();i++) {
                    if (thisClause[i]>0) {
                        thisClauseBF |= variables[i];
                    } else if (thisClause[i]<0) {
                        thisClauseBF |= !variables[i];
                    }
                }
                nextClauses &= thisClauseBF;
            }

            // Go over the literals
            std::vector<int> filteredLiterals;
            BF committedLiterals = mgr.constantFalse();
            for (unsigned int i=0;i<variables.size();i++) {
                BF restOfClause = committedLiterals;
                for (unsigned int j=i+1;j<variables.size();j++) {
                    if (thisClause[j]>0) {
                        restOfClause |= variables[j];
                    } else if (thisClause[j]<0) {
                        restOfClause |= !variables[j];
                    }
                }

                // Check if this literal is needed
                BF literal;
                if (thisClause[i]>0) {
                    literal = variables[i];
                } else {
                    literal = !variables[i];
                }

                if ((clausesSoFar & nextClauses & restOfClause) == (clausesSoFar & nextClauses & (restOfClause | literal))) {
                    filteredLiterals.push_back(0);
                } else {
                    committedLiterals |= literal;
                    filteredLiterals.push_back(thisClause[i]);
                }
            }
            clausesSoFar &= committedLiterals;
            clausesFoundSoFarIntAfterFiltering.insert(filteredLiterals);
        }


        // Print the final set of clauses to stdout
        std::cout << "# Simplified safety assumption CNF clauses:\n";
        for (auto it = clausesFoundSoFarIntAfterFiltering.begin();it!=clausesFoundSoFarIntAfterFiltering.end();it++) {
            for (unsigned int i=0;i<variables.size();i++) {
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
