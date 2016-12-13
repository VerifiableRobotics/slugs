/*!
    \file    variableTypes.hpp
    \brief   Centralized list of symbolic variables that the extensions or the
             base version of the synthesis algorithm may need.

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

#ifndef __VARIABLE_TYPES_HPP__
#define __VARIABLE_TYPES_HPP__

/*
 * When allocating a new variable type (or typically two as Pre- and Post-variables
 * are needed, three things need to be done:
 *
 * 1. Add the variable type to the Enum "VariableType"
 * 2. Allocate a name for the variable type
 * 3. Set a parent type (can also be at the top-most level, i.e., "NoneVariableType")
 */

//@{
/** @name Some macros that allow to define variable information here. Overridden by the
 *  variable manager
 */
#ifndef INTERPRET_ADDITIONAL_VARIABLE_TYPE_INFO
 #define REGISTER_VARIABLE_TYPE_STRING(a,b)
 #define REGISTER_VARIABLE_TYPE_HIERARCHY(a,b)
#endif
//@}

//! The enumeration of all variable types
typedef enum {

    // Abstract types - Pre or Post?
    Pre, Post,

    // Standard set of variables - present already in the standard GR(1) context
    PreInput, PreOutput, PostInput, PostOutput,

    // Variables used in "extensionIROSfastslow.hpp"
    PreOutputFast, PreOutputSlow, PostOutputFast, PostOutputSlow,
    
    // Variables used in "extensionInterleave.hpp"
    PreOutputBefore, PreOutputAfter, PostOutputBefore, PostOutputAfter,

    // Variables used in "IncompleteInformationEstimatorSynthesis.hpp"
    ObservablePre, ObservablePost, ControllablePre, ControllablePost,
    UnobservablePre, UnobservablePost, EstimationPre, EstimationPost,

    // Variables used in extensionNondeterministicMotion.hpp
    PreMotionState, PostMotionState, PreOtherOutput, PostOtherOutput,
    PreMotionControlOutput, PostMotionControlOutput,

    // Variables used in extensionExtractSymbolicStrategy.hpp
    SymbolicStrategyCounterVar,

    // Variables used in extensionTwoDimensionalCost.hpp
    CurrentLivenessAssumptionCounterPre, CurrentLivenessAssumptionCounterPost, IsInftyCostPre,
    IsInftyCostPost, TransitiveClosureIntermediateVariables,

    // Dummy parameters used for internal reasons. Must be last in the enum
    NoneVariableType

} VariableType;

