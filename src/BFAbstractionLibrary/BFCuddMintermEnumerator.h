#ifndef __BF_CUDD_MINTERM_ENUMERTOR__
#define __BF_CUDD_MINTERM_ENUMERTOR__

#ifndef USE_CUDD
#error This header file should be included indirectly by BF.h
#endif

#include <tr1/unordered_set>

class BFMintermEnumerator {
private:
    std::vector<BF> const &primaryVariables;
    std::vector<BF> const &secondaryVariables;
    BF remainingMinterms;
    std::tr1::unordered_set<int> primaryVarIndices;
public:
    BFMintermEnumerator(BF bf, std::vector<BF> const &_primaryVariables, std::vector<BF> const &_secondaryVariables);
    bool hasNextMinterm();
    void getNextMinterm(std::vector<int> &resultStorageSpace);
};

#endif
