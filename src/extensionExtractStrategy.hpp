#ifndef __EXTENSION_EXTRACT_STRATEGY_HPP
#define __EXTENSION_EXTRACT_STRATEGY_HPP

#include "gr1context.hpp"
#include <string>

template<class T> class XExtractStrategy : public T {
protected:
    // New variables
    std::string outputFilename;

    // Inherited stuff used
    using T::realizable;
    using T::computeAndPrintExplicitStateStrategy;

    XExtractStrategy<T>(std::list<std::string> &filenames) : T(filenames) {
        if (filenames.size()==0) {
            outputFilename = "";
        } else {
            outputFilename = filenames.front();
            filenames.pop_front();
        }
    }

public:

    void execute() {
        T::execute();
        if (realizable) {
            if (outputFilename=="") {
                computeAndPrintExplicitStateStrategy(std::cout);
            } else {
                std::ofstream of(outputFilename.c_str());
                if (of.fail()) {
                    SlugsException ex(false);
                    ex << "Error: Could not open output file'" << outputFilename << "\n";
                    throw ex;
                }
                computeAndPrintExplicitStateStrategy(of);
                if (of.fail()) {
                    SlugsException ex(false);
                    ex << "Error: Writing to output file'" << outputFilename << "failed. \n";
                    throw ex;
                }
                of.close();
            }
        }
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XExtractStrategy<T>(filenames);
    }
};

#endif
