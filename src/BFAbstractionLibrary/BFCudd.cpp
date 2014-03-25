/*
 * BFCudd.cpp
 *
 *  Created on: 06.08.2010
 *      Author: ehlers
 */

#include "BFCudd.h"
#include "BFCuddVarCube.h"
#include <map>
#include <limits>
#include <cuddInt.h>

/**
 * Internal function
 */
double recurse_getNofSatisfyingAssignments(DdManager *dd, DdNode *orig, DdNode *cube, std::map<DdNode*,double> &buffer) {

	// Normalize the cube
	DdNode *cubeNext;
	if (Cudd_Regular(cube)==cube) {
		cubeNext = cuddT(cube);
	} else {
        throw "Wua";
		if (!Cudd_IsConstant(cube)) {
			cube = Cudd_Regular(cube);
			cubeNext = (DdNode*)(((size_t)(cuddE(cube)) ^ 0x1));
		} else {
			// Constant
			return (orig==dd->one)?1:0;
		}
	}

	if (Cudd_IsConstant(orig)) {
		if (Cudd_IsConstant(cube)) {
			return (orig==dd->one)?1:0;
		} else {
			return 2*recurse_getNofSatisfyingAssignments(dd,orig,cubeNext,buffer);
		}
	}

	size_t xoring = (Cudd_Regular(orig)==orig?0:1);
	DdNode *reference = Cudd_Regular(orig);

	if (Cudd_IsConstant(cube)) return std::numeric_limits<double>::quiet_NaN(); // Missing variable!
	int i1 = cuddI(dd,cube->index);
	int i2 = cuddI(dd,reference->index);

	if (i1<i2) {
		double value = 2*recurse_getNofSatisfyingAssignments(dd,(DdNode*)((size_t)reference ^ xoring),cubeNext,buffer);
		return value;
	} else if (i1>i2) {
		return std::numeric_limits<double>::quiet_NaN();
	} else {
        // Use buffer only for the cases that i1==i2
        if (buffer.count(orig)>0) return buffer[orig];
		double value = recurse_getNofSatisfyingAssignments(dd,(DdNode*)((size_t)(cuddT(reference)) ^ xoring),cubeNext,buffer)
			+ recurse_getNofSatisfyingAssignments(dd,(DdNode*)((size_t)(cuddE(reference)) ^ xoring),cubeNext,buffer);
		buffer[orig] = value;
		return value;
	}
}


/**
 * A function that find out how many variable assignments evaluate to true for a given BDD
 * @param cube A bdd containing the variables to depend on
 * @return The number of assignments.
 */
double BFBdd::getNofSatisfyingAssignments(const BFBddVarCube &_cube) const {

	DdNode *cube = _cube.cube;

	std::map<DdNode*,double> buffer;

	return recurse_getNofSatisfyingAssignments(mgr,node,cube,buffer);
}
