/*!
    \file    main.cpp
    \brief   Program entry point

    --------------------------------------------------------------------------

    SLUGS: SmaLl bUt complete Gr(1) Synthesis tool

    Copyright (c) 2013, Ruediger Ehlers, Vasumathi Raman, and Cameron Finucane
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

#include <fstream>
#include <cstring>
#include "extensionComputeCNFFormOfTheSpecification.hpp"
#include "extensionBiasForAction.hpp"
#include "extensionExtractExplicitStrategy.hpp"
#include "extensionCounterstrategy.hpp"
#include "extensionExtractExplicitCounterstrategy.hpp"
#include "extensionRoboticsSemantics.hpp"
#include "extensionWeakenSafetyAssumptions.hpp"
#include "extensionFixedPointRecycling.hpp"
#include "extensionInteractiveStrategy.hpp"
#include "extensionIROSfastslow.hpp"
#include "extensionInterleave.hpp"
#include "extensionAnalyzeInitialPositions.hpp"
#include "extensionAnalyzeAssumptions.hpp"
#include "extensionComputeInterestingRunOfTheSystem.hpp"
#include "extensionAnalyzeSafetyLivenessInteraction.hpp"
#include "extensionAbstractWinningTraceGenerator.hpp"
#include "extensionPermissiveExplicitStrategy.hpp"
#include "extensionIncompleteInformationEstimatorSynthesis.hpp"
#include "extensionNondeterministicMotion.hpp"
#include "extensionExtractSymbolicStrategy.hpp"
#include "extensionTwoDimensionalCost.hpp"

//===================================================================================
// List of command line arguments
//===================================================================================
const char *commandLineArguments[] = {
    "--onlyRealizability","Use this parameter if no synthesized system should be computed, but only the realizability/unrealizability result is to be computed. If this option is *not* given, an automaton is computed. In case of realizability, the synthesized controller is printed to an output file name if it is given, and to stdout otherwise.",
    "--symbolicStrategy","Extract a symbolic (BDD-based) strategy.",
    "--jsonOutput","When computing an explicit finite-state strategy, use json output.",
    "--sysInitRoboticsSemantics","In standard GR(1) synthesis, a specification is called realizable if for every initial input proposition valuation that is allowed by the initialization contraints, there is some suitable output proposition valuation. In the modified semantics for robotics applications, the controller has to be fine with any admissible initial position in the game.",
    "--computeWeakenedSafetyAssumptions","Extract a minimal conjunctive normal form Boolean formula that represents some minimal CNF for a set of safety assumptions that leads to realiazability and is a weakened form of the safety assumptions given. Requires the option '--onlyRealizability' to be given as well.",
    "--biasForAction","Extract controllers that rely on the liveness assumptions being satisfies as little as possible.",
    "--computeCNFFormOfTheSpecification","Lets the Synthesis tool skip the synthesis step, and only compute a version of the specification that can be used by other synthesis tools that require it to be CNF-based.",
    "--counterStrategy","Computes the environment counterstrategy",
    "--simpleRecovery","Adds transitions to the system implementation that allow it to recover from sparse environment safety assumption faults in many cases",
    "--experimentalIncrementalSynthesis","A very experimental implementation of incremental synthesis for sequences of changing GR(1) specifications. By no means stable yet!",
    "--fixedPointRecycling","Modifies the realizability checking algorithm to recycle previous innermost fixed points. Realizability checking should typically become faster this way.",
    "--interactiveStrategy","Opens an interactive shell after realizability checking to allow examining the properties of the generated strategy.",
    "--IROSfastslow","Uses fastslow semantics from IROS 2012 paper. Requires different input file format.",
    "--analyzeInterleaving","Interleaves the turn-taking. Requires different input file format.",
    "--analyzeInitialPositions","Performs an analysis of the set of starting positions in the realizability game.",
    "--restrictToReachableStates","Restricts the analysis of the starting positions (see --analyzeInitialPositions) to reachable ones.",
    "--analyzeAssumptions","Checks which assumptions are actually needed and which assumptions are helpful (i.e., they sometimes reduce reactive distances to the goal).",
    "--analyzeSafetyLivenessInteraction","Analyzes how safety and liveness properties interact in the specification.",
    "--computeInterestingRunOfTheSystem","Computes an interesting run of the synthesized system",
    "--computeAbstractWinningTrace","Computes an abstract trace that is winning for the system or the environment.",
    "--extractExplicitPermissiveStrategy","Computes an explicit-state permissive strategy.",
    "--computeIncompleteInformationEstimator","Computes a imcomplete-information state estimation controller.",
    "--nonDeterministicMotion","Computes a controller using an non-deterministic motion abstraction.",
    "--twoDimensionalCost","Computes a controller that optimizes for waiting and action cost at the same time."
};

//===================================================================================
// List of combinations allowed
//
// -> Parameters need to be ordered lexicographically
// -> File names will be read from the inside-out. "XExtractStrategy" must therefore
//    be outermost, as it represents an *optional* parameter
//
// Constraints on the parameter combinations:
// - 'computeWeakenedSafetyAssumptions' requires 'onlyRealizability' - No strategy can be computed in this case
// - 'biasForAction' is not compatible with 'onlyRealizability' - 'biasForAction' only makes a difference for extracting a strategy
// - '--computeCNFFormOfTheSpecification' is only available with '--sysInitRoboticsSemantics' (but only for clarity issues - the inheritance is actually not needed
//
// Constraints on the ordering of the templates:
// - XExtractStratey is always last, to make sure that the last file name provided is the output file.
//===================================================================================
struct OptionCombination { std::string params; GR1Context* (*factory)(std::list<std::string> &l); OptionCombination(std::string _p, GR1Context* (*_f)(std::list<std::string> &l)) : params(_p), factory(_f) {} };
OptionCombination optionCombinations[] = {
    OptionCombination("",XExtractExplicitStrategy<GR1Context,false,false>::makeInstance),
    OptionCombination("--jsonOutput",XExtractExplicitStrategy<GR1Context,false,true>::makeInstance),
    OptionCombination("--symbolicStrategy",XExtractSymbolicStrategy<GR1Context,false>::makeInstance),
    OptionCombination("--simpleRecovery",XExtractExplicitStrategy<GR1Context,true,false>::makeInstance),
    OptionCombination("--jsonOutput --simpleRecovery",XExtractExplicitStrategy<GR1Context,true,true>::makeInstance),
    OptionCombination("--simpleRecovery --symbolicStrategy",XExtractSymbolicStrategy<GR1Context,true>::makeInstance),
    OptionCombination("--onlyRealizability",GR1Context::makeInstance),
    OptionCombination("--sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<GR1Context>,false,false>::makeInstance),
    OptionCombination("--jsonOutput -sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<GR1Context>,false,true>::makeInstance),
    OptionCombination("--simpleRecovery --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<GR1Context>,true,false>::makeInstance),
    OptionCombination("--jsonOutput --simpleRecovery --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<GR1Context>,true,true>::makeInstance),
    OptionCombination("--symbolicStrategy --sysInitRoboticsSemantics",XExtractSymbolicStrategy<XRoboticsSemantics<GR1Context>,false>::makeInstance),
    OptionCombination("--simpleRecovery ---symbolicStrategy -sysInitRoboticsSemantics",XExtractSymbolicStrategy<XRoboticsSemantics<GR1Context>,true>::makeInstance),
    OptionCombination("--onlyRealizability --sysInitRoboticsSemantics",XRoboticsSemantics<GR1Context>::makeInstance),
    OptionCombination("--computeWeakenedSafetyAssumptions --onlyRealizability",XComputeWeakenedSafetyAssumptions<GR1Context>::makeInstance),
    OptionCombination("--computeWeakenedSafetyAssumptions --onlyRealizability --sysInitRoboticsSemantics",XComputeWeakenedSafetyAssumptions<XRoboticsSemantics<GR1Context> >::makeInstance),
    OptionCombination("--biasForAction",XExtractExplicitStrategy<XBiasForAction<GR1Context>,false,false>::makeInstance),
    OptionCombination("--biasForAction --simpleRecovery",XExtractExplicitStrategy<XBiasForAction<GR1Context>,true,false>::makeInstance),
    OptionCombination("--biasForAction --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XBiasForAction<GR1Context> >,false,false>::makeInstance),
    OptionCombination("--biasForAction --simpleRecovery --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XBiasForAction<GR1Context> >,true,false>::makeInstance),
    OptionCombination("--biasForAction --jsonOutput",XExtractExplicitStrategy<XBiasForAction<GR1Context>,false,true>::makeInstance),
    OptionCombination("--biasForAction --jsonOutput --simpleRecovery",XExtractExplicitStrategy<XBiasForAction<GR1Context>,true,true>::makeInstance),
    OptionCombination("--biasForAction --jsonOutput --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XBiasForAction<GR1Context> >,false,true>::makeInstance),
    OptionCombination("--biasForAction --jsonOutput --simpleRecovery --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XBiasForAction<GR1Context> >,true,true>::makeInstance),
    OptionCombination("--biasForAction --symbolicStrategy",XExtractSymbolicStrategy<XBiasForAction<GR1Context>,false>::makeInstance),
    OptionCombination("--biasForAction --simpleRecovery --symbolicStrategy",XExtractSymbolicStrategy<XBiasForAction<GR1Context>,true>::makeInstance),
    OptionCombination("--biasForAction --symbolicStrategy --sysInitRoboticsSemantics",XExtractSymbolicStrategy<XRoboticsSemantics<XBiasForAction<GR1Context> >,false>::makeInstance),
    OptionCombination("--biasForAction --simpleRecovery --symbolicStrategy --sysInitRoboticsSemantics",XExtractSymbolicStrategy<XRoboticsSemantics<XBiasForAction<GR1Context> >,true>::makeInstance),
    OptionCombination("--computeCNFFormOfTheSpecification --sysInitRoboticsSemantics",XComputeCNFFormOfTheSpecification<GR1Context>::makeInstance),
    OptionCombination("--counterStrategy",XExtractExplicitCounterStrategy<XCounterStrategy<GR1Context,false> >::makeInstance),
    OptionCombination("--counterStrategy --sysInitRoboticsSemantics",XExtractExplicitCounterStrategy<XCounterStrategy<GR1Context,true> >::makeInstance),
    OptionCombination("--fixedPointRecycling",XExtractExplicitStrategy<XFixedPointRecycling<GR1Context>,false,false>::makeInstance),
    OptionCombination("--fixedPointRecycling --simpleRecovery",XExtractExplicitStrategy<XFixedPointRecycling<GR1Context>,true,false>::makeInstance),
    OptionCombination("--fixedPointRecycling --jsonOutput",XExtractExplicitStrategy<XFixedPointRecycling<GR1Context>,false,true>::makeInstance),
    OptionCombination("--fixedPointRecycling --jsonOutput --simpleRecovery",XExtractExplicitStrategy<XFixedPointRecycling<GR1Context>,true,true>::makeInstance),
    OptionCombination("--fixedPointRecycling --onlyRealizability",XFixedPointRecycling<GR1Context>::makeInstance),
    OptionCombination("--fixedPointRecycling --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XFixedPointRecycling<GR1Context> >,false,false>::makeInstance),
    OptionCombination("--fixedPointRecycling --simpleRecovery --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XFixedPointRecycling<GR1Context> >,true,false>::makeInstance),
    OptionCombination("--fixedPointRecycling --jsonOutput --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XFixedPointRecycling<GR1Context> >,false,true>::makeInstance),
    OptionCombination("--fixedPointRecycling --jsonOutput --simpleRecovery --sysInitRoboticsSemantics",XExtractExplicitStrategy<XRoboticsSemantics<XFixedPointRecycling<GR1Context> >,true,true>::makeInstance),
    OptionCombination("--fixedPointRecycling --onlyRealizability --sysInitRoboticsSemantics",XRoboticsSemantics<XFixedPointRecycling<GR1Context> >::makeInstance),
    OptionCombination("--fixedPointRecycling --symbolicStrategy",XExtractSymbolicStrategy<XFixedPointRecycling<GR1Context>,false>::makeInstance),
    OptionCombination("--fixedPointRecycling --simpleRecovery --symbolicStrategy",XExtractSymbolicStrategy<XFixedPointRecycling<GR1Context>,true>::makeInstance),
    OptionCombination("--fixedPointRecycling --symbolicStrategy --sysInitRoboticsSemantics",XExtractSymbolicStrategy<XRoboticsSemantics<XFixedPointRecycling<GR1Context> >,false>::makeInstance),
    OptionCombination("--fixedPointRecycling --simpleRecovery --symbolicStrategy --sysInitRoboticsSemantics",XExtractSymbolicStrategy<XRoboticsSemantics<XFixedPointRecycling<GR1Context> >,true>::makeInstance),
    OptionCombination("--computeWeakenedSafetyAssumptions --fixedPointRecycling --onlyRealizability",XComputeWeakenedSafetyAssumptions<XFixedPointRecycling<GR1Context> >::makeInstance),
    OptionCombination("--computeWeakenedSafetyAssumptions --fixedPointRecycling --onlyRealizability --sysInitRoboticsSemantics",XComputeWeakenedSafetyAssumptions<XRoboticsSemantics<XFixedPointRecycling<GR1Context> > >::makeInstance),
    OptionCombination("--computeCNFFormOfTheSpecification --fixedPointRecycling --sysInitRoboticsSemantics",XComputeCNFFormOfTheSpecification<XFixedPointRecycling<GR1Context> >::makeInstance),
    OptionCombination("--interactiveStrategy",XInteractiveStrategy<GR1Context>::makeInstance),
    OptionCombination("--IROSfastslow",XExtractExplicitStrategy<XIROSFS<GR1Context>,false,false>::makeInstance),
    OptionCombination("--jsonOutput --IROSfastslow",XExtractExplicitStrategy<XIROSFS<GR1Context>,false,true>::makeInstance),
    OptionCombination("--IROSfastslow --onlyRealizability",XIROSFS<GR1Context>::makeInstance),
    OptionCombination("--IROSfastslow --sysInitRoboticsSeantics",XExtractExplicitStrategy<XRoboticsSemantics<XIROSFS<GR1Context> >,false,false>::makeInstance),
    OptionCombination("--jsonOutput --IROSfastslow --sysInitRoboticsSeantics",XExtractExplicitStrategy<XRoboticsSemantics<XIROSFS<GR1Context> >,false,true>::makeInstance),
    OptionCombination("--IROSfastslow --onlyRealizability --sysInitRoboticsSemantics",XRoboticsSemantics<XIROSFS<GR1Context> >::makeInstance),
    OptionCombination("--IROSfastslow --symbolicStrategy",XExtractSymbolicStrategy<XIROSFS<GR1Context>,false>::makeInstance),
    OptionCombination("--IROSfastslow --symbolicStrategy --sysInitRoboticsSemantics",XExtractSymbolicStrategy<XRoboticsSemantics<XIROSFS<GR1Context> >,false>::makeInstance),
    OptionCombination("--analyzeInterleaving",XInterleave<GR1Context>::makeInstance),
    OptionCombination("--analyzeInitialPositions", XAnalyzeInitialPositions<GR1Context,false>::makeInstance),
    OptionCombination("--analyzeInitialPositions --restrictToReachableStates", XAnalyzeInitialPositions<GR1Context,true>::makeInstance),
    OptionCombination("--analyzeSafetyLivenessInteraction", XAnalyzeSafetyLivenessInteraction<GR1Context>::makeInstance),
    OptionCombination("--analyzeAssumptions", XAnalyzeAssumptions<GR1Context>::makeInstance),
    OptionCombination("--computeInterestingRunOfTheSystem", XComputeInterestingRunOfTheSystem<GR1Context>::makeInstance),
    OptionCombination("--computeAbstractWinningTrace", XAbstractWinningTraceGenerator<GR1Context>::makeInstance),
    OptionCombination("--extractExplicitPermissiveStrategy", XExtractPermissiveExplicitStrategy<GR1Context>::makeInstance),
    OptionCombination("--computeIncompleteInformationEstimator", XIncompleteInformationEstimatorSynthesis<GR1Context>::makeInstance),
    OptionCombination("--nonDeterministicMotion",XNonDeterministicMotion<GR1Context,false>::makeInstance),
    OptionCombination("--nonDeterministicMotion --sysInitRoboticsSemantics",XNonDeterministicMotion<GR1Context,true>::makeInstance),
    OptionCombination("--twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<GR1Context>,false,false>::makeInstance),
    OptionCombination("--simpleRecovery --twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<GR1Context>,true,false>::makeInstance),
    OptionCombination("--jsonOutput --twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<GR1Context>,false,true>::makeInstance),
    OptionCombination("--jsonOutput --simpleRecovery --twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<GR1Context>,true,true>::makeInstance),
    OptionCombination("--symbolicStrategy --twoDimensionalCost",XExtractSymbolicStrategy<XTwoDimensionalCost<GR1Context>,false>::makeInstance),
    OptionCombination("--simpleRecovery --symbolicStrategy --twoDimensionalCost",XExtractSymbolicStrategy<XTwoDimensionalCost<GR1Context>,true>::makeInstance),
    OptionCombination("--sysInitRoboticsSemantics --twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<XRoboticsSemantics<GR1Context> >,false,false>::makeInstance),
    OptionCombination("--simpleRecovery --sysInitRoboticsSemantics --twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<XRoboticsSemantics<GR1Context> >,true,false>::makeInstance),
    OptionCombination("--jsonOutput --sysInitRoboticsSemantics --twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<XRoboticsSemantics<GR1Context> >,false,true>::makeInstance),
    OptionCombination("--jsonOutput --simpleRecovery --sysInitRoboticsSemantics --twoDimensionalCost",XExtractExplicitStrategy<XTwoDimensionalCost<XRoboticsSemantics<GR1Context> >,true,true>::makeInstance),
    OptionCombination("--symbolicStrategy --sysInitRoboticsSemantics --twoDimensionalCost",XExtractSymbolicStrategy<XTwoDimensionalCost<XRoboticsSemantics<GR1Context> >,false>::makeInstance),
    OptionCombination("--simpleRecovery --symbolicStrategy --sysInitRoboticsSemantics --twoDimensionalCost",XExtractSymbolicStrategy<XTwoDimensionalCost<XRoboticsSemantics<GR1Context> >,true>::makeInstance)

    // TODO: Combination between BiasForAction and FixedPointRecycling is not supported yet but would make sense
};

/**
 * @brief Prints the help to stderr that the user sees when running "slugs --help" or when supplying
 *        incorrect parameters
 */
