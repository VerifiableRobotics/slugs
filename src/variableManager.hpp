/*!
    \file    variableManager.hpp
    \brief   Variable manager for Slugs - provides the needed variable vectors
             and cubes

    --------------------------------------------------------------------------

    SLUGS: SmaLl bUt complete Gr(1) Synthesis tool

    Copyright (c) 2013-2014, Ruediger Ehlers, Vasumathi Raman, and Cameron Finucane
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in the
          documentation and/or other materials provided with the distribution.
        * Neither the name of any university at which development of this tool
          was performed nor the names of its contributors may be used to endorse
          or promote products derived from this software without specific prior
          written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS AND CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */


#ifndef __VARIABLE_MANAGER_HPP__
#define __VARIABLE_MANAGER_HPP__

#include "bddDump.h"
#include <map>
#include <vector>
#include <utility>
#include <boost/noncopyable.hpp>

//! The Variable manager class. Used as basis for the GR(1) context and keeps track of all
//! variables.
class SlugsVariableManager : public VariableInfoContainer {
protected:

    //@{
    /** @name Definitions for the BF-related stuff
     */
    BFManager mgr;
    std::vector<BF> variables;
    std::vector<std::string> variableNames;
    std::vector<VariableType> variableTypes;
    std::vector<std::set<int> > variableTypesAll; //! Is computed when calling computeVariableInformation()
    //@}


public: // To be changed later
    std::map<BFVarVector*,std::vector<int> >  varVectorsToConstruct;
    std::map<BFVarCube*,std::set<int> >  varCubesToConstruct;
    std::map<std::vector<BF>*,std::vector<int> >  varVectorOfBFsToConstruct;

public:

    void computeVariableInformation();
    bool doesVariableInheritType(int variableNumber, VariableType type) const {
        assert(variableTypesAll.size()>0); // computeVariableInformation must have been called already
        return variableTypesAll[variableNumber].count(type)>0;
    }

    //@{
    /**
     * @name The following functions are inherited from the VariableInfo container.
     * They allow us to the the BF_dumpDot function for debugging new variants of the synthesis algorithm
     */
    void getVariableTypes(std::vector<std::string> &types) const;
    void getVariableNumbersOfType(std::string typeString, std::vector<unsigned int> &nums) const;
    int addVariable(VariableType type, std::string name);
    virtual BF getVariableBF(unsigned int number) const {
        return variables[number];
    }
    virtual std::string getVariableName(unsigned int number) const {
        return variableNames[number];
    }
    virtual unsigned int findVariableNumber(std::string nameString) const {
        for (unsigned int i=0;i<variables.size();i++) {
            if (nameString==variableNames[i]) return i;
        }
        assert(false);
        return (unsigned int)(-1);
    }
    //@}
};

// Template class to simplify building variable vectors
// Note that the class is non-copyable as it reports an address to the
// slugs variable manager
class SlugsVarVector : public boost::noncopyable {
    BFVarVector vector;
public:
    SlugsVarVector(VariableType t1, SlugsVariableManager *varMgr) {
        std::vector<int> types;
        types.push_back(t1);
        varMgr->varVectorsToConstruct[&vector] = types;
    }
    SlugsVarVector(VariableType t1, VariableType t2, SlugsVariableManager *varMgr) {
        std::vector<int> types;
        types.push_back(t1);
        types.push_back(t2);
        varMgr->varVectorsToConstruct[&vector] = types;
    }
    operator BFVarVector& () {return vector;}
    size_t size() { return vector.size(); }
};

// Template class to simplify building variable cube
// Note that the class is non-copyable as it reports an address to the
// slugs variable manager
class SlugsVarCube : public boost::noncopyable {
    BFVarCube cube;
public:
    SlugsVarCube(VariableType t1, SlugsVariableManager *varMgr) {
        std::set<int> types;
        types.insert(t1);
        varMgr->varCubesToConstruct[&cube] = types;
    }
    SlugsVarCube(VariableType t1, VariableType t2, SlugsVariableManager *varMgr) {
        std::set<int> types;
        types.insert(t1);
        types.insert(t2);
        varMgr->varCubesToConstruct[&cube] = types;
    }
    SlugsVarCube(VariableType t1, VariableType t2, VariableType t3, SlugsVariableManager *varMgr) {
        std::set<int> types;
        types.insert(t1);
        types.insert(t2);
        types.insert(t3);
        varMgr->varCubesToConstruct[&cube] = types;
    }
    SlugsVarCube(VariableType t1, VariableType t2, VariableType t3, VariableType t4, SlugsVariableManager *varMgr) {
        std::set<int> types;
        types.insert(t1);
        types.insert(t2);
        types.insert(t3);
        types.insert(t4);
        varMgr->varCubesToConstruct[&cube] = types;
    }
    operator BFVarCube& () {return cube;}
    size_t size() { return cube.size(); }
};

// Template class to simplify building variable vectors
// Note that the class is non-copyable as it reports an address to the
// slugs variable manager
class SlugsVectorOfVarBFs : public boost::noncopyable {
    std::vector<BF> vector;
public:
    SlugsVectorOfVarBFs(VariableType t1, SlugsVariableManager *varMgr) {
        std::vector<int> types;
        types.push_back(t1);
        varMgr->varVectorOfBFsToConstruct[&vector] = types;
    }
    SlugsVectorOfVarBFs(VariableType t1, VariableType t2, SlugsVariableManager *varMgr) {
        std::vector<int> types;
        types.push_back(t1);
        types.push_back(t2);
        varMgr->varVectorOfBFsToConstruct[&vector] = types;
    }
    size_t size() { return vector.size(); }
    operator const std::vector<BF>& () {return vector;}
    BF operator[](size_t pos) const { return vector.at(pos); }
};

#endif
