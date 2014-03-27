//=====================================================================
// Some general tools
//=====================================================================
#include "gr1context.hpp"

#include <vector>
#include <map>
#include <list>
#include <sstream>
#include <cstdlib>

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
 * @brief A function that takes a BF "in" over the set of variables "var" and returns a new BF over the same variables
 *        that only represents one concrete variable valuation in "in" to the variables in "var"
 *        This version tries to randomize everything!
 * @param in a BF to determinize
 * @param vars the care set of variables
 * @return the determinized BF
 */
BF GR1Context::determinizeRandomized(BF in, std::vector<BF> vars) {
    for (auto it = vars.begin();it!=vars.end();it++) {
        if (rand()%2) {
            BF res = in & !(*it);
            if (res.isFalse()) {
                in = in & *it;
            } else {
                in = res;
            }
        } else {
            BF res = in & (*it);
            if (res.isFalse()) {
                in = in & !(*it);
            } else {
                in = res;
            }
        }
    }
    return in;
}


