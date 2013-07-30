//=====================================================================
// Some general tools
//=====================================================================
#include "gr1context.hpp"

#include <vector>
#include <map>
#include <list>
#include <sstream>
#include <boost/functional/hash.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>

/**
 * @brief A function that takes a BF "in" over the set of variables "var" and returns a new BF over the same variables
 *        that only represents one concrete variable valuation in "in" to the variables in "var"
 * @param in a BF to determinize
 * @param vars the care set of variables
 * @return the determinized BF
 */
BF GR1Context::determinize(BF in, std::vector<BF> vars) {
    for (auto it = vars.begin();it!=vars.end();it++) {
        BF res = in & !(*it);
        if (res.isFalse()) {
            in = in & *it;
        } else {
            in = res;
        }
    }
    return in;
}


/**
 * @brief Performs a semantic Hash of a BF, i.e., a string that summarizes a BF and is independent of the variable order in the BDD manager and the like.
 * @param bdd the BF to be hashed over the variables in the field "variables"
 * @return A string that represents the hash - this includes a HEX string that summarizes the variable information and then a HEX string that summarizes the BF.
 *
 */
std::string GR1Context::bddSemanticHash(BF bdd) {

    // Hash the variableSet
    boost::hash<std::vector<std::string> > variablesHash;
    std::ostringstream os;
    os << std::hex << variablesHash(variableNames) << ',';

    // Do not hash the BDD for special cases
    if (bdd.isFalse()) {
        os << "FALSE";
    } else if (bdd.isTrue()) {
        os << "TRUE";
    } else {

        // General Idea: Compute for every Literal a mask and take the AND with the mask at a branch, and the OR at merging. This approach
        // is invariant of the variable ordering
        unsigned int nof64BitHashElements = variables.size()/8;
        boost::random::mersenne_twister_engine< uint64_t, 64, 624, 397, 31, 0x9908b0df, 11, 0xffffffff, 7, 0x9d2c5680, 15, 0xefc60000, 18, 1812433253> randomNumberGenerator(31337u);
        std::vector<std::pair<uint64_t*,uint64_t*> > literalMasks;
        for (unsigned int i=0;i<variables.size();i++) {
            uint64_t *mask1 = new uint64_t[nof64BitHashElements];
            uint64_t *mask2 = new uint64_t[nof64BitHashElements];
            literalMasks.push_back(std::pair<uint64_t*,uint64_t*>(mask1,mask2));
            for (unsigned int j=0;j<nof64BitHashElements;j++) {
                mask1[j] = 0;
                mask2[j] = 0;
                for (uint k=1;k<=variables.size();k = k << 4) {
                    mask1[j] |= randomNumberGenerator();
                    mask2[j] |= randomNumberGenerator();
                }
            }
        }

        // Constants
        uint64_t *trueMask = new uint64_t[nof64BitHashElements];
        uint64_t *falseMask = new uint64_t[nof64BitHashElements];
        for (unsigned int i=0;i<nof64BitHashElements;i++) {
            trueMask[i] = 0xFFFFFFFFFFFFFFFF;
            falseMask[i] = 0;
        }

        std::map<unsigned int,uint64_t*> cache;
        cache[mgr.constantTrue().getHashCode()] = trueMask;
        cache[mgr.constantFalse().getHashCode()] = falseMask;

        // Compute Variable lookup table
        std::map<unsigned int,std::pair<uint64_t*,uint64_t*> > variableLookup;
        variableLookup[0] = std::pair<uint64_t*,uint64_t*>(trueMask,falseMask);
        for (unsigned int i=0;i<variables.size();i++) {
            variableLookup[variables[i].readNodeIndex()] = literalMasks[i];
        }

        // Recurse!
        bddSemanticHashRecurse(bdd,nof64BitHashElements,variableLookup,cache);

        // Add the value here
        uint64_t *result = cache[bdd.getHashCode()];
        for (unsigned int i=0;i<nof64BitHashElements;i++) {
            os << result[i] << ".";
        }

        // Clean up
        for (auto it = literalMasks.begin();it!=literalMasks.end();it++) {
            delete[] it->first;
            delete[] it->second;
        }
        for (auto it = cache.begin();it!=cache.end();it++) {
            delete it->second;
        }
    }

    // Return the has
    return os.str();
}

void GR1Context::bddSemanticHashRecurse(BF bdd,unsigned int nof64BitHashElements,std::map<unsigned int,std::pair<uint64_t*,uint64_t*> > &variableLookup, std::map<unsigned int,uint64_t*> &cache) {
    if (cache.count(bdd.getHashCode())>0) return;
    std::pair<uint64_t*,uint64_t*> mask = variableLookup.at(bdd.readNodeIndex());
    BF succTrue = bdd.trueSucc();
    BF succFalse = bdd.falseSucc();
    bddSemanticHashRecurse(succTrue,nof64BitHashElements,variableLookup,cache);
    bddSemanticHashRecurse(succFalse,nof64BitHashElements,variableLookup,cache);
    uint64_t* resultTrue = cache[succTrue.getHashCode()];
    uint64_t* resultFalse = cache[succFalse.getHashCode()];
    uint64_t* thisOne = new uint64_t[nof64BitHashElements];
    for (uint i=0;i<nof64BitHashElements;i++) {
        thisOne[i] = (resultTrue[i] & (mask.first)[i]) | (resultFalse[i] & (mask.second)[i]);
        //std::cout << "resultTrue: " << resultTrue[i] << std::endl;
        //std::cout << "resultFalse: " << resultFalse[i] << std::endl;
        //std::cout << "ThisOne Hash" << i << ": " << thisOne[i] << std::endl;
    }
    cache[bdd.getHashCode()] = thisOne;
}
