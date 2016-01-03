#include "BF.h"
#include <unordered_map>
#include <queue>

BFMintermEnumerator::BFMintermEnumerator(BF bf, BF careSet, std::vector<BF> const &_primaryVariables, std::vector<BF> const &_secondaryVariables) :
    primaryVariables(_primaryVariables), secondaryVariables(_secondaryVariables), remainingMinterms(bf) {

    assert(secondaryVariables.size()==primaryVariables.size());

    //! Build the minterm BDD with the secondary variables being "any" bits
    for (unsigned int i=0;i<primaryVariables.size();i++) {
        BF case1 = (!((!remainingMinterms).ExistAbstractSingleVar(primaryVariables[i]))) & secondaryVariables[i];
        BF case2 = remainingMinterms & !secondaryVariables[i];
        remainingMinterms = case1 | case2;
    }

    // Only generate solutions later that are in the care set
    remainingMinterms &= careSet;

    //! What are pre variables, what are post variables?
    for (unsigned int i=0;i<primaryVariables.size();i++) {
        primaryVarIndices.insert(primaryVariables[i].readNodeIndex());
    }
}

bool BFMintermEnumerator::hasNextMinterm() {
    return !(remainingMinterms.isFalse());
}

//==============================================================
// Localized class for BDD traversal for finding minterms
// Uses CUDD functions directly
//==============================================================
struct NodeOrderPair {
    DdNode *node;
    int order;
    inline NodeOrderPair(DdNode *_node, int _order) : node(_node), order(_order) {}
    inline NodeOrderPair(DdManager *mgr, DdNode *_node) : node(_node) {
        order = cuddI(mgr,(unsigned int) Cudd_Regular(node)->index);
        // std::cerr << "Allocated Node Order Pair " << node << " with order " << order << std::endl;
    };
    inline DdNode *getNode() const { return node; }
    inline int getOrder() const { return order; }
    inline operator DdNode*() const { return node; }
};

inline bool operator<(const NodeOrderPair& lhs, const NodeOrderPair& rhs) {
    if (lhs.getOrder() != rhs.getOrder()) return lhs.getOrder() > rhs.getOrder();
    return lhs.getNode() < rhs.getNode();
}

/**
 * @brief Computes the next minterm
 * @param resultStorageSpace This vector is filled with a -1/0/1 representation of the minterm, where -1 is false and 0 is "don't care"
 */
