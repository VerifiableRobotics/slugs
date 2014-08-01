/*
 * BFCuddVarVector.h
 *
 *  Created on: 06.08.2010
 *      Author: ehlers
 */

#ifndef BFCUDDVARVECTOR_H_
#define BFCUDDVARVECTOR_H_

#include "BFCudd.h"

class BFBddVarVector {
private:
	DdNode **nodes;
	DdManager *mgr;
	int nofNodes;
	friend class BFBdd;
	friend class BFBddManager;
public:
        BFBddVarVector() : nodes(NULL), mgr(NULL), nofNodes(0) {};
	~BFBddVarVector() {
		if (nodes!=NULL) {
			for (int i=0;i<nofNodes;i++) Cudd_RecursiveDeref(mgr,nodes[i]);
			delete[] nodes;
		}
	}

	BFBddVarVector(BFBddVarVector const &other) {
		nofNodes = other.nofNodes;
		mgr = other.mgr;
		if (other.nodes!=NULL) {
			nodes = new DdNode*[nofNodes];
			for (int i=0;i<nofNodes;i++) {
				DdNode *temp = other.nodes[i];
				Cudd_Ref(temp);
				nodes[i] = temp;
			}
		} else {
			nodes = NULL;
		}
	}


	BFBddVarVector& operator=(const BFBddVarVector &other) {
		if (this==&other) return *this;
		if (nodes!=NULL) {
			for (int i=0;i<nofNodes;i++) Cudd_RecursiveDeref(mgr,nodes[i]);
			delete[] nodes;
		}
		nofNodes = other.nofNodes;
		mgr = other.mgr;
		if (other.nodes!=NULL) {
			nodes = new DdNode*[nofNodes];
			for (int i=0;i<nofNodes;i++) {
				DdNode *temp = other.nodes[i];
				Cudd_Ref(temp);
				nodes[i] = temp;
			}
		} else {
			nodes = NULL;
		}
		return *this;
	}

	int size() const { return nofNodes; };
};

#endif /* BFBDDVARVECTOR_H_ */
