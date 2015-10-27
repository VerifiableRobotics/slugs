#!/usr/bin/python
#
# A tool that computes the pareto curve for realizability of a specification with multiple parameters.
#
# In order to use it, a wrapper script for calling slugs needs to be used that accepts all parameters.

import os
import sys
import subprocess, itertools
import time

def checkRealizability(scriptName,parameters):

    '''
    This functions calls a script and looks out for lines stating realizability/unrealizability by Slugs.
    It returns an error message if the subprocess returned with a non-zero errorcode or if multiple results are found.
    Otherwise, the result is True or False.    
    '''
    slugsProcess = subprocess.Popen(scriptName+" "+" ".join([str(p) for p in parameters]), shell=True, bufsize=1048000, stdout=subprocess.PIPE,stderr=subprocess.STDOUT)

    nofRealizableLines = 0
    nofUnrealizableLines = 0
    lastLines = []

    for line in slugsProcess.stdout.readlines():
        line = line.strip()
        if len(lastLines)>100:
            lastLines = lastLines[1:]+[line]
        else:
            lastLines.append(line)
        if line=="RESULT: Specification is realizable.":
            nofRealizableLines +=1 
        elif line=="RESULT: Specification is unrealizable.":
            nofUnrealizableLines +=1 
    
    errorCode = slugsProcess.wait()
    if (errorCode!=0):
        errorMessage = "External tool or script terminated with a non-zero error code: "+str(errorCode)+"\n"+"\n".join(lastLines)
        return errorMessage
    if (nofRealizableLines+nofUnrealizableLines)>1:
        return "Found more than one realizability/unrealizability result!"
    if (nofRealizableLines+nofUnrealizableLines)==0:
        return "Found no realizability/unrealizability result!"
    if nofRealizableLines>0:
        return True
    elif nofUnrealizableLines>0:
        return False
    else:
        raise Exception("Internal error. Should not be able to happen.")
        
        
def tupleComparator(t1,t2):
    canBeSmaller = True
    canBeLarger = True
    for i in xrange(0,len(t1)):
        if t1[i]<t2[i]:
            canBeLarger = False
        elif t1[i]>t2[i]:
            canBeSmaller = False
    if canBeSmaller:
        return True
    if canBeLarger:
        return False
    return None

