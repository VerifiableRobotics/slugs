#include <fstream>
#include <cstring>
#include "extensionExtractStrategy.hpp"
#include "extensionRoboticsSemantics.hpp"
#include "extensionWeakenSafetyAssumptions.hpp"
#undef fail

//===================================================================================
// List of command line arguments
//===================================================================================
const char *commandLineArguments[] = {
    "--onlyRealizability","Use this parameter if no synthesized system should be computed, but only the realizability/unrealizability result is to be computed. If this option is *not* given, an automaton is computed. In case of realizability, the synthesized controller is printed to an output file name if it is given, and to stdout otherwise.",
    "--sysInitRoboticsSemantics","In standard GR(1) synthesis, a specification is called realizable if for every initial input proposition valuation that is allowed by the initialization contraints, there is some suitable output proposition valuation. In the modified semantics for robotics applications, the controller has to be fine with any admissible initial position in the game.",
    "--computeWeakenedSafetyAssumptions","Extract a minimal conjunctive normal form Boolean formula that represents some minimal CNF for a set of safety assumptions that leads to realiazability and is a weakened form of the safety assumptions given. Requires the option '--onlyRealizability' to be given as well."
};

//===================================================================================
// List of combinations allowed
//
// -> Parameters need to be ordered lexicographically
// -> File names will be read from the inside-out. "XExtractStrategy" must therefore
//    be outermost, as it represents an *optional* parameter
//===================================================================================
struct OptionCombination { std::string params; GR1Context* (*factory)(std::list<std::string> &l); OptionCombination(std::string _p, GR1Context* (*_f)(std::list<std::string> &l)) : params(_p), factory(_f) {} };
OptionCombination optionCombinations[] = {
    OptionCombination("",XExtractStrategy<GR1Context>::makeInstance),
    OptionCombination("--onlyRealizability",GR1Context::makeInstance),
    OptionCombination("--sysInitRoboticsSemantics",XExtractStrategy<XRoboticsSemantics<GR1Context> >::makeInstance),
    OptionCombination("--onlyRealizability --sysInitRoboticsSemantics",XRoboticsSemantics<GR1Context>::makeInstance),
    OptionCombination("--computeWeakenedSafetyAssumptions --onlyRealizability",XComputeWeakenedSafetyAssumptions<GR1Context>::makeInstance),
    OptionCombination("--computeWeakenedSafetyAssumptions --onlyRealizability --sysInitRoboticsSemantics",XComputeWeakenedSafetyAssumptions<XRoboticsSemantics<GR1Context> >::makeInstance)
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

    // Try to run
    try {

        // Prepare context
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
                GR1Context *context = (*(optionCombinations[i].factory))(filenames);
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
    } catch (SlugsException e) {
        std::cerr << "Error: " << e.getShouldPrintUsage() << std::endl;
        return 1;
    }

    return 0;
}
