#ifndef __EXTENSION_COMPUTE_CNF_FORM_OF_THE_SPECIFICATION_HPP
#define __EXTENSION_COMPUTE_CNF_FORM_OF_THE_SPECIFICATION_HPP

#include "gr1context.hpp"
#include <map>

/**
 * This extension lets the Synthesis tool actually skip the synthesis step, and only compute
 * a version of the specification that can be used by other synthesis tools that require it to be CNF-based.
 */
template<class T> class XComputeCNFFormOfTheSpecification : public T {
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

    XComputeCNFFormOfTheSpecification<T>(std::list<std::string> &filenames) : T(filenames) {}
public:

    static int normsign(int x) {
        if (x<0) return -1;
        if (x>0) return 1;
        throw "Illegal value for function 'normsign'";
    }

    void execute() {

        // Variable lists
        for (unsigned int i=0;i<variableNames.size();i++) {
            if (i==variableNames.size()-1) throw "Error: The ComputeCNFFormOfTheSpecification extension assumes that pre- and post-variabels are assigned alternatingly (error case 1).\n";
            if (variableTypes[i]==PreInput) {
                std::cout << "Input: " << variableNames[i] << " " << i+1 << " " << i+2 << "\n";
                if (variableTypes[i+1]!=PostInput) throw "Error: The ComputeCNFFormOfTheSpecification extension assumes that pre- and post-variabels are assigned alternatingly (error case 2).\n";
                if (variableNames[i+1]!=variableNames[i]+"'") throw "Error: The ComputeCNFFormOfTheSpecification extension assumes that post-variables have a primed name (error case 1).";
                i++;
            } else if (variableTypes[i]==PreOutput) {
                std::cout << "Output: " << variableNames[i] << " " << i+1 << " " << i+2 << "\n";
                if (variableTypes[i+1]!=PostOutput) throw "Error: The ComputeCNFFormOfTheSpecification extension assumes that pre- and post-variabels are assigned alternatingly (error case 3).\n";
                if (variableNames[i+1]!=variableNames[i]+"'") throw "Error: The ComputeCNFFormOfTheSpecification extension assumes that post-variables have a primed name (error case 2).";
                i++;
            } else {
                throw "Error: The ComputeCNFFormOfTheSpecification extension assumes that pre- and post-variabels are assigned alternatingly (error case 4).\n";
            }
        }

        // Work on the safety assumptions, safety guarantees, and initialization constraints
        for (unsigned int type=0;type<3;type++) {
            BF rest = type==0?safetySys:(type==1?safetyEnv:(initEnv&initSys));
            BF found = mgr.constantFalse();
            std::string typeString = type==0?"SafetyGuarantee: ":(type==1?"SafetyAssumption: ":"Init: ");
            while ((rest | found)!=mgr.constantTrue()) {
                BF thisCube = determinize((!rest) & (!found), variables);
                std::cout << typeString;
                for (unsigned int i=0;i<variables.size();i++) {
                    BF candidate = thisCube.ExistAbstractSingleVar(variables[i]);
                    if ((candidate & rest & !found).isFalse()) {
                        thisCube = candidate;
                    } else if ((thisCube & variables[i]).isFalse()) {
                        std::cout << i+1 << " ";
                    } else {
                        std::cout << -(int)i-(int)1 << " ";
                    }
                }
                std::cout << "0\n";
                found |= thisCube;
            }
            if (found != !rest) throw "Internal error (did not compute the correct CNF)";
        }

        // Operate on the Liveness conditions
        // Translate all clauses that only concern post variables to only pre-variables along the way
        for (unsigned int type=0;type<2;type++) {
            std::vector<BF> &rest = type==0?livenessGuarantees:livenessAssumptions;
            std::string typeString = type==0?"LivenessGuarantee:":"LivenessAssumption:";
            for (auto it = rest.begin();it!=rest.end();it++) {
                BF found = mgr.constantFalse();
                std::cout << typeString;
                while ((*it & !found)!=mgr.constantFalse()) {
                    std::cout << " ";
                    std::vector<int> literals;
                    bool allPostSoFar = true;
                    BF thisCube = determinize((*it) & (!found), variables);
                    for (unsigned int i=0;i<variables.size();i++) {
                        BF candidate = thisCube.ExistAbstractSingleVar(variables[i]);
                        if ((candidate & !*it).isFalse()) {
                            thisCube = candidate;
                        } else if ((thisCube & variables[i]).isFalse()) {
                            literals.push_back(-(int)i-1);
                            allPostSoFar &= (variableTypes[i]==PostInput) || (variableTypes[i]==PostOutput);
                        } else {
                            literals.push_back(i+1);
                            allPostSoFar &= (variableTypes[i]==PostInput) || (variableTypes[i]==PostOutput);
                        }
                    }
                    found |= thisCube;
                    // Print the cube
                    //std::cout << "P:" << allPostSoFar << " ";
                    for (auto it2=literals.begin();it2!=literals.end();it2++) {
                        std::cout << (allPostSoFar?(*it2)-normsign(*it2):*it2) << " ";
                    }
                    std::cout << "0";
                    std::cout.flush();
                }
                if (found != *it) throw "Internal error (did not compute the correct CNF, case 2)";
                std::cout << "\n";
            }
        }
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XComputeCNFFormOfTheSpecification<T>(filenames);
    }
};

#endif
