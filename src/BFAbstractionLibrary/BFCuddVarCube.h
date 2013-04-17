/*
 * BFCuddVarCube.h
 *
 *  Created on: 06.08.2010
 *      Author: ehlers
 */

#ifndef BFCUDDVARCUBE_H_
#define BFCUDDVARCUBE_H_

#include "BFCudd.h"

class BFBddVarCube {
	friend class BFBdd;
	friend class BFBddManager;
private:
	DdNode *cube;
	DdManager *mgr;
	int cubeSize;

    BFBddVarCube( DdManager *_mgr, DdNode *node, int size ) : cube(node), mgr(_mgr), cubeSize(size) { Cudd_Ref(cube); };
public:
	BFBddVarCube() : cube(NULL) {};
	BFBddVarCube(const BFBddVarCube &other) : cube(other.cube), mgr(other.mgr), cubeSize(other.cubeSize) { if (cube!=NULL) Cudd_Ref(cube); };
	BFBddVarCube(const BFBdd &onlyOne) : cube(onlyOne.node), mgr(onlyOne.mgr), cubeSize(1) { if (cube!=NULL) Cudd_Ref(cube); };
	~BFBddVarCube() {
		if (cube!=NULL) Cudd_RecursiveDeref(mgr,cube);
	};
	int size() const { return cubeSize; };

	BFBddVarCube &operator=(const BFBddVarCube &other) {
		if (this==&other) return *this;
		if (cube!=NULL) Cudd_RecursiveDeref(mgr,cube);
		cube = other.cube;
		mgr = other.mgr;
		cubeSize = other.cubeSize;
		if (cube!=NULL) Cudd_Ref(cube);
		return *this;
	};
};

#endif /* BFCUDDVARCUBE_H_ */
