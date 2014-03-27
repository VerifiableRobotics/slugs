//=====================================================================
// General data structures for GR(1) synthesis
//=====================================================================
#ifndef __GR1CONTEXT_HPP__
#define __GR1CONTEXT_HPP__

#include "BF.h"
#include <set>
#include <list>
#include <vector>
#include "variableTypes.hpp"
#include "variableManager.hpp"

/**
 * @brief Container class for all GR(1) synthesis related activities
 *        Modifications of the GR(1) synthesis algorithm
 *        (or strategy extraction) should derive from this class, as it
 *        provides input parsing and BF/BDD book-keeping.
 */
class GR1Context : public SlugsVariableManager {
protected:

    //@{
    /** @name BF-related stuff that is computed in the constructor, i.e., with loading the input specification
     */
    std::vector<BF> livenessAssumptions;
    std::vector<BF> livenessGuarantees;
    BF initEnv;
    BF initSys;
    BF safetyEnv;
    BF safetySys;
    SlugsVarVector varVectorPre{PreInput, PreOutput, this};
    SlugsVarVector varVectorPost{PostInput, PostOutput, this};
    SlugsVarCube varCubePostInput{PostInput,this};
    SlugsVarCube varCubePostOutput{PostOutput,this};
    SlugsVarCube varCubePreInput{PreInput,this};
    SlugsVarCube varCubePreOutput{PreOutput,this};
    SlugsVarCube varCubePre{PreOutput,PreInput,this};
    SlugsVarCube varCubePost{PostOutput,PostInput,this};
    SlugsVectorOfVarBFs preVars{Pre, this};
    SlugsVectorOfVarBFs postVars{Post, this};
    SlugsVectorOfVarBFs postInputVars{PostInput, this};
    SlugsVectorOfVarBFs postOutputVars{PostOutput, this};
    //@}

    //@{
    /** @name Information that is computed during realizability checking
     *  The following variables are set by the realizability checking part. The first one
     *  contains information to be used during strategy extraction - it prodives a sequence
     *  of BFs/BDDs that represent transitions in the game that shall be preferred over those
     *  that come later in the vector. The 'unsigned int' data type in the vector represents the goal
     *  that a BF refers to. The winningPositions BF represents which positions are winning for the system player.
     */
    std::vector<std::pair<unsigned int,BF> > strategyDumpingData;
    bool realizable;
    BF winningPositions;
    //@}

    //! This variable is only used during parsing the input instance.
    //! It allows us to get better error messages for parsing.
    unsigned int lineNumberCurrentlyRead;

    //@{
    /**
     * @name Internal functions - these are used during parsing an input instance
     */
    BF parseBooleanFormulaRecurse(std::istringstream &is,std::set<VariableType> &allowedTypes, std::vector<BF> &memory);
    BF parseBooleanFormula(std::string currentLine,std::set<VariableType> &allowedTypes);
    //@}

    //! A protected default constructor - to be used if input parsing is to be performed by
    //! the plugin, which must know what it is doing then!
    GR1Context() {}

public:
    GR1Context(std::list<std::string> &filenames);
    virtual ~GR1Context() {}
    virtual void computeWinningPositions();
    virtual void checkRealizability();
    virtual void execute();
    static BF determinize(BF in, std::vector<BF> vars);
    static BF determinizeRandomized(BF in, std::vector<BF> vars);
    virtual void init(std::list<std::string> &filenames);
    
    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new GR1Context(filenames);
    }

};


/**
 * @brief Helper class for easier BF-based fixed point computation
 */
class BFFixedPoint {
private:
    BF currentValue;
    bool reachedFixedPoint;
public:
    BFFixedPoint(BF init) : currentValue(init), reachedFixedPoint(false) {}
    void update(BF newOne) {
        if (currentValue == newOne) {
            reachedFixedPoint = true;
        } else {
            currentValue = newOne;
        }
    }
    bool isFixedPointReached() const { return reachedFixedPoint; }
    BF getValue() { return currentValue; }
};

/**
 * @brief A customized class for Exceptions in Slugs - Can trigger printing the comman line parameters of Slugs
 */
class SlugsException {
private:
    std::ostringstream message;
    bool shouldPrintUsage;
public:
    SlugsException(bool _shouldPrintUsage) : shouldPrintUsage(_shouldPrintUsage) {}
    SlugsException(bool _shouldPrintUsage, std::string msg) : shouldPrintUsage(_shouldPrintUsage) { message << msg; }
    SlugsException(SlugsException const &other) : shouldPrintUsage(other.shouldPrintUsage) { message << other.message.str();}
    bool getShouldPrintUsage() const { return shouldPrintUsage; }
    SlugsException& operator<<(const std::string str) { message << str; return *this; }
    SlugsException& operator<<(const double value) { message << value; return *this; }
    SlugsException& operator<<(const int value) { message << value; return *this; }
    SlugsException& operator<<(const unsigned int value) { message << value; return *this; }
    std::string getMessage() { return message.str(); }
};


#endif
