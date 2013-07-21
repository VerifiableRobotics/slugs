#ifndef __INCREMENTAL_SYNTHESIS_HPP__
#define __INCREMENTAL_SYNTHESIS_HPP__

#include "gr1context.hpp"
#include <time.h>
#include <boost/algorithm/string.hpp>

/**
 * This extension lets the robot perform incremental synthesis/resynthesis in case of changing specifications.
 * The extension
 */
template<class T> class XIncrementalSynthesis : public T {
protected:
    // Inherit stuff that we actually use here.
    using T::variables;
    using T::variableTypes;
    using T::safetyEnv;
    using T::safetySys;
    using T::determinize;
    using T::mgr;
    using T::variableNames;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::initSys;
    using T::initEnv;

    XIncrementalSynthesis<T>(std::list<std::string> &filenames) : T() {
        if (filenames.size()>0) {
            SlugsException e(true);
            e << "Error when using the incremental synthesis extension. No filename expected - reading from stdin instead!";
            throw e;
        }
        std::cerr << "Warning: The incremental synthesis extension is still under development! Though shall not trust its results at this point in any form.\n";
    }

    /**
     * @brief Starts an interactive shell that allows the user to type in synthesis commands.s
     */
    void execute() {
        std::cout << "Note Incremental synthesis mode active. Reading commands from stdin. Commands available are:\n - Adding input bits and output bits: INPUT <Bitname> and OUTPUT <Bitname>\n";
        std::cout << " - Adding/remove safety/liveness assumption/guarantee. Use the first letter of each case to describe what you want, e.g., RLA for removing liveness assumptions. For removing, add the property name as parameter. For adding, add as parameters first the property name and then the string as used by the slugs input format.";
        std::cout << " - Check realizability: CR\n";
        std::cout << " - Quit: QUIT\n";
        std::cout << "\n";
        while (true) {

            // Read next line
            std::string currentLine;
            std::getline(std::cin,currentLine);
            boost::trim(currentLine);
            if (currentLine.length()>0) {
                size_t positionOfFirstSpace = currentLine.find(" ");
                std::string currentCommand = (positionOfFirstSpace!=std::string::npos)?currentLine.substr(0,positionOfFirstSpace):currentLine;
                boost::to_upper(currentCommand);

                // Switch statement for commands
                if (currentCommand=="QUIT") {
                    return;
                } else {
                    std::cout << "Command: '" << currentCommand << "'\n";
                }



            }
            std::cout << "Time (in s): " << (double)clock()/CLOCKS_PER_SEC << std::endl;
        }
    }

public:
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XIncrementalSynthesis<T>(filenames);
    }

};




#endif