void BFMintermEnumerator::getNextMinterm(std::vector<int> &resultStorageSpace) {

    DdManager *ddMgr = remainingMinterms.manager()->getMgr();
    const BFManager &mgr = *(remainingMinterms.manager());
    DdNode *root = remainingMinterms.getCuddNode();

    //! Find next minterm
    std::unordered_map<DdNode*,std::pair<int,DdNode *> > bestPredecessor;
    std::priority_queue<NodeOrderPair> todo;
    todo.push(NodeOrderPair(ddMgr,root));
    bestPredecessor[root] = std::pair<int,DdNode*>(0,(DdNode*)((size_t)-1));
    while (todo.size()>0) {
        NodeOrderPair current = todo.top();
        assert(bestPredecessor.count(current)>0);
        todo.pop();
        // std::cerr << "Working on variable " << current.getNode() << " with variable index " << Cudd_NodeReadIndex(current.getNode()) << ", cost " << bestPredecessor[current.getNode()].first << " and precessor " << bestPredecessor[current.getNode()].second << "\n";
        // std::cerr << "Actual successors " << Cudd_T(current.getNode()) << " and " << Cudd_E(current.getNode()) << "\n";
        if (Cudd_IsConstant(current.getNode())) {

            //! See if we found a path to the TRUE node
            if (current.getNode() == Cudd_ReadOne(ddMgr)) {

                // Ok, let's compute a cube!
                std::unordered_set<int> cube;
                DdNode *currentSucc = current.getNode();
                std::pair<int,DdNode*> currentPred = bestPredecessor[current.getNode()];
                while (currentPred.second!=(DdNode*)((size_t)-1)) {

                    DdNode *predecessor = currentPred.second;
                    if ((size_t)predecessor & 1) {
                        // Search for a pointer whose complement in the current one
                        DdNode *predecessorRegular = Cudd_Regular(predecessor);
                        if (Cudd_T(predecessorRegular)==Cudd_Not(currentSucc)) {
                            if (primaryVarIndices.count(Cudd_NodeReadIndex(predecessorRegular))>0) {
                                cube.insert(Cudd_NodeReadIndex(predecessorRegular));
                            }
                        } else {
                            assert(Cudd_E(predecessorRegular)==Cudd_Not(currentSucc));
                            cube.insert(-1*Cudd_NodeReadIndex(predecessorRegular)-1);
                        }
                    } else {
                        if (Cudd_T(predecessor)==currentSucc) {
                            if (primaryVarIndices.count(Cudd_NodeReadIndex(predecessor))>0) {
                                cube.insert(Cudd_NodeReadIndex(predecessor));
                            }
                        } else {
                            assert(Cudd_E(predecessor)==currentSucc);
                            cube.insert(-1*Cudd_NodeReadIndex(predecessor)-1);
                        }
                    }

                    // Mode on
                    currentPred = bestPredecessor[predecessor];
                    currentSucc = predecessor;
                }

                // Print-a-cube
                // std::cerr << "Cube:";
                // for (auto it = cube.begin();it!=cube.end();it++) {
                //     std::cerr << " " << *it;
                // }

                BF toBeExcluded = mgr.constantFalse();
                for (unsigned int i=0;i<primaryVariables.size();i++) {
                    if (cube.count(-1-1*secondaryVariables[i].readNodeIndex())>0) {
                        if (cube.count(primaryVariables[i].readNodeIndex())>0) {
                            resultStorageSpace.push_back(1);
                        } else {
                            assert(cube.count(-1-1*primaryVariables[i].readNodeIndex())>0);
                            resultStorageSpace.push_back(-1);
                        }
                        toBeExcluded |= secondaryVariables[i];
                    } else {
                        resultStorageSpace.push_back(0);
                    }
                }

                remainingMinterms &= toBeExcluded;

                // BF_newDumpDot(*this,thisCube,NULL,"/tmp/thiscube.dot");
                assert(todo.size()==0);
            }

        } else {
            int currentCost = bestPredecessor[current].first;
            if (primaryVarIndices.count(Cudd_NodeReadIndex(current.getNode()))>0) {
                //! Is a PRE variable, i.e., represents a literal that we care for
                //! T Direction
                DdNode *post1 = Cudd_T(current.getNode());
                if ((size_t)(current.getNode()) & 1) post1 = (DdNode *)((size_t)post1 ^ 1);
                // std::cerr << "T-Successor of this PRE node is: " << post1 << std::endl;
                if (bestPredecessor.count(post1)==0) {
                    // std::cerr << "Allocating new bestPredecessor (A) Information for " << post1 << std::endl;
                    bestPredecessor[post1] = std::pair<int,DdNode*>(currentCost,current.getNode());
                    todo.push(NodeOrderPair(ddMgr,post1));
                } else {
                    if (currentCost < bestPredecessor[post1].first) {
                        // std::cerr << "Overwriting bestPredecessor (A) Information for " << post1 << std::endl;
                        bestPredecessor[post1] = std::pair<int,DdNode*>(currentCost,current.getNode());
                    }
                }

                //! E Direction
                DdNode *post2 = Cudd_E(current.getNode());
                if ((size_t)(current.getNode()) & 1) post2 = (DdNode *)((size_t)post2 ^ 1);
                // std::cerr << "E-Successor of this PRE node is: " << post2 << std::endl;
                if (bestPredecessor.count(post2)==0) {
                    // std::cerr << "Allocating new bestPredecessor (B) Information for " << post2 << std::endl;
                    bestPredecessor[post2] = std::pair<int,DdNode*>(currentCost,current.getNode());
                    todo.push(NodeOrderPair(ddMgr,post2));
                } else {
                    if (currentCost < bestPredecessor[post2].first) {
                        // std::cerr << "Overwriting bestPredecessor (B) Information for " << post2 << std::endl;
                        bestPredecessor[post2] = std::pair<int,DdNode*>(currentCost,current.getNode());
                    }
                }

            } else {
                //! Is a Post Var, i.e., it represents whether we have
                //! a literal or not

                //! T Direction - Variable value not fixed!
                DdNode *post1 = Cudd_T(current.getNode());
                if ((size_t)(current.getNode()) & 1) post1 = Cudd_Not(post1);
                // std::cerr << "T-Successor of this POST node is: " << post1 << std::endl;
                if (bestPredecessor.count(post1)==0) {
                    // std::cerr << "Allocating new bestPredecessor (C) Information for " << post1 << std::endl;
                    bestPredecessor[post1] = std::pair<int,DdNode*>(currentCost,current.getNode());
                    todo.push(NodeOrderPair(ddMgr,post1));
                } else {
                    if (currentCost < bestPredecessor[post1].first) {
                        // std::cerr << "Overwriting bestPredecessor (C) Information for " << post1 << std::endl;
                        bestPredecessor[post1] = std::pair<int,DdNode*>(currentCost,current.getNode());
                    }
                }

                // E Direction - Variable value fixed
                DdNode *post2 = Cudd_E(current.getNode());
                if ((size_t)(current.getNode()) & 1) post2 = Cudd_Not(post2);
                // std::cerr << "E-Successor of this POST node is: " << post2 << std::endl;
                if (bestPredecessor.count(post2)==0) {
                    // std::cerr << "Allocating new bestPredecessor (D) Information for " << post2 << std::endl;
                    bestPredecessor[post2] = std::pair<int,DdNode*>(currentCost+1,current.getNode());
                    todo.push(NodeOrderPair(ddMgr,post2));
                    // std::cerr << "Just wrote a cost of " << bestPredecessor[post2].first << std::endl;
                    // std::cerr << "...but wanted... " << currentCost+1;
                } else {
                    if (currentCost+1 < bestPredecessor[post2].first) {
                        // std::cerr << "Overwriting bestPredecessor (D) Information for " << post2 << std::endl;
                        bestPredecessor[post2] = std::pair<int,DdNode*>(currentCost+1,current.getNode());
                    }
                }
            }
        }
    }

}