void printToolUsageHelp() {
    std::cerr << "Usage of slugs:\n";
    std::cerr << "slugs [options] <FileNames> \n\n";
    std::cerr << "The first input file is supposed to be in 'slugs' format. The others are in the format required by the options used. \n\n";
    for (unsigned int i=0;i<sizeof(commandLineArguments)/sizeof(const char*);i+=2) {
        unsigned int leftStuff = strlen(commandLineArguments[i]);
        std::cerr << commandLineArguments[i] << " ";
        std::istringstream is(commandLineArguments[i+1]);
        unsigned int left = 80-leftStuff-1;
        while (!(is.eof())) {
            std::string next;
            is >> next;
            if (next.size()<left) {
                std::cerr << " " << next;
                left -= next.size() + 1;
            } else {
                left = 80-leftStuff-1;
                std::cerr << "\n";
                for (unsigned int i=0;i<leftStuff+1;i++) std::cerr << " ";
            }
        }
        std::cerr << "\n";
    }
    std::cerr << "\n";
}

/**
 * @brief The main function. Parses arguments from the command line and instantiates a synthesizer object accordingly.
 * @return the error code: >0 means that some error has occured. In case of realizability or unrealizability, a value of 0 is returned.
 */
int main(int argc, const char **args) {
    std::cerr << "SLUGS: SmaLl bUt complete Gr(1) Synthesis tool (see the documentation for an author list).\n";

    std::list<std::string> filenames;
    std::set<std::string> parameters;

    // Parse paramters
    for (int i=1;i<argc;i++) {
        std::string arg = args[i];
        if (arg[0]=='-') {
            bool found = false;
            for (unsigned int i=0;i<sizeof(commandLineArguments)/sizeof(const char*);i+=2) {
                if (commandLineArguments[i] == arg) {
                    found = true;
                }
            }
            if (!found) {
                std::cerr << "Error: Parameter '" << arg << "' is unknown.\n\n";
                printToolUsageHelp();
                return 1;
            }
            parameters.insert(arg);
        } else {
            filenames.push_back(arg);
        }
    }

    // Catch all errors from this point onwards
    try {

        // Prepare list of parameters as string to look up the combination used in the list 'optionCombinations'
        std::ostringstream os;
        bool first = true;
        for (auto it = parameters.begin();it!=parameters.end();it++) {
            if (!first) {
                os << " ";
            } else {
                first = false;
            }
            os << *it;
        }
        std::string totalParameters = os.str();
        for (unsigned int i=0;i<sizeof(optionCombinations)/sizeof(OptionCombination);i++) {
            if (optionCombinations[i].params==totalParameters) {

                // Found the combination - then instantiate context and perform synthesis.
                GR1Context *context = (*(optionCombinations[i].factory))(filenames);
                context->init(filenames);

                // If after the "init" function chain of the context, there are
                // some yet unused file names, then too many have been provided to the
                // user.
                if (filenames.size()>0) {
                    std::cerr << "Error: You provided too many file names!\n";
                    return 1;
                }
                context->computeVariableInformation();
                context->execute();
                delete context;
                return 0;
            } // else { std::cerr << optionCombinations[i].params << ":" << totalParameters << "!\n"; }
        }

        std::cerr << "Error: the chosen option combination is not permissible.\n";
        return 1;

    } catch (const char *error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    } catch (std::string error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    } catch (SlugsException e) {
        std::cerr << "Error: " << e.getMessage() << std::endl;
        if (e.getShouldPrintUsage()) {
            std::cerr << std::endl;
            printToolUsageHelp();
        }
        return 1;
    } catch (BFDumpDotException e) {
        std::cerr << "Error: " << e.getMessage() << std::endl;
        return 1;
    }
}
