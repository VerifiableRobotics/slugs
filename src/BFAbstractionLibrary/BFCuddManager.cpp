/*
 * BFCuddManager.cpp
 * Created on: 06.08.2010
 * Contains functions for the BDD manager
 *      Author: ehlers
 */

#include "BF.h"
#include <set>
#include <limits>
#include <stdexcept>
#include <sstream>
#include "dddmp.h"
#include "mtr.h"

/**
 * Creates a new BDDManager.
 *
 * @param maxMemoryInMB The amount of memory to be used. Must be <4096 as CUDD does not support more
 * @param reorderingMaxBlowup The maximal allowed blowup during sifting steps in the reordering algorithm. Standard is 1.2 - use 1.1 to have less reordering done. A value of 1.0 results in greedy reordering.
 * @author ehlers
 */
BFBddManager::BFBddManager(unsigned int maxMemoryInMB, float reorderingMaxBlowup) {

	mgr = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, (long) maxMemoryInMB * 1024UL * 1024UL);

	// Configuring the manager
	Cudd_AutodynEnable(mgr, CUDD_REORDER_SIFT);
	Cudd_SetMaxGrowth(mgr, reorderingMaxBlowup);
	Cudd_SetMinHit(mgr, 1);
	setAutomaticOptimisation(true);
}

BFBddManager::~BFBddManager() {
	int nofLeft = Cudd_CheckZeroRef(mgr);
	if (nofLeft != 0) {
		std::cerr << "Warning: " << nofLeft << " referenced nodes in the BDD manager left on destruction!\n";
	}
	Cudd_Quit(mgr);
}

/**
 * A function for switching automatic BDD optimisation (variable reordering) on and off
 *
 * @param enable Whether automatic optimisation should be enabled or not
 * @author ehlers
 */
void BFBddManager::setAutomaticOptimisation(bool enable) {
	if (enable)
		Cudd_AutodynEnable(mgr, CUDD_REORDER_SAME);
	else
		Cudd_AutodynDisable(mgr);
}

/**
 * @brief Sets the maximum blowup due to variable reordering in the BDD during the optimization of the variable ordering.
 * @param reorderingMaxBlowup
 */
void BFBddManager::setReorderingMaxBlowup(float reorderingMaxBlowup) {
    Cudd_SetMaxGrowth(mgr, reorderingMaxBlowup);
}

/**
 * Some statistics printing function
 */
void BFBddManager::printStats() {
	Cudd_PrintInfo(mgr, stdout);
}

/**
 * Groups some variables such that they "stick" together during the reordering process.
 *  --> Not currently supported due to a change in the CUDD API - the makeTreeNode function is no longer available
 * @param which The variables that should be grouped. They must have not been allocated a non-grouped variable in between the allocation of two grouped variables.
 */
/*void BFBddManager::groupVariables(const std::vector<BFBdd> &which) {

	// Only allow continuous variables to be grouped.
	std::set<DdHalfWord> indices;
	DdHalfWord min = std::numeric_limits<DdHalfWord>::max();
	DdHalfWord max = std::numeric_limits<DdHalfWord>::min();
	for (unsigned int i = 0; i < which.size(); i++) {
		DdNode *node = which[i].node;
		DdHalfWord index = ((Cudd_Regular(node))->index);
		if (index < min)
			min = index;
		if (index > max)
			max = index;
		indices.insert(index);
	}

	if ((unsigned int) (max - min + 1) != indices.size())
		throw std::runtime_error("Error in BFBddManager::groupVariables(const std::vector<BFBdd> &which) - Can only group continuous variables!\n");

	Cudd_MakeTreeNode(mgr, min, max - min + 1, MTR_DEFAULT);
}*/

BFBddVarCube BFBddManager::computeCube(const BFBdd *vars, const int * phase, int n) const {
	DdNode **vars2 = new DdNode*[n];
	for (int i = 0; i < n; i++)
		vars2[i] = vars[i].node;
	DdNode *cube = Cudd_bddComputeCube(mgr, vars2, const_cast<int*> (phase), n);
	BFBddVarCube cubic(mgr, cube, n);
	delete[] vars2;
	return cubic;
}

BFBddVarCube BFBddManager::computeCube(const std::vector<BFBdd> &vars) const {
	DdNode **vars2 = new DdNode*[vars.size()];
	int *phase = new int[vars.size()];
	for (unsigned int i = 0; i < vars.size(); i++) {
		vars2[i] = vars[i].node;
		phase[i] = 1;
	}
	DdNode *cube = Cudd_bddComputeCube(mgr, vars2, phase, vars.size());
	BFBddVarCube cubic(mgr, cube, vars.size());
	delete[] vars2;
	delete[] phase;
	return cubic;
}

BFBddVarVector BFBddManager::computeVarVector(const std::vector<BFBdd> &from) const {
	BFBddVarVector v;
	v.nodes = new DdNode*[from.size()];
	for (unsigned int i = 0; i < from.size(); i++) {
		v.nodes[i] = (from)[i].node;
		Cudd_Ref(v.nodes[i]);
	}
	v.nofNodes = from.size();
	v.mgr = mgr;
	return v;
}

BFBdd BFBddManager::readBDDFromFile(const char *filename, std::vector<BFBdd> &vars) const {

    FILE *file = fopen (filename,"r");
    if (file == NULL){
        std::ostringstream os;
        os << "Error in BFBddManager::readBDDFromFile(const char *filename, std::vector<BFBdd> &vars) - Could not read a BDD from from file '" << filename << "'.";
        throw std::runtime_error(os.str().c_str());
    }

    int *idMatcher = new int[vars.size()];
    for (unsigned int i=0;i<vars.size();i++) {
        idMatcher[i] = vars[i].readNodeIndex();
    }
    DdNode *node = Dddmp_cuddBddLoad(mgr, DDDMP_VAR_COMPOSEIDS, NULL, NULL, idMatcher, DDDMP_MODE_DEFAULT,NULL,file);
    fclose(file);
    delete[] idMatcher;
    Cudd_Deref(node);
    return BFBdd(this,node);   
}

void BFBddManager::writeBDDToFile(const char *filename, std::string fileprefix, BFBdd bdd, std::vector<BFBdd> &vars, std::vector<std::string> variableNames) const {

    FILE *file = fopen (filename,"w");
    if (file == NULL){
        std::ostringstream os;
        os << "Error in BFBddManager::writeBDDToFile(const char *filename, std::vector<BFBdd> &vars, std::vector<std::string> variableNames) - Could not write a BDD from to file '" << filename << "'.";
        throw std::runtime_error(os.str().c_str());
    }
    if (fileprefix.size() != fwrite(fileprefix.c_str(),1,fileprefix.size(),file)) {
        throw "Error: Unable to write prefix string to file.";
    }

    int *idMatcher = new int[vars.size()];
    for (unsigned int i=0;i<vars.size();i++) {
        idMatcher[i] = vars[i].readNodeIndex();
    }

    const char* varNamesChar[vars.size()];
    for (unsigned int i=0;i<vars.size();i++) {
        varNamesChar[i] = variableNames[i].c_str();
    }

    int storeReturnValue = Dddmp_cuddBddStore(
      mgr,
      NULL,
      bdd.getCuddNode(),
      (char**)varNamesChar, // char ** varnames, IN: array of variable names (or NULL)
      idMatcher,
      DDDMP_MODE_TEXT,
      // DDDMP_VARNAMES,
      DDDMP_VARIDS,
      NULL,
      file
    );

    delete[] idMatcher;
    fclose(file);

    if (storeReturnValue!=DDDMP_SUCCESS) throw "Error: Unable to write BDD to file.";

}

