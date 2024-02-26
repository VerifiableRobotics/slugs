#ifndef __EXTENSION_EXTRACT_ENVIRONMENT_FRIENDLY_STRATEGY_HPP
#define __EXTENSION_EXTRACT_ENVIRONMENT_FRIENDLY_STRATEGY_HPP

#include "gr1context.hpp"
#include <string>

/**
 *  An extension that modifies the strategy extraction s.t. an environment-friendly strategy is constructed. 
 *  It is based on the 4-nested fixed-point algorithm implemented in the extension ...
 *  This file can only be run after the latter has been executed.
 * 
 *  This extension contains some code by Github user "johnyf", responsible for producing JSON output.
 */
template<class T, bool jsonOutput> class XExtractEnvironmentFriendlyStrategy : public T {
protected:
    // New variables
    std::string outputFilename;

    // Inherited stuff used
    using T::mgr;
    using T::winningPositions;
    using T::initSys;
    using T::initEnv;
    using T::preVars;
    using T::livenessGuarantees;
    using T::livenessAssumptions;
    using T::strategyDumpingDataFourFP;
    using T::strategyDumpingStatesFourFP;
    using T::variables;
    using T::safetyEnv;
    using T::variableTypes;
    using T::realizable;
    using T::postVars;
    using T::varCubePre;
    using T::variableNames;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::determinize;
    using T::doesVariableInheritType;
    
    // Constructor
    XExtractEnvironmentFriendlyStrategy<T,jsonOutput>(std::list<std::string> &filenames): T(filenames) {}

public:

  // The following two functions are used to ensure that the extracted strategy is printed to a file, if a file-name is provided when slugs is called, and printed to the console otherwise.
  
    void init(std::list<std::string> &filenames) {
        T::init(filenames);
        if (filenames.size()==0) {
            outputFilename = "";
        } else {
            outputFilename = filenames.front();
            filenames.pop_front();
        }
    }

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

    
    /**
     * @brief This re-defines the way the strategy is extracted for the 4-nested fixed-point to ensure
     *        that the resulting strategy is indeed "environmental-friendly".
     *        This function requires that the realizability of the specification has already been
     *        detected and that the variables "strategyDumpingDataFourFP", "strategyDumpingStatesFourFP" and
     *        "winningPositions" have been filled by the 4-nested FP algorithm with meaningful data.
     * @param outputStream - Where the strategy shall be printed to.
     */
    void computeAndPrintExplicitStateStrategy(std::ostream &outputStream) {
      
       

        // We don't want any reordering from this point onwards, as
        // the BDD manipulations from this point onwards are 'kind of simple'.
        mgr.setAutomaticOptimisation(false);
	
	
	// We first check which ^abX are non-empty in the k-th iteration over Y within the last iteration over Z. 
	// If they are empty strategyDumpingStatesFourFP[a][k][b] is actually empty. 
	// It should be noted that there is always at least one b for which ^abX is non-empty.
	// These indices are stored in the vector of vectors NonEmptyBs[a][k].
	std::vector<std::vector<std::vector<unsigned int>>> NonEmptyBs(livenessAssumptions.size()); 
	std::vector<std::vector<unsigned int>> helpA;
	std::vector<unsigned int> helpK;
	for (unsigned int a=0;a<livenessGuarantees.size();a++) { 
	  helpA.clear();
	  for (unsigned int k=0;k<strategyDumpingDataFourFP[a].size();k++) {
	    helpK.clear();
	    for (unsigned int b=0;b<strategyDumpingDataFourFP[a][k].size();b++) { 
	      if (strategyDumpingStatesFourFP[a][k][b].size()!=0) {
		helpK.push_back(b);
	      }
	    }
	    helpA.push_back(helpK);
	  }
	  NonEmptyBs[a]=helpA;
	}
	

        // List of states in existance so far. The first map
        // maps from a BF node pointer (for the pre variable valuation) and a goal
        // to a state number. The vector then contains the concrete valuation.
        std::map<std::tuple<size_t, unsigned int, unsigned int, unsigned int, unsigned int>, unsigned int > lookupTableForPastStates;
        std::vector<BF> bfsUsedInTheLookupTable;
        std::list<std::tuple<size_t, unsigned int, unsigned int, unsigned int, unsigned int> > todoList;

        // Prepare initial to-do list from the allowed initial states
        BF todoInit = winningPositions & initSys & initEnv;
	
	// now we determine the rank of every state within the set todoInit to store it along with it for latter extraction of the strategy.
        while (!(todoInit.isFalse())) {
            BF concreteState = determinize(todoInit,preVars); // pick one particular value assignment that is allowed by the bdd todoInit
	    
	    // Check which rank the state assigned to concreteState has by checking strategyDumpingStatesFourFP from top to bottom. 
	    // Note that all states in winningPositions are stored in strategyDumpingStatesFourFP at some point, so these while-loops terminate
	    // assigning concreteState its lowest possible rank. As the algorithm always starts with bh=0, it always checks the sets that avoid
	    // lower indexed assumptions first. This might not be optimal w.r.t. ensuring fastest progress to the goal.
	    unsigned int a=0;
            unsigned int k=0;
	    unsigned int bh=0;
 	    unsigned int b=NonEmptyBs[a][k][bh];
	    unsigned int l=0;
	    
            bool firstTry = true;
            while (((concreteState & strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && a<livenessGuarantees.size()-1) || firstTry) {
	      if (!(firstTry)) { 
		a=(a+1) % livenessGuarantees.size();
	      }
	      k=0;
	      bh=0; 	
	      b=NonEmptyBs[a][k][bh];
	      l=0;
	      while (((concreteState &strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && k<strategyDumpingStatesFourFP[a].size()-1) || firstTry) {
	        if (!(firstTry)) { k++;}
		bh=0; 	
		b=NonEmptyBs[a][k][bh];
		l=0;
                while (((concreteState & strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && bh<NonEmptyBs[a][k].size()-1)  || firstTry){
		  if (!(firstTry)) {
		    bh++;
		    b=NonEmptyBs[a][k][bh];
		   }
		   else {
		     firstTry=false;
		   }
		   l=0;
		   while ((concreteState & strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && l<strategyDumpingStatesFourFP[a][k][b].size()-1){
		     l++;
		   }
		}
	      }
	    }
	    
	    // after we found the correct ranking, we safe it to lookup 
            std::tuple<size_t, unsigned int, unsigned int, unsigned int, unsigned int> lookup = std::tuple<size_t, unsigned int, unsigned int, unsigned int, unsigned int>(concreteState.getHashCode(),a,k,b,l);
	    
	    // we give the currently investigated state a number (0 for the first one) to have a unique identifyer for each state when printing the strategy
            lookupTableForPastStates[lookup] = bfsUsedInTheLookupTable.size();
	    
	    // then we push the BF of the currently investigated state in the list bfsUsedInTheLookupTable, also for printing.
            bfsUsedInTheLookupTable.push_back(concreteState);
	    
	    //delete the state that was handled above from the set todoInit and re-iterate the loop if todoInit is not yet empty,
	    //hence, there exists more states in the queue
            todoInit &= !concreteState; 
	    
	    // add the investigated state allong with its ranking (both stored in lookup to the todoList for the strategy extraction
            todoList.push_back(lookup);
         }
        

        // Print JSON Header if JSON output is desired
        if (jsonOutput) {
            outputStream << "{\"version\": 0,\n \"slugs\": \"0.0.1\",\n\n";

            // print names of variables
            bool first = true;
            outputStream << " \"variables\": [";
            for (unsigned int i=0; i<variables.size(); i++) {
                if (doesVariableInheritType(i, Pre)) {
                    if (first) {
                        first = false;
                    } else {
                        outputStream << ", ";
                    }
                    outputStream << "\"" << variableNames[i] << "\"";
                }
            }
            outputStream << "],\n\n \"nodes\": {\n";
        }
        

        // Extract strategy by investigating one state at the time from the todoList and queue newly encountered post states to the bottom of the list
        while (todoList.size()>0) {
	    // we take the first state from the todoList
            std::tuple<size_t, unsigned int, unsigned int, unsigned int, unsigned int> (current) = todoList.front();
	    // and delete it from the list
            todoList.pop_front();
	    
	    // get the unique identifyer for this state
            unsigned int stateNum = lookupTableForPastStates[current];
	    
	    // get the BF associated with this state
            BF currentState = bfsUsedInTheLookupTable[stateNum];
	   
	    // get the rank of this state
            unsigned int currentA = std::get<1>(current);
            unsigned int currentK = std::get<2>(current);
	    unsigned int currentB = std::get<3>(current);
	    unsigned int currentL = std::get<4>(current);

   
            // Print state information
            if (jsonOutput) {
		outputStream << "\"" << stateNum << "\": {\n\t\"rank\": " << currentA << "," << currentK << ","  << currentB << "," << currentL << ",\n\t\"state\": [";
            } else {
                outputStream << "State " << stateNum << " with rank " << currentA << " -> <";
            }

            bool first = true;
            for (unsigned int i=0;i<variables.size();i++) {
                if (doesVariableInheritType(i,Pre)) {
                    if (first) {
                        first = false;
                    } else {
                        outputStream << ", ";
                    }
                    if (!jsonOutput) outputStream << variableNames[i] << ":";
                    outputStream << (((currentState & variables[i]).isFalse())?"0":"1");
                }
            }
           
            if (jsonOutput) {
                outputStream << "],\n";  // end of state list
//                 // start list of successors
                outputStream << "\t\"trans\": [";
            } else {
                outputStream << ">\n\tWith successors : ";
            }
            first = true;

            // Compute all transitions allowed from currentState by the strategy (safed to strategyDumpingDataFourFP),
	    // while resprecting the rank of currentState and ensuring that only transitions are considered that are safe for the environment
	    //(safety for the system is already ensured before saving transitions to strategyDumpingDataFourFP.
            BF remainingTransitions = currentState & strategyDumpingDataFourFP[currentA][currentK][currentB][currentL] & safetyEnv;

            // investigate possible post-state allowed by remainingTransitions and add it to the queue for strategy extraction
	    while (!(remainingTransitions.isFalse())) {
		
	        // pick one particular post state allowed by remainingTransitions (remember that we have already determinzed the pre-states)
		BF newCombination = determinize(remainingTransitions,postVars);    
		
		// newCombination is one single transition consisting of pre and post states allowed by the strategy. 
		// Hence, by this we have picked a particular system move based on the observed environment move, both specified by the transition.
		// We therefore can remove any other transition within the set remainingTransitions that has the same environment move,
		// as we are fixing one particular sythem move for each environment move only. 
		// This is done by quantifying out all system moves and removing the resulting transition set from remainingTransitions
		BF inputCaptured = newCombination.ExistAbstract(varCubePostOutput);
		remainingTransitions &= !inputCaptured;
		
		// Now we want to add the post state of the investigated transition newCombination to the state queue for strategy extraction. For this, we need to make the post state a pre-state without any particular transition associated with it. This is done by swapping variables after quantifying all pre-states out.
		newCombination = newCombination.ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);

		// additionally, we have to determine the lowest possible rank of the state newCombination (which is now only containing pre-variables),
		// while respecting the rules for the strategy extraction.
		    unsigned int a=currentA;	
		    unsigned int k=currentK;
		    unsigned int b=currentB;
		    unsigned int l=0;
		    
		    // First we consider the third case in the strategy construction. In this case a,k and b need to stay unchanged. 
		    // Hence, we only check in which iteration l over ^abW within the iteration k over ^aY newCombination was added to ^abW. 
		    // There has to exists at least one such l, which is why the loop terminates with the right value.
		    if ((currentK != 0) && (currentL !=0)) {
		      while ((newCombination & strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && l<strategyDumpingStatesFourFP[a][k][b].size()){
			  l++;
			}
		    }
		    else {
		      // In the first case of the strategy construction we update a and have to re-evaluate the ranking of the post states for k,b and l
		      if ((currentK == 0) && (currentL ==0)) { //third case
			a=(a+1) % livenessGuarantees.size();
		      }
		      // Note that the second case is encountered whenever we do not have the first or the third case. 
		      // We therefore do not have to check for it seperately.
		      // In this case, also all value k, b and l might change, but a is left unchanged.
	              
	              // Determine the lowest possible k,b and l values for newCombination.
		      bool firstTry = true;
		      k=0;
		      unsigned int bh=0;
		      b=NonEmptyBs[a][k][bh];
		      l=0;
		      while (((newCombination &strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && k<strategyDumpingStatesFourFP[a].size()-1) || firstTry) {
			    if (!(firstTry)) {
			      k++;
			    }
			    bh=0; 
			    b=NonEmptyBs[a][k][bh];
			    l=0;
			    while (((newCombination & strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && bh<NonEmptyBs[a][k].size()-1)  || firstTry){
			      if (!(firstTry)) {
				bh++;
				b=NonEmptyBs[a][k][bh];
			      }
			      else {
				firstTry=false;
			      }
			      l=0;
			      while ((newCombination & strategyDumpingStatesFourFP[a][k][b][l]).isFalse() && l<strategyDumpingStatesFourFP[a][k][b].size()-1){
				l++;
			      }
			    }
			}
		      }
		    
		    // Now that we have found the pre-state and its ranking of the transition we where investigating, 
		    // we need to check wheather it is already contained in the list and the strategy is only looping back there, or its a new state.
		    
		    unsigned int tn;
		    
		    // we are looking for the state in 'target', which stores the state along with its ranking
		    std::tuple<size_t, unsigned int, unsigned int, unsigned int, unsigned int> target = std::tuple<size_t, unsigned int, unsigned int, unsigned int, unsigned int>(newCombination.getHashCode(),a,k,b,l);
		    
		    if (lookupTableForPastStates.count(target)==0) { 
		         // this state is not yet part of the lookupTable, so we add it to the lookupTable and the todoList
			tn = lookupTableForPastStates[target] = bfsUsedInTheLookupTable.size();
			bfsUsedInTheLookupTable.push_back(newCombination);
			todoList.push_back(target);
		    } else {
		       // the state is already part of the lookupTable
			tn = lookupTableForPastStates[target];
		    }

		    // Print
		    if (first) {
			first = false;
		    } else {
			outputStream << ", ";
		    }

		    outputStream << tn;
		}

            if (jsonOutput) {
                outputStream << "]\n}";
                if (!(todoList.empty())) {
                    outputStream << ",";
                }
                outputStream << "\n\n";
            } else {
                outputStream << "\n";
            }
        }
        if (jsonOutput) {
            // close "nodes" dict and json object
            outputStream << "}}\n";
        }
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XExtractEnvironmentFriendlyStrategy<T,jsonOutput>(filenames);
    }
};

#endif
