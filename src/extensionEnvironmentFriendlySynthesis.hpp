#ifndef __EXTENSION_ENVIRONMENT_FRIENDLY_SYNTHESIS_HPP
#define __EXTENSION_ENVIRONMENT_FRIENDLY_SYNTHESIS_HPP

#include "gr1context.hpp"
#include <string>

/**
 * A class that modifies the core implementation of the 3-nested fixed-point algorithm to the 4-nested fixed-point algorithm ensuring that environment assumptions are not falsified, i.e., the resulting strategy is "environmental-friendly" 
 * If UseHeuristic is set to true, a sound but not complete heuristic is used that only investigates b=a.
 */
template<class T, bool UseHeuristic> class XEnvironmentFriendlySynthesis : public T {
protected:
    // New variables 
        //new data structures to store transitions (Data) and states (States) for strategy extraction
        std::vector<std::vector<std::vector<std::vector<BF>>>> strategyDumpingDataFourFP;
        std::vector<std::vector<std::vector<std::vector<BF>>>> strategyDumpingStatesFourFP;
    
    // Inherited stuff used
        using T::mgr;
        using T::livenessGuarantees;
        using T::livenessAssumptions;
        using T::varVectorPre;
        using T::varVectorPost;
        using T::varCubePostOutput;
        using T::varCubePostInput;
        using T::varCubePost;
        using T::safetyEnv;
        using T::safetySys;
        using T::winningPositions;
        using T::variables;
        using T::determinize;
        using T::doesVariableInheritType;

    // Constructor
        XEnvironmentFriendlySynthesis<T,UseHeuristic>(std::list<std::string> &filenames) : T(filenames) {}

public:
    
    /**
    * @brief A print function used for debugging this file.
    * It that takes a BF "in" and prints all concrete variable valuation in "in", 
    * either for the full transition (inclusing pre and post vars)
    * or only for the (pre) set only printing PreVars.
    * @param in a BF to be analized
    * @param printPre print pre-values if true
    * @param printPost print post-values if true
    */    
        
        
    void PrintSet(BF in, bool printPre, bool  printPost) {
        
        //define that everything is printed to the console  
        std::ostream &outputStream = std::cout;
        
        while (!(in.isFalse())) {
            
            // pick a particular valuation to all variables (pre and post) in 'in'
            BF concreteState = determinize(in,variables);
            
            // print pre-values of concreteState
            if (printPre){
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i, Pre)) {
                        outputStream << (((concreteState & variables[i]).isFalse())?"0":"1") << ", ";
                    }
                }
            }
            
            // print post-values of concreteState
            if (printPost){
                outputStream << "\t ";
                for (unsigned int i=0;i<variables.size();i++) {
                    if (doesVariableInheritType(i, Post)) {
                        outputStream << (((concreteState & variables[i]).isFalse())?"0":"1") << ", ";
                    }
                }
            }
            
            outputStream << "\n";
            
            //update the queue
            in &= !concreteState;
        }
    }

    

    /**
     * @brief A function overwriting the way winning positions are computed by computing the result of the new 4-nested fixed-point algorithm which ensures non-conflictingness w.r.t. the environment assumptions. 
     To enable debugging, we add an input parameter specifying that we print to the console. 
     */
    
     void computeWinningPositions() {
                  
        // NOT USED: std::ostream &outputStream = std::cout; //only used for debugging, defines that everything is printed to the console
	    
	//  vectors to store intermediate results of the fixed-point computations used to construct strategyDumpingDataFourFP
	std::vector<std::vector<std::vector<BF>>> subvectorI;
	std::vector<BF> subvectorJ;
	//  vectors to store intermediate results of the fixed-point computations used to construct strategyDumpingStatesFourFP       
	std::vector<std::vector<std::vector<BF>>> subvectorIs;
	std::vector<BF> subvectorJs;

	// (1) The greatest fixed point - called "Z" in the paper
	BFFixedPoint nu2(mgr.constantTrue()); // initializing Z to the full state set

// 	unsigned int kh=0; //only used for debugging to specify for which iteration sets should be printed
	
	// Iterate until we have found a fixed point
	for (;!nu2.isFixedPointReached();) {

	    // To extract a strategy in case of realizability, we need to store a sequence of 'preferred' transitions and a
	    // ranking over the winning states. These only need to be computed during the last execution of the outermost
	    // greatest fixed point. Since we don't know which one is the last one, we store them in every iteration,
	    // so that after the last iteration, we obtained the necessary data. Before any new iteration, we need to
	    // clear the old data, though.
	    strategyDumpingDataFourFP.clear();
	    strategyDumpingStatesFourFP.clear();

	    // (2) Iterate over all of the liveness guarantees. 
	    // Put the results into the variable 'nextContraintsForGoals' for every
	    // goal. Then, after we have iterated over the goals, we can update nu2.
	    BF nextContraintsForGoals = mgr.constantTrue();
	    for (unsigned int a=0;a<livenessGuarantees.size();a++) {
	      
		// This evaluates the first term in the conjunction of the FP expression. Ziterate are all candidate
		// transitions, i.e., the ones leading to Z (i.e., the post state (primed part) is in Z) and fulfill the currently
		// chased liveness guarantee (on the pre-state) and are allowed by the system transitions.  
		BF Ziterate = livenessGuarantees[a] & (nu2.getValue().SwapVariables(varVectorPre,varVectorPost)) & safetySys;
		// coxZ computes the controllable predicessor (by running (safetyEnv.Implies(Ziterate)) and gets rid of all 
		// particular post state assignment, and therefore essentially only returing a state (of non-primed variables)
		// rather then a transition
		BF coxZ = ((safetyEnv.Implies(Ziterate)).ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                // coxZ &= safetyEnv; 	// only used for debugging, to make the printed sets for readable, as coxZ also includes
					// non-reachable states. Is not necessary for the strategy extraction as the latter makes 
					// sure that only reachable states are evaluated.
		
		
		//DEBUGING
		    // if (kh==0 && a==0 ) {
		      // outputStream << "Ziterate, kh=" << kh << ", a=" << a << "\n";
		      // PrintSet(Ziterate,true,true);
		      // outputStream << "coxZ, kh=" << kh << ", a=" << a << "\n";
		      // PrintSet(coxZ,true,false);                
		    // }
                 

		// (3) The least-fixed point - called '^aY' in the paper
		BFFixedPoint mu1(mgr.constantFalse()); // initializing ^aY to the empty set
		
//                 unsigned int k=0; // only used during debugging, counting the iteration over Y in the current iteration over Z
		
		// To extract the ranking and the strategy we will safe all sets ^aY^k encounterd in the last iteration over Z in the subvector I
		// As we do not know what the last iteration is, we safe these sets in every iteration over Z and clear the data here.
		subvectorI.clear();
		subvectorIs.clear();
		
		// this starts the iteration over ^aY    
		for (;!mu1.isFixedPointReached();) {

		    // This evaluates the second term in the conjunction of the FP expression. Yiterate and coxY are interpreted analogously to Z
		    BF Yiterate = (mu1.getValue().SwapVariables(varVectorPre,varVectorPost)) & safetySys;
		    BF coxY = ((safetyEnv.Implies(Yiterate)).ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                    // coxY &= safetyEnv; // only used during debugging
		    
		    //DEBUGING
		      // if (kh==0 && a==0 ) {
			// outputStream << "Yiterate, kh=" << kh << ", a=" << a << ", k=" << k<< "\n";
			// PrintSet(Yiterate,true,true);
			// outputStream << "coxY, kh=" << kh << ", a=" << a << ", k=" << k<< "\n";
			// PrintSet(coxY,true,false);                
		      // }

                    
		    // (4) Iterate over the liveness assumptions. 
		    // Store the positions that are found to be winning for *any*
		    // of them into the variable 'goodForAnyLivenessAssumption'.
		    BF goodForAnyLivenessAssumption = mu1.getValue();	
		    
		    // To extract the ranking and the strategy we will safe the converged sets ab^X encounterd in the last iteration over Z in the subvector I
		    // As we do not know what the last iteration is, we safe these sets in every iteration over Z and clear the data here.
		    // It is important for the strategy extension to work to pre-define the size of subvector B as not every b might allow for a set of winning states. If we just use push_back, the line of subvectorB might not coincide with the index b and a wrong strategy might be extracted
		    std::vector<std::vector<BF>> subvectorB(livenessAssumptions.size());
		    std::vector<std::vector<BF>> subvectorBs(livenessAssumptions.size());

		    for (unsigned int b=0;b<livenessAssumptions.size();b++) {
		        
                       // if we use the Heuristic, we should only evaluate the two innermost fixed points if a==b.
                       // Otherwise we evaluate every combination of a and b.
                        if ((b==a) | ((b!=a) & !(UseHeuristic))) {

			// (5) The greatest fixed point - called '^abX' in the paper
			BFFixedPoint nu0(mgr.constantTrue()); // initializing ^abX to the full state set
			
// 		        unsigned int lh=0; //only used for debugging
			
			// this starts the iteration over ^bX    			
			for (;!nu0.isFixedPointReached();) {			    
				
			    // (6) The least-fixed point - called '^abW' in the paper
			    BFFixedPoint mu0(mgr.constantFalse());  // initializing ^abW to the empty state set			  
			  
//                             unsigned int l=0; //only used for debugging
			    
			    // To extract the ranking and the strategy we will safe all sets ^abX^l encounterd in the last iteration over ^abX 
			    // within the last iteration over Z in the subvector J.
		            // As we do not know what the last iteration is, we safe these sets in every iteration and clear the data here.
			    subvectorJ.clear(); 
			    subvectorJs.clear(); 

			    // this starts the iteration over ^abW
			    for (;!mu0.isFixedPointReached();) {
				
			        // We split the evaluation of the CondPre overator in the last term of the fixed-point into a controllable predecessor over W and the extential predecessor over WuXoA, as defined below.
				BF W=mu0.getValue();
				BF X=nu0.getValue();
				BF WuXoA= W | (X & !(livenessAssumptions[b]));
				
				// With these state set definitions, the interpretation of Xiterate and coxX is analogous to Z,Y
				BF Xiterate = (WuXoA.SwapVariables(varVectorPre,varVectorPost)) & safetySys & !(livenessAssumptions[b]);
				BF coxX = ((safetyEnv.Implies(Xiterate)).ExistAbstract(varCubePostOutput)).UnivAbstract(varCubePostInput);
                                // coxX &= safetyEnv; // only used for debugging
				
			        // Here again the interpretation of Witerate is analogous
				BF Witerate = (W.SwapVariables(varVectorPre,varVectorPost)) & safetySys & !(livenessAssumptions[b]);
				// As we are now computing the existential predecessor for W we have to substitute '(safetyEnv.Implies(Xiterate)' doing 'for all environment moves there exists a system move s.t. the resulting trace is within Witerate' by '(safetyEnv & Witerate.ExistAbstract(varCubePostOutput))' doing 'there exists an environment and a system move s.t. the resulting trace is within Witerate'. The second part is again just removing particular post-state assignments to get essentially a set of (pre)states and not a transition.
				BF exPreW = ((safetyEnv & Witerate.ExistAbstract(varCubePostOutput)).ExistAbstract(varCubePostOutput)).ExistAbstract(varCubePostInput);

				
				//DEBUGING
 				    // if (kh==0 && a==0  && k==0 && lh==0 && b==0) {
				      // outputStream << "Xiterate, kh=" << kh << ", a=" << a << ", k=" << k << ", lh=" << lh << ", b=" << b << ", l="  << l << "\n";
				      // PrintSet(Xiterate,true,true);
				      // outputStream << "coxX, kh=" << kh << ", a=" << a << ", k=" << k << ", lh=" << lh << ", b=" << b << ", l=" << l << "\n";
				      // PrintSet(coxX,true,false); 
				      // outputStream << "Witerate, kh=" << kh << ", a=" << a << ", k=" << k << ", lh=" << lh << ", b=" << b << ", l=" << l << "\n";
				      // PrintSet(Witerate,true,true);
				      // outputStream << "exPreW, kh=" << kh << ", a=" << a << ", k=" << k << ", lh=" << lh << ", b=" << b << ", l=" << l << "\n";
				      // PrintSet(exPreW,true,false);
				    //}
				
			        // Combining all four iteration variables Z,Y,X and W as described by the fixed-point results in the state set (of ^abW^(l+1))
				// computed in the current interation and needs to be safed to extract the ranking for the strategy extraction.   
				BF foundStates = coxZ | coxY | (coxX & exPreW);
				mu0.update(foundStates); // updating the fixed point over ^abW
				subvectorJs.push_back(foundStates); // saving ^abW for later use	    
				
				// To avoid re-computing possible transitions for the strategy extraction we also need to safe all candidate transitions, given by
				// foundPath. The computation of foundPath is more involved as me need to make sure that whenever the system can force progress (using an existentially quantified transition in Witerate) it needs to use this one, instead of only keeping the system within ^abX (i.e., using a transition in Xiterate). All transitions for which the latter is true, are captured by newTransitionsW. However, if the environment moves s.t.\ the latter is not true, we need to take a transition form Xitertate, which is captured by newTransitionsX. Finally, we have to combine those path which the ones found by iterating over Y and Z.
				BF newTransitionsW = ((coxX & exPreW) & Witerate);
				BF newTransitionsX = ((coxX & exPreW) & Xiterate) & !(newTransitionsW.ExistAbstract(varCubePostOutput));
				BF foundPaths = (Ziterate) | (Yiterate) | (newTransitionsW | newTransitionsX);
				subvectorJ.push_back(foundPaths);
				
				//DEBUGING
				    // if (kh==0 && a==0 && k==0 && lh==0 && b==0) {
				      // outputStream << "foundPaths, kh=" << kh << ", a=" << a << ", k=" << k << ", lh=" << lh << ", b=" << b << ", l=" << l << "\n";
				      // PrintSet(foundPaths,true,true);
				      // outputStream << "foundStates, kh=" << kh << ", a=" << a << ", k=" << k << ", lh=" << lh << ", b=" << b << ", l=" << l << "\n";
				      // PrintSet(foundStates,true,false); 
				    // }
				
//                                  l++;// only used for debugging
				}
				
		            // after the fixed-point over ^abW has terminated, we update ^abX with this value
			    nu0.update(mu0.getValue());
			    
//                             lh++;// only used for debugging
			}
			

			// Update the set of positions that are winning for some liveness assumption
			goodForAnyLivenessAssumption |= nu0.getValue();
                        
			// After ^abX has terminated as well, safe the sets ^abW^l stored line-by-line in subvectorJs and the corresponding permissible transitions in the corresponding line of subvectorBs and subvectorB, respectively.
			subvectorB[b]=subvectorJ; 
			subvectorBs[b]=subvectorJs;   

		  
		    }
                    }
		    
		    // after all b's have been investigated, we update ^aY with their disjunction stored in goodForAnyLivenessAssumption
		    mu1.update(goodForAnyLivenessAssumption);
		    
		    // as this concludes the k'th iteration over Y, we safe the obtained states and transitions to subvectorIs and subvector I, respectively.
		    subvectorI.push_back(subvectorB);
		    subvectorIs.push_back(subvectorBs);
		    
//                     k++;   // only used for debugging
		    
		}

		// Update the set of positions that are winning for any goal for the outermost fixed point
		nextContraintsForGoals &= mu1.getValue();
		
		// as the iteration of ^aY has terminated, we are now saving the obtained results to the a'th line of strategyDumping*. Note that the synthesis is only successful if all these sets are non-empty. Therefore, we do not have to take the same care as for storing b's
		strategyDumpingDataFourFP.push_back(subvectorI);  
		strategyDumpingStatesFourFP.push_back(subvectorIs);   
		
	    }

	    // after all a's have been investigated, we update Z with their conjunction stored in nextContraintsForGoals
	    nu2.update(nextContraintsForGoals);
            
// 	    kh++; // only used for debugging
	}

	// We found the set of winning positions
	winningPositions = nu2.getValue();
 
}

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XEnvironmentFriendlySynthesis<T,UseHeuristic>(filenames);
    }
        
};

#endif

