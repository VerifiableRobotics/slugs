#ifndef __EXTENSION_ABSTRACT_WINNING_TRACE_GENERATOR_HPP
#define __EXTENSION_ABSTRACT_WINNING_TRACE_GENERATOR_HPP

#include "gr1context.hpp"

/**
 * In case a
 *
 */
template<class T> class XAbstractWinningTraceGenerator : public T {
protected:
    using T::initSys;
    using T::initEnv;
    using T::safetyEnv;
    using T::safetySys;
    using T::mgr;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::varCubePostInput;
    using T::varCubePreOutput;
    using T::varCubePreInput;
    using T::preVars;
    using T::determinize;
    using T::variables;
    using T::variableNames;
    using T::doesVariableInheritType;
    using T::postInputVars;

    SlugsVarCube varCubePre{PreInput,PreOutput,this};

    const std::string STRING_FOR_TABLE_LINE{"\x01"};

    // Variables used for analyzing "structured slugs variable grouping"
    std::map<std::string,StructuredSlugsVariableGroup> variableGroups;
    std::map<unsigned int, std::string> masterVariables;
    std::set<unsigned int> variablesInSomeGroup;

    // Constructor
    XAbstractWinningTraceGenerator<T>(std::list<std::string> &filenames) : T(filenames) {}

    // A generic function for printing a table. The indexing of the table is first-column-then-row.
    // If a vertical line is to be added, a column consisting only of an STRING_FOR_TABLE_LINE element
    // is to be used. For horizontal lines, all non-vertical-line columns should have a STRING_FOR_TABLE_LINE
    // element at the same position.
    void printTable(std::vector<std::vector<std::string> > const &table) {

        // Obtain column sizes
        std::vector<unsigned int> columnSizes;
        size_t nofLines = 0;
        for (auto it = table.begin();it!=table.end();it++) {
            size_t maximum = 0;
            nofLines = std::max(nofLines,it->size());
            for (auto it2 = it->begin();it2!=it->end();it2++) {
                maximum = std::max(maximum,it2->length());
            }
            columnSizes.push_back(maximum);
        }

        // The lines
        for (unsigned int line=0;line<nofLines;line++) {

            // Check if this is a horizontal line
            bool isHline = false;
            unsigned int searchCol = 0;
            while (searchCol<table.size()) {
                // std::cerr << "S: " << searchCol << "," << line << " , " << table[searchCol].size() << ": " << table[searchCol][0] << std::endl;
                if ((table[searchCol].size()==1) && (table[searchCol][0]==STRING_FOR_TABLE_LINE)) {
                    searchCol++;
                } else {
                    if (table[searchCol][line]==STRING_FOR_TABLE_LINE) {
                        isHline = true;
                    }
                    searchCol = table.size();
                }
            }

            // Now go through line
            if (isHline) {
                for (unsigned int column=0;column<table.size();column++) {
                    if ((table[column].size()==1) && (table[column][0]==STRING_FOR_TABLE_LINE)) {
                        std::cout << "+";
                    } else {
                        std::cout << "--";
                        for (unsigned int i=0;i<columnSizes[column];i++) std::cout << "-";
                    }
                }
            } else {
                for (unsigned int column=0;column<table.size();column++) {
                    if ((table[column].size()==1) && (table[column][0]==STRING_FOR_TABLE_LINE)) {
                        std::cout << "|";
                    } else {
                        std::cout << " ";
                        // std::cerr << "U: " << column << "," << line << " , " << table[column].size() << std::endl;
                        std::cout << table[column][line];
                        for (unsigned int i=0;i<columnSizes[column]-table[column][line].length();i++) std::cout << " ";
                        std::cout << " ";
                    }
                }
            }
            std::cout << "\n";
        }
    }


    void computeAbstractStrategyTrace(bool environmentShouldWin) {

        BF winningPositions = mgr.constantFalse();
        unsigned int round = 0;
        int roundFoundInitPos = -1;
        BF oldWinningPositions = !winningPositions;
        std::vector<BF> distances;
        if (environmentShouldWin) {
            // Losing for the system - the environment can try to falsify now.
            while ((winningPositions!=oldWinningPositions) && (roundFoundInitPos==-1)) {
                distances.push_back(winningPositions);
                oldWinningPositions = winningPositions;
                winningPositions |= (safetyEnv & (winningPositions.SwapVariables(varVectorPre,varVectorPost) | !safetySys).UnivAbstract(varCubePostOutput)).ExistAbstract(varCubePostInput);
                round++;
                if ((roundFoundInitPos==-1) &&
                        ((!initSys) | (initEnv & winningPositions)).UnivAbstract(varCubePreOutput).ExistAbstract(varCubePreInput).isTrue()) {
                    roundFoundInitPos = round;
                }
            }
        } else {
            // Losing for the environment - the system can try to falsify now.
            while ((winningPositions!=oldWinningPositions) && (roundFoundInitPos==-1)) {
                distances.push_back(winningPositions);
                oldWinningPositions = winningPositions;
                winningPositions |= (safetyEnv.Implies((safetySys & winningPositions.SwapVariables(varVectorPre,varVectorPost))).ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                if (winningPositions.isFalse()) std::cerr << "Still nothing!\n";
                round++;
                if ((roundFoundInitPos==-1) &&
                        initEnv.Implies(initSys & winningPositions).ExistAbstract(varCubePreOutput).UnivAbstract(varCubePreInput).isTrue()) {
                    roundFoundInitPos = round;
                }
            }
        }
        BF_newDumpDot(*this,winningPositions,NULL,"/tmp/winningPos.dot");
        // std::cerr << "RoundWinningPos: " << roundFoundInitPos << std::endl;

        // Debugging output
        //for (unsigned int i=0;i<distances.size();i++) {
        //    std::ostringstream fn;
        //    fn << "/tmp/distancesUnoptimized" << i << ".dot";
        //    BF_newDumpDot(*this,distances[i],NULL,fn.str());
        // }

        // Compute abstract trace that is as concrete as possible!
        if (roundFoundInitPos>-1) {
            BF startingState = initEnv & initSys & winningPositions;
            if (!(startingState.isFalse())) {
                startingState = determinize(startingState,preVars);
                distances.push_back(startingState);

                // Try to generalize
                // In every step, check if we can restrict our attention to a fixed value for a variable
                // without destroying the fact that from every *previous* state, we can guarantee to either win
                // in the next transition, or land in a state that is still within our restriction.
                //
                // Do multiple passes in order to make the result as strict as possible
                bool madeChange = true;
                while (madeChange) {
                    madeChange = false;
                    // std::cerr << "Iterating!\n";
                    for (int i=distances.size()-2;i>=0;i--) {
                        // std::cerr << "i: " << i << std::endl;
                        // Try for every variable to restrict it to one value
                        for (unsigned int j=0;j<preVars.size();j++) {
                            // Iterate over the possible values
                            bool foundARestriction = false;
                            for (unsigned int mycase=0;(mycase<2) & !foundARestriction;mycase++) {

                                BF candidatePost = distances[i];
                                BF candidatePre = distances[i+1];

                                if (mycase==0) {
                                    candidatePost &= preVars[j];
                                } else if (mycase==1) {
                                    candidatePost &= !preVars[j];
                                } else {
                                    throw "Should not happen!";
                                }

                                BF newPre;
                                if (environmentShouldWin) {
                                    newPre = (safetyEnv & (candidatePost.SwapVariables(varVectorPre,varVectorPost) | !safetySys).UnivAbstract(varCubePostOutput)).ExistAbstract(varCubePostInput);
                                } else {
                                    newPre = (safetyEnv.Implies((safetySys & candidatePost.SwapVariables(varVectorPre,varVectorPost))).ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                                }
                                if ((newPre >= candidatePre) && (candidatePost != distances[i])) {
                                    madeChange = true;
                                    distances[i] = candidatePost;
                                    // std::cerr << "Made change!\n";
                                }
                            }
                        }
                    }
                }
            } else {
                std::cout << "There is no strategy for the ";
                if (environmentShouldWin) {
                    std::cout << "environment";
                } else {
                    std::cout << "system";
                }
                std::cout << " environment to win in finite time from an initial position.\n\n";
                return;
            }
        } else {
            return; // Can't win because the other player is winning.
        }

        // Replace the last element in case of an environment-winning strategy -- we should have the last input here then,
        // provided that it is well-defined.
        if ((distances.size()>1) && (environmentShouldWin)) {
           distances[0] = determinize(((safetyEnv & !safetySys) | !distances[1]).UnivAbstract(varCubePostOutput).UnivAbstract(varCubePre),postInputVars).SwapVariables(varVectorPre,varVectorPost);
        }

        // Debugging output
        // for (unsigned int i=0;i<distances.size();i++) {
        //     std::ostringstream fn;
        //    fn << "/tmp/distancesOptimized" << i << ".dot";
        //    BF_newDumpDot(*this,distances[i],NULL,fn.str());
        // }


        // Make and print table (indexing: first column, then row)
        std::vector<std::vector<std::string> > tableCells;
        tableCells.push_back(std::vector<std::string>()); tableCells.back().push_back(STRING_FOR_TABLE_LINE);
        tableCells.push_back(std::vector<std::string>());

        // Make first column
        std::vector<std::string> &firstCol = tableCells.back();
        firstCol.push_back(STRING_FOR_TABLE_LINE); // Horizontal line
        firstCol.push_back("Atomic prop./Round");
        firstCol.push_back(STRING_FOR_TABLE_LINE); // Horizontal line
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,PreInput)) {
                if (variablesInSomeGroup.count(i)==0) {
                    firstCol.push_back(variableNames[i]);
                }
            }
        }
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,PreInput)) {
                if (variablesInSomeGroup.count(i)!=0) {
                    auto it = masterVariables.find(i);
                    if (it!=masterVariables.end()) {
                        firstCol.push_back(it->second);
                        // std::cerr << "V: " << it->second << std::endl;
                    }
                }
            }
        }
        firstCol.push_back(STRING_FOR_TABLE_LINE); // Horizontal line
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,PreOutput)) {
                if (variablesInSomeGroup.count(i)==0) {
                    firstCol.push_back(variableNames[i]);
                }
            }
        }
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,PreOutput)) {
                if (variablesInSomeGroup.count(i)!=0) {
                    auto it = masterVariables.find(i);
                    if (it!=masterVariables.end()) {
                        firstCol.push_back(it->second);
                        // std::cerr << "V: " << it->second << std::endl;
                    }
                }
            }
        }
        firstCol.push_back(STRING_FOR_TABLE_LINE); // Horizontal line
        // std::cerr << "UX: " << firstCol.size() << std::endl;

        // Make Double-vertical lines
        tableCells.push_back(std::vector<std::string>()); tableCells.back().push_back(STRING_FOR_TABLE_LINE);
        tableCells.push_back(std::vector<std::string>()); tableCells.back().push_back(STRING_FOR_TABLE_LINE);
        for (int i=distances.size()-1;i>=0;i--) {

            // Data column
            tableCells.push_back(std::vector<std::string>());
            std::vector<std::string> &nextCol = tableCells.back();
            nextCol.push_back(STRING_FOR_TABLE_LINE); // Horizontal line

            // Round number
            std::ostringstream os;
            os << distances.size()-i-1;
            nextCol.push_back(os.str());
            nextCol.push_back(STRING_FOR_TABLE_LINE);

            // Data bits
            // Input ungrouped
            for (unsigned int j=0;j<variables.size();j++) {
                if (doesVariableInheritType(j,PreInput)) {
                    if (variablesInSomeGroup.count(j)==0) {
                        if (distances[i].isFalse()) {
                            nextCol.push_back("X");
                        } else if ((distances[i] & variables[j]).isFalse()) {
                            nextCol.push_back("0");
                        } else if ((distances[i] & !variables[j]).isFalse()) {
                            nextCol.push_back("1");
                        } else {
                            nextCol.push_back("*");
                        }
                    }
                }
            }

            // Input Grouped
            for (unsigned int j=0;j<variables.size();j++) {
                if (doesVariableInheritType(j,PreInput)) {
                    if (variablesInSomeGroup.count(j)!=0) {
                        auto it = masterVariables.find(j);
                        if (it!=masterVariables.end()) {
                            if (distances[i].isFalse()) {
                                nextCol.push_back("X");
                            } else {
                                bool isAbstract = false;
                                std::string key = it->second;
                                StructuredSlugsVariableGroup &group = variableGroups[key];
                                unsigned int value = 0;
                                if ((distances[i] & !variables[group.masterVariable]).isFalse()) {
                                    value += 1;
                                } else if (!((distances[i] & variables[group.masterVariable]).isFalse())) {
                                    isAbstract = true;
                                }
                                unsigned int multiplier=2;
                                for (auto it = group.slaveVariables.begin();it!=group.slaveVariables.end();it++) {
                                    if ((distances[i] & !variables[*it]).isFalse()) {
                                        value += multiplier;
                                    } else if (!((distances[i] & variables[*it]).isFalse())) {
                                        isAbstract = true;
                                    }
                                    multiplier *= 2;
                                }
                                if (isAbstract) {
                                    nextCol.push_back("*");
                                } else {
                                    std::ostringstream valueStream;
                                    valueStream << group.minValue+value;
                                    nextCol.push_back(valueStream.str());
                                }
                            }
                        }
                    }
                }
            }

            nextCol.push_back(STRING_FOR_TABLE_LINE); // Horizontal line


            // Output ungrouped
            for (unsigned int j=0;j<variables.size();j++) {
                if (doesVariableInheritType(j,PreOutput)) {
                    if (variablesInSomeGroup.count(j)==0) {
                        if (distances[i].isFalse()) {
                            nextCol.push_back("X");
                        } else if ((distances[i] & variables[j]).isFalse()) {
                            nextCol.push_back("0");
                        } else if ((distances[i] & !variables[j]).isFalse()) {
                            nextCol.push_back("1");
                        } else {
                            nextCol.push_back("*");
                        }
                    }
                }
            }

            // Output Grouped
            for (unsigned int j=0;j<variables.size();j++) {
                if (doesVariableInheritType(j,PreOutput)) {
                    if (variablesInSomeGroup.count(j)!=0) {
                        auto it = masterVariables.find(j);
                        if (it!=masterVariables.end()) {
                            if (distances[i].isFalse()) {
                                nextCol.push_back("X");
                            } else {
                                bool isAbstract = false;
                                std::string key = it->second;
                                StructuredSlugsVariableGroup &group = variableGroups[key];
                                unsigned int value = 0;
                                if ((distances[i] & !variables[group.masterVariable]).isFalse()) {
                                    value += 1;
                                } else if (!((distances[i] & variables[group.masterVariable]).isFalse())) {
                                    isAbstract = true;
                                }
                                unsigned int multiplier=2;
                                for (auto it = group.slaveVariables.begin();it!=group.slaveVariables.end();it++) {
                                    if ((distances[i] & !variables[*it]).isFalse()) {
                                        value += multiplier;
                                    } else if (!((distances[i] & variables[*it]).isFalse())) {
                                        isAbstract = true;
                                    }
                                    multiplier *= 2;
                                }
                                if (isAbstract) {
                                    nextCol.push_back("*");
                                } else {
                                    std::ostringstream valueStream;
                                    valueStream << group.minValue+value;
                                    nextCol.push_back(valueStream.str());
                                }
                            }
                        }
                    }
                }
            }

            nextCol.push_back(STRING_FOR_TABLE_LINE); // Horizontal line

            // Spacer!
            tableCells.push_back(std::vector<std::string>()); tableCells.back().push_back(STRING_FOR_TABLE_LINE);
        }

        if (environmentShouldWin)
            std::cout << "Abstract countertrace:\n";
        else
            std::cout << "Abstract trace:\n";
        printTable(tableCells);
        std::cout << "\n";
    }