def runSearch(scriptName, oldParameterTypes):
    '''
    The main search procedure
    '''
    negativePoints = []
    positivePoints = []
    
    # Normalize the parameter types
    parameterTypes = []
    for (mini,maxi,direction) in oldParameterTypes:
        assert mini>=0
        assert maxi>=0
        if not direction:
            parameterTypes.append((-1*maxi,-1*mini))
        else:
            parameterTypes.append((mini,maxi))
    
    while True:
        # Find a new query that would reduce uncertainty in a maximal possible way
        allTuples = itertools.product(*[xrange(mini,maxi+1) for (mini,maxi) in parameterTypes])
        bestTuple = None
        bestTupleValue = -1
        bestTupleDistrib = None
        # Test for each tuple how many new tuples it would decide at least
        for tupleA in allTuples:
            distPos = 0
            distNeg = 0
            
            # Check if already covered.
            outerCover = False
            for tupleC in negativePoints:
                if tupleComparator(tupleA,tupleC)==True:
                    outerCover = True
            if not outerCover:
                for tupleC in positivePoints:
                    if tupleComparator(tupleC,tupleA)==True:
                        outerCover = True
            
            # ...otherwise: check how much we gain by evaluating....
            if not outerCover:
                # -> Positive
                for tupleB in itertools.product(*[xrange(tupleA[i],parameterTypes[i][1]+1) for i in xrange(0,len(parameterTypes))]):
                    covered = False
                    for tupleC in positivePoints:
                        if tupleComparator(tupleC,tupleB)==True:
                            covered = True
                    if not covered:
                        distNeg+=1

                # -> Negative
                for tupleB in itertools.product(*[xrange(parameterTypes[i][0],tupleA[i]+1) for i in xrange(0,len(parameterTypes))]):
                    covered = False                    
                    for tupleC in negativePoints:
                        if tupleComparator(tupleB,tupleC)==True:
                            covered = True
                    if not covered:
                        distPos+=1
                thisValue = min(distNeg,distPos)
                if distPos>0 or distNeg>0:
                    if thisValue >= bestTupleValue:
                        bestTuple = tupleA
                        bestTupleValue = thisValue 
                        bestTupleDistrib = (distPos,distNeg)
                
        if bestTupleValue==-1:
            returnValues = []
            for a in positivePoints:
                translatedTuple = []
                for i in xrange(0,len(oldParameterTypes)):
                    if oldParameterTypes[i][2]:
                        translatedTuple.append(a[i])
                    else:
                        translatedTuple.append(-1*a[i])
                returnValues.append(translatedTuple)
            return returnValues
    
        # Test realizability
        sys.stdout.write("Testing realizability of "+str(bestTuple)+" (quality of test: "+str(bestTupleDistrib[0])+","+str(bestTupleDistrib[1])+"): ")
        sys.stdout.flush()
        
        # Translate back to monotone/antitone tuple
        translatedTuple = []
        for i in xrange(0,len(oldParameterTypes)):
            if oldParameterTypes[i][2]:
                translatedTuple.append(bestTuple[i])
            else:
                translatedTuple.append(-1*bestTuple[i])
        
        startTime = time.time()
        result = checkRealizability(scriptName,translatedTuple)
        totalTime = time.time() - startTime
        
        if result==True:
            # -> Realizable
            # Remove now redundant elements first
            positivePoints = [a for a in positivePoints if not tupleComparator(bestTuple,a)==True]
            positivePoints.append(bestTuple)
            print "Yes, after %f seconds" % totalTime
        elif result==False:
            # -> Unrealizable
            # Remove now redundant elements first
            negativePoints = [a for a in negativePoints if not tupleComparator(a,bestTuple)==True]
            negativePoints.append(bestTuple)
            print "No, after %f seconds" % totalTime
        else:
            print >>sys.stderr, "Error: Could not obtain a result from the called script."            
            
            

# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv) <= 2:
        print >>sys.stderr, "Error: Expected a script name and a list of parameters to the script. For each, the minimal values, maximum values and whether realizability, when seen as a function to {0,1} is MONOTONE or ANTITONE, must be stated. All numbers need to be integer values."
        print >>sys.stderr, "An example of a correct call is: ./multiDimensionalRealizabilitySearch.py ./evalScript.py 1 10 MONOTONE 2 9 ANTITONE"
        sys.exit(1)

    scriptName = sys.argv[1]
    parameterTypes = []
    if len(sys.argv) % 3 != 2:
        print >>sys.stderr, "Error: For every parameter of the model, minimal and maximal values and a MONOTONE/ANTITONE declaration needs to be given"
        sys.exit(1)

    paramTypes = []
    for i in xrange(0,len(sys.argv)/3):
        minValue = sys.argv[i*3+2]
        maxValue = sys.argv[i*3+3]
        monotonicity = sys.argv[i*3+4]
    
        if (monotonicity).strip().upper()=="MONOTONE":
            monotone = True
        elif (monotonicity).strip().upper()=="ANTITONE":
            monotone = False
        else:
            print >>sys.stderr, "Error: The monotonicity parameters must be MONOTONE or ANTITONE."
        try:
            minValue = int(minValue)
            maxValue = int(maxValue)
        except ValueError:
            print >>sys.stderr, "Error: The second and fourth parameters must be integer values."

        paramTypes.append((minValue,maxValue,monotone))

    # Final Test - we know that maxValue==minValue now
    result = runSearch(scriptName,paramTypes)

    print "Pareto front: ",result

