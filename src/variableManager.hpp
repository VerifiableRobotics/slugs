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
#include <list>
#include <vector>
#include <utility>


//! The Variable manager class. Used as basis for the GR(1) context and keeps track of all
//! variables.
class VariableManager : public VariableInfoContainer {
protected:

    //@{
    /** @name Definitions for the BF-related stuff
     */
    BFManager mgr;
    std::vector<BF> variables;
    std::vector<std::string> variableNames;
    std::vector<VariableType> variableTypes;
    //@}


public: // To be changed later
    std::list<std::pair<std::vector<int>,BFVarVector*> >  varVectorsToConstruct;

public:

    void computeVariableInformation();

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
    //@}
};

// Template class to simplify building variable Vectors
class SlugsVarVector {
    BFVarVector vector;
public:
    SlugsVarVector(VariableType t1, VariableManager *varMgr) {
        std::vector<int> types;
        types.push_back(t1);
        varMgr->varVectorsToConstruct.push_back(std::pair<std::vector<int>,BFVarVector*>(types,&vector));
    }
    SlugsVarVector(VariableType t1, VariableType t2, VariableManager *varMgr) {
        std::vector<int> types;
        types.push_back(t1);
        types.push_back(t2);
        varMgr->varVectorsToConstruct.push_back(std::pair<std::vector<int>,BFVarVector*>(types,&vector));
    }
    operator BFVarVector& () {return vector;}
};

#endif
