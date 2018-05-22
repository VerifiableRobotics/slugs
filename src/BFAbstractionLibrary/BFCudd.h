/*
 * BFCudd.h
 *
 *  Created on: 06.08.2010
 *      Author: ehlers
 */

#ifndef BFCUDD_H_
#define BFCUDD_H_

#include "BFCuddManager.h"

class BFBddManager;
class BFBddVarCube;
class BFBddVarVector;

class BFBdd {
private:
	const BFBddManager *bfmanager;
	DdManager *mgr;
	DdNode *node;

	inline BFBdd(const BFBddManager *_manager, DdNode *_node) :
		bfmanager(_manager), mgr(_manager->getMgr()), node(_node) {
		Cudd_Ref(node);
	}

public:
	inline BFBdd() :
		bfmanager(0), mgr(0), node(0) {
	}

	inline ~BFBdd() {
		if (node != 0)
			Cudd_RecursiveDeref(mgr, node);
	}

	inline BFBdd(const BFBdd &bdd) :
		bfmanager(bdd.bfmanager), mgr(bdd.mgr), node(bdd.node) {
		if (node != 0)
			Cudd_Ref(node);
	}

    inline BFBdd(BFBdd &&bdd) : bfmanager(bdd.bfmanager), mgr(bdd.mgr), node(bdd.node) {
        bdd.node = 0;
    }

    inline BFBdd& operator=(BFBdd &&other) {
        if (this == &other)
            return *this;

        if ((mgr != 0) && (node != 0)) {
            Cudd_RecursiveDeref(mgr, node);
        }

        bfmanager = other.bfmanager;
        mgr = other.mgr;
        node = other.node;
        other.node = 0;

        return *this;
    }

	inline bool isFalse() const {
		return node == (Cudd_Not(Cudd_ReadOne(mgr)));
	}

	inline bool isTrue() const {
		return node == Cudd_ReadOne(mgr);
	}

	inline double getNofMinterms() {
		return Cudd_CountPathsToNonZero(node);
	}
	double getNofSatisfyingAssignments(const BFBddVarCube &cube) const;
	inline bool isValid() const {
		return node != 0;
	}

	inline BFBdd operator&=(const BFBdd& other) {
		DdNode *result = Cudd_bddAnd(mgr, node, other.node);
		Cudd_Ref(result);
		Cudd_RecursiveDeref(mgr, node);
		node = result;
		return *this;
	}

	inline BFBdd operator|=(const BFBdd& other) {
		DdNode *result = Cudd_bddOr(mgr, node, other.node);
		Cudd_Ref(result);
		Cudd_RecursiveDeref(mgr, node);
		node = result;
		return *this;
	}

	inline int operator==(const BFBdd& other) const {
		return (node == other.node);
	}

	inline BFBdd& operator=(const BFBdd &other) {
		if (this == &other)
			return *this;

		if ((mgr != 0) && (node != 0)) {
			Cudd_RecursiveDeref(mgr, node);
		}

		bfmanager = other.bfmanager;
		mgr = other.mgr;
		node = other.node;
		if (node != 0)
			Cudd_Ref(node);

		return *this;
	}

	inline int operator<=(const BFBdd& other) const {
		return Cudd_bddLeq(mgr, node, other.node);
	}

	inline int operator>=(const BFBdd& other) const {
		return Cudd_bddLeq(mgr, other.node, node);
	}

	inline int operator<(const BFBdd& other) const {
		int value = Cudd_bddLeq(mgr, node, other.node) && (node != other.node);
		return value;
	}

	inline int operator>(const BFBdd& other) const {
		return Cudd_bddLeq(mgr, other.node, node) && (node != other.node);
	}

	inline BFBdd operator!() const {
		assert(node != 0);
		return BFBdd(bfmanager, Cudd_Not(node));
	}

	inline BFBdd operator&(const BFBdd& other) const {
		return BFBdd(bfmanager, Cudd_bddAnd(mgr, node, other.node));
	}

	inline BFBdd operator|(const BFBdd& other) const {
		return BFBdd(bfmanager, Cudd_bddOr(mgr, node, other.node));
	}

	inline BFBdd operator-(const BFBdd& other) const {
		return BFBdd(bfmanager, Cudd_bddAnd(mgr, node, Cudd_Not(other.node)));
	}

	inline BFBdd operator^(const BFBdd& other) const {
		return BFBdd(bfmanager, Cudd_bddXor(mgr, node, other.node));
	}

        inline BFBdd optimizeRestrict(const BFBdd &other) const {
            // return *this;
            return BFBdd(bfmanager, Cudd_bddRestrict(mgr,node,other.node));
        }

	inline int operator!=(const BFBdd& other) const {
		return node != other.node;
	}

	inline const BFBddManager* manager() const {
		return bfmanager;
	}

	inline bool ReachedLeastFixedPoint(const BFBdd &oldValue) const {
		return *this == oldValue;
	}

	inline bool ReachedGratestFixedPoint(const BFBdd &oldValue) const {
		return *this == oldValue;
	}

	inline BFBdd minimizeUsingCareSet(const BFBdd &other) const {
		DdNode *newOne = Cudd_bddLICompaction(mgr, node, other.node);
		return BFBdd(bfmanager, newOne);
	}

	inline bool isConstant() const {
		return Cudd_IsConstant(node);
	}
	;
	inline int getSize() const {
		return Cudd_DagSize(node);
	}
	;
	inline unsigned int readNodeIndex() const {
		return Cudd_NodeReadIndex(node);
	}
	;
	inline BFBdd SwapVariables(const BFBddVarVector &x, const BFBddVarVector &y) const;
	inline BFBdd AndAbstract(const BFBdd& g, const BFBddVarCube& cube) const;
	inline BFBdd ExistAbstract(const BFBddVarCube& cube) const;
	inline BFBdd ExistAbstractSingleVar(const BFBdd& var) const;
	inline BFBdd UnivAbstract(const BFBddVarCube& cube) const;
        inline BFBdd UnivAbstractSingleVar(const BFBdd& var) const;
	inline BFBdd Implies(const BFBdd& other) const {
        return (!(*this)) | other;
	}

	// Not yet supported
	// BFBdd findSatisfyingAssignment(const BFBddVarCube& cube) const;
	// void findSatisfyingAssignment(const BFBddVarCube& cube, std::list<std::pair<DdHalfWord, bool> > &dest) const;

	/**
	 * This function returns a hashCode, which identifies this node uniquely. This means that, as long as this
	 * the BF object exist, precisely the BFs that represent the same boolean functions have the same
	 * hash code. Note that after deleting this node, the code will be recycled.
	 */
	inline size_t getHashCode() const {
		return (size_t) (node);
	}

    inline DdNode* getCuddNode() const {
        return node;
    }

	friend class BFBddManager;
	friend class BFBddVarCube;
	friend class BFBddVarVector;
};

#endif /* BFCUDD_H_ */
