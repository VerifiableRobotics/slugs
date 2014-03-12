/*
 * BF.h
 *
 *  Created on: 06.08.2010
 *      Author: ehlers
 */

#ifndef BF_H_
#define BF_H_

#ifdef USE_CUDD
#define USE_BDD

#include "BFCuddManager.h"
#include "BFCudd.h"
#include "BFCuddVarCube.h"
#include "BFCuddVarVector.h"

#define BF BFBdd
#define BFManager BFBddManager
#define BFVarCube BFBddVarCube
#define BFVarVector BFBddVarVector

#include "BFCuddInlines.h"
#include "BFCuddMintermEnumerator.h"

#undef fail

#else
#error Unknown Boolean function engine.
#endif

#endif /* BF_H_ */
