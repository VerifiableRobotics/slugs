#!/usr/bin/python
#
# A tool to incrementally call a script with a parameter which in the end calls slugs
# The tool reports the least parameter such that the result is realizable 
# (and makes sure that the script is called last with this parameter).

import os
import sys
import subprocess, md5, time



def checkRealizability(scriptName,parameter):
    '''
    This functions calls a script and looks out for lines stating realizability/unrealizability by Slugs.
    It returns an error message if the subprocess returned with a non-zero errorcode or if multiple results are found.
    Otherwise, the result is True or False.    
    '''
    slugsProcess = subprocess.Popen(scriptName+" "+str(parameter), shell=True, bufsize=1048000, stdout=subprocess.PIPE,stderr=subprocess.STDOUT)

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
        


# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv)!=5:
        print >>sys.stderr, "Error: Expected precisely four parameters: The script name, the minimum value, the maximum value, and whether realizability, when seen as a function to {0,1} is MONOTONE or ANTITONE. The middle two paramters need to be integer numbers"
        sys.exit(1)

    scriptName = sys.argv[1]
    minValue = sys.argv[2]
    maxValue = sys.argv[3]
    if (sys.argv[4]).strip().upper()=="MONOTONE":
        monotone = True
    elif (sys.argv[4]).strip().upper()=="ANTITONE":
        monotone = False
    else:
        print >>sys.stderr, "Error: The fourth parameter must be MONOTONE or ANTITONE."
    
    try:
        minValue = int(minValue)
        maxValue = int(maxValue)
    except ValueError:
        print >>sys.stderr, "Error: The second and fourth parameters must be integer values."

    testList = []

    while (minValue<maxValue):
        testValue = (minValue+maxValue) >> 1
        # Always round such that unrealizability is more common
        if (not monotone) and ((minValue+maxValue) % 2)>0:
            testValue += 1
        result = checkRealizability(scriptName,testValue)

        if result==True:
            if monotone:
                maxValue = testValue
            else:
                minValue = testValue
            testList.append(str(testValue)+"+")
        elif result==False:
            if monotone:
                minValue = testValue+1
            else:
                maxValue = testValue-1
            testList.append(str(testValue)+"-")
        else:
            print >>sys.stderr, "Error:\n"+result
            sys.exit(1)

    # Final Test - we know that maxValue==minValue now
    result = checkRealizability(scriptName,maxValue)
    if result==True:
        if monotone:
            print "The minimum value for realizability is "+str(maxValue)
        else:
            print "The maximum value for realizability is "+str(maxValue)
        print "\nTests performed: "+" ".join(testList)
    elif result==False:
        print "Error: The specification seems to be totally unrealizable!"
    else:
        print >>sys.stderr, "Error:\n"+result
        sys.exit(1)