//! Variable class names. Used, for example, when printing debugging BDDs
REGISTER_VARIABLE_TYPE_STRING(Pre,"Pre")
REGISTER_VARIABLE_TYPE_STRING(Post,"Post")
REGISTER_VARIABLE_TYPE_STRING(PreInput,"PreInput")
REGISTER_VARIABLE_TYPE_STRING(PreOutput,"PreOutput")
REGISTER_VARIABLE_TYPE_STRING(PostInput,"PostInput")
REGISTER_VARIABLE_TYPE_STRING(PostOutput,"PostOutput")
REGISTER_VARIABLE_TYPE_STRING(PreOutputFast,"PreOutputFast")
REGISTER_VARIABLE_TYPE_STRING(PreOutputSlow,"PreOutputSlow")
REGISTER_VARIABLE_TYPE_STRING(PostOutputFast,"PostOutputFast")
REGISTER_VARIABLE_TYPE_STRING(PostOutputSlow,"PosOutputSlow")
REGISTER_VARIABLE_TYPE_STRING(PreOutputBefore,"PreOutputBefore")
REGISTER_VARIABLE_TYPE_STRING(PreOutputAfter,"PreOutputAfter")
REGISTER_VARIABLE_TYPE_STRING(PostOutputBefore,"PostOutputBefore")
REGISTER_VARIABLE_TYPE_STRING(PostOutputAfter,"PostOutputAfter")
REGISTER_VARIABLE_TYPE_STRING(ObservablePre,"ObservablePre")
REGISTER_VARIABLE_TYPE_STRING(ObservablePost,"ObservablePost")
REGISTER_VARIABLE_TYPE_STRING(UnobservablePre,"UnobservablePre")
REGISTER_VARIABLE_TYPE_STRING(UnobservablePost,"UnobservablePost")
REGISTER_VARIABLE_TYPE_STRING(EstimationPre,"EstimationPre")
REGISTER_VARIABLE_TYPE_STRING(EstimationPost,"EstimationPost")
REGISTER_VARIABLE_TYPE_STRING(ControllablePre,"ControllablePre")
REGISTER_VARIABLE_TYPE_STRING(ControllablePost,"ControllablePost")
REGISTER_VARIABLE_TYPE_STRING(PreMotionState,"PreMotionState")
REGISTER_VARIABLE_TYPE_STRING(PostMotionState,"PostMotionState")
REGISTER_VARIABLE_TYPE_STRING(PreOtherOutput,"PreOtherOutput")
REGISTER_VARIABLE_TYPE_STRING(PostOtherOutput,"PostOtherOutput")
REGISTER_VARIABLE_TYPE_STRING(PreMotionControlOutput,"PreMotionControlOutput")
REGISTER_VARIABLE_TYPE_STRING(PostMotionControlOutput,"PostMotionControlOutput")
REGISTER_VARIABLE_TYPE_STRING(SymbolicStrategyCounterVar,"SymbolicStrategyCounterVar")
REGISTER_VARIABLE_TYPE_STRING(CurrentLivenessAssumptionCounterPost, "CurrentLivenessAssumptionCounterPost")
REGISTER_VARIABLE_TYPE_STRING(IsInftyCostPost,"IsInftyCostPost")
REGISTER_VARIABLE_TYPE_STRING(CurrentLivenessAssumptionCounterPre, "CurrentLivenessAssumptionCounterPre")
REGISTER_VARIABLE_TYPE_STRING(IsInftyCostPre,"IsInftyCostPre")
REGISTER_VARIABLE_TYPE_STRING(TransitiveClosureIntermediateVariables,"TransitiveClosureIntermediateVariables")


//! Variable hierarchy. Allows to include subgrouped variables when computing variable
//! vectors and cubes
REGISTER_VARIABLE_TYPE_HIERARCHY(Pre,NoneVariableType)
REGISTER_VARIABLE_TYPE_HIERARCHY(Post,NoneVariableType)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreInput,Pre)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostInput,Post)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreOutput,Pre)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostOutput,Post)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreOutputFast,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreOutputSlow,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostOutputFast,PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostOutputSlow,PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreOutputBefore,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreOutputAfter,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostOutputBefore,PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostOutputAfter,PostOutput)

// Variables used in "IncompleteInformationEstimatorSynthesis.hpp"
REGISTER_VARIABLE_TYPE_HIERARCHY(ObservablePre,PreInput)
REGISTER_VARIABLE_TYPE_HIERARCHY(ObservablePost,PostInput)
REGISTER_VARIABLE_TYPE_HIERARCHY(UnobservablePre,PreInput)
REGISTER_VARIABLE_TYPE_HIERARCHY(UnobservablePost,PostInput)
REGISTER_VARIABLE_TYPE_HIERARCHY(EstimationPre,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(EstimationPost,PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(ControllablePre,ObservablePre)
REGISTER_VARIABLE_TYPE_HIERARCHY(ControllablePost,ObservablePost)

// Variables used in extensionNondeterministicMotion.hpp
REGISTER_VARIABLE_TYPE_HIERARCHY(PreMotionState,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostMotionState,PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreOtherOutput,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostOtherOutput,PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PreMotionControlOutput,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(PostMotionControlOutput,PostOutput)

// Variables used in extensionExtractSymbolicStrategy.hpp
REGISTER_VARIABLE_TYPE_HIERARCHY(SymbolicStrategyCounterVar,NoneVariableType)

// Variables used in extensionTwoDimensionalCost.hpp
REGISTER_VARIABLE_TYPE_HIERARCHY(CurrentLivenessAssumptionCounterPre, PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(IsInftyCostPre,PreOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(CurrentLivenessAssumptionCounterPost, PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(IsInftyCostPost,PostOutput)
REGISTER_VARIABLE_TYPE_HIERARCHY(TransitiveClosureIntermediateVariables,NoneVariableType)


#endif