public:

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XAbstractWinningTraceGenerator<T>(filenames);
    }

    void execute() {

        //========================
        // Compute variable groups
        //========================
        // Interpret grouped variables (the ones with an '@' in their names)
        // It is assumed that they have precisely the shape produced by the structured
        // slugs parser. This is checked, however, and an exception is thrown otherwise.
        for (unsigned int i=0;i<variables.size();i++) {
            if (doesVariableInheritType(i,Pre)) {
                size_t firstAt = variableNames[i].find_first_of('@');
                if (firstAt!=std::string::npos) {
                    if (firstAt<variableNames[i].size()-1) {
                        std::string prefix = variableNames[i].substr(0,firstAt);
                        // std::cerr << variableNames[i] << std::endl;
                        // There is a latter after the '@'
                        // Now check if this is the master variabe
                        if ((firstAt<variableNames[i].size()-2) && (variableNames[i][firstAt+1]=='0') && (variableNames[i][firstAt+2]=='.')) {
                            // Master variable found. Read the information from the master variable
                            variableGroups[prefix].setMasterVariable(i);
                            if (variableGroups[prefix].slaveVariables.size()>0) throw "Error: in the variable groups introduced by the structured slugs parser, a master variable must be listed before its slaves.";
                            // Parse min/max information
                            std::istringstream is(variableNames[i].substr(firstAt+3,std::string::npos));
                            is >> variableGroups[prefix].minValue;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 1)";
                            char sep;
                            is >> sep;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 2)";
                            is >> variableGroups[prefix].maxValue;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 3)";
                            masterVariables[i] = prefix;
                        } else {
                            // Verify that slave variables are defined in the correct order
                            variableGroups[prefix].slaveVariables.push_back(i);
                            std::istringstream is(variableNames[i].substr(firstAt+1,std::string::npos));
                            unsigned int number;
                            is >> number;
                            if (is.fail()) throw std::string("Error parsing bound-encoded variable name ")+variableNames[i]+" (reason 4)";
                            if (number!=variableGroups[prefix].slaveVariables.size()) throw std::string("Error: in the variable groups introduced by the structured slugs parser, the slave variables are not in the right order: ")+variableNames[i];
                        }
                    }
                    variablesInSomeGroup.insert(i);
                }
            }
        }

        // Now the real work can be done.
        computeAbstractStrategyTrace(true);
        computeAbstractStrategyTrace(false);
    }

};



#endif
