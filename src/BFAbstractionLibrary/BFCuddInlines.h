/*
 * BFCuddInlines.h
 *
 *  Created on: 06.08.2010
 *      Author: ehlers
 *
 * Contains the definitions of inlined BDD functions that depend on the declaration of all
 * BDD classes.
 */

#ifndef BFCUDDINLINES_H_
#define BFCUDDINLINES_H_

#include <cuddInt.h>

inline BFBdd BFBddManager::constantTrue() const {
	return BFBdd(this, Cudd_ReadOne(mgr));
}

inline BFBdd BFBddManager::constantFalse() const {
	return BFBdd(this, Cudd_Not(Cudd_ReadOne(mgr)));
}

inline BFBdd BFBddManager::newVariable() {
	return BFBdd(this, Cudd_bddNewVar(mgr));
}

inline BFBdd BFBdd::SwapVariables(const BFBddVarVector &x, const BFBddVarVector &y) const {
	// Check that the variables are actually grouped in the Cudd variable order
#if 0
//#ifdef DEBUG
	for (int i = 0; i < x.nofNodes; i++) {
		//std::cout << "Test SwapVariables order: " << i << "\n";
		const int diff = std::abs(Cudd_ReadPerm(mgr,Cudd_NodeReadIndex(x.nodes[i])) - Cudd_ReadPerm(mgr,Cudd_NodeReadIndex(y.nodes[i])));
		assert(diff==1);
	}
#endif
	return BFBdd(bfmanager, Cudd_bddSwapVariables(mgr, node, x.nodes, y.nodes, x.nofNodes));
}

inline BFBdd BFBdd::AndAbstract(const BFBdd& g, const BFBddVarCube& cube) const {
	return BFBdd(bfmanager, Cudd_bddAndAbstract(mgr, node, g.node, cube.cube));
}

inline BFBdd BFBdd::ExistAbstract(const BFBddVarCube& cube) const {
	return BFBdd(bfmanager, Cudd_bddExistAbstract(mgr, node, cube.cube));
}

inline BFBdd BFBdd::ExistAbstractSingleVar(const BFBdd& var) const {
	return ExistAbstract(BFBddVarCube(var));
}

inline BFBdd BFBdd::UnivAbstractSingleVar(const BFBdd& var) const {
        return UnivAbstract(BFBddVarCube(var));
}

inline BFBdd BFBdd::UnivAbstract(const BFBddVarCube& cube) const {
	return BFBdd(bfmanager, Cudd_bddUnivAbstract(mgr, node, cube.cube));
}

inline BFBdd BFBddManager::multiAnd(const std::vector<BFBdd> &parts) const {
	DdNode *soFar = Cudd_ReadOne(mgr);
	Cudd_Ref(soFar);
	for (std::vector<BFBdd>::const_iterator it = parts.begin(); it != parts.end(); it++) {
		DdNode *next = Cudd_bddAnd(mgr, soFar, it->node);
		Cudd_Ref(next);
		Cudd_RecursiveDeref(mgr, soFar);
		soFar = next;
	}
	Cudd_Deref(soFar);
	return BFBdd(this, soFar);
}

inline BFBdd BFBddManager::multiOr(const std::vector<BFBdd> &parts) const {
	DdNode *soFar = Cudd_Not(Cudd_ReadOne(mgr));
	Cudd_Ref(soFar);
	for (std::vector<BFBdd>::const_iterator it = parts.begin(); it != parts.end(); it++) {
		DdNode *next = Cudd_bddOr(mgr, soFar, it->node);
		Cudd_Ref(next);
		Cudd_RecursiveDeref(mgr, soFar);
		soFar = next;
	}
	Cudd_Deref(soFar);
	return BFBdd(this, soFar);
}


#endif /* BFCUDDINLINES_H_ */
