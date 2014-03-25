#!/usr/bin/python
#

import random
import os
import sys
import subprocess, md5, time

# =====================================================
# Option configuration
# =====================================================
slugsExecutableAndBasicOptions = sys.argv[0][0:sys.argv[0].rfind("tools/findMinimalUnrealizableCore.py")]+"src/slugs"
slugsReturnFile = "/tmp/check_"+str(os.getpid())+".slugsreturn"


# =====================================================
# Slugs -> AIG Encoder, used by the main function
# =====================================================
def recurseMakeAIGEncodingOfBooleanFormula(formula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines):
    
    # Cut off next token    
    posSpace = formula.find(" ")
    if posSpace==-1:
        firstToken = formula
        restOfFormula = ""
    else:
        firstToken = formula[0:posSpace]
        restOfFormula = formula[posSpace+1:]

    # Process token
    if firstToken=="1":
        return (restOfFormula,1,varNumbersUsedSoFar,nofGates)
    elif firstToken=="0":
        return (restOfFormula,0,varNumbersUsedSoFar,nofGates)
    if firstToken=="!":
        (restOfFormula,resultingGateNumber,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
        return (restOfFormula,resultingGateNumber ^ 1,varNumbersUsedSoFar,nofGates)
    elif firstToken=="|":
        (restOfFormula,resultingGateNumber,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
        (restOfFormula,resultingGateNumber2,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
        varNumbersUsedSoFar += 1
        nofGates += 1
        lines.append(str(varNumbersUsedSoFar*2)+" "+str(resultingGateNumber ^ 1)+" "+str(resultingGateNumber2 ^ 1))
        return (restOfFormula,varNumbersUsedSoFar*2 ^ 1,varNumbersUsedSoFar,nofGates)
    elif firstToken=="^":
        (restOfFormula,resultingGateNumber,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
        (restOfFormula,resultingGateNumber2,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
        lines.append(str(varNumbersUsedSoFar*2+2)+" "+str(resultingGateNumber ^ 1)+" "+str(resultingGateNumber2 ^ 1))
        lines.append(str(varNumbersUsedSoFar*2+4)+" "+str(resultingGateNumber)+" "+str(resultingGateNumber2))
        lines.append(str(varNumbersUsedSoFar*2+6)+" "+str(varNumbersUsedSoFar*2+3)+" "+str(varNumbersUsedSoFar*2+5))
        varNumbersUsedSoFar += 3
        nofGates += 3
        return (restOfFormula,varNumbersUsedSoFar*2,varNumbersUsedSoFar,nofGates)
    elif firstToken=="&":
        (restOfFormula,resultingGateNumber,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
        (restOfFormula,resultingGateNumber2,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
        varNumbersUsedSoFar += 1
        nofGates += 1
        lines.append(str(varNumbersUsedSoFar*2)+" "+str(resultingGateNumber)+" "+str(resultingGateNumber2))
        return (restOfFormula,varNumbersUsedSoFar*2,varNumbersUsedSoFar,nofGates)
    elif firstToken=="$":
        posSpace = restOfFormula.find(" ")
        nofParts = int(restOfFormula[0:posSpace])
        restOfFormula = restOfFormula[posSpace+1:]
        memoryTable = []
        for i in xrange(0,nofParts):
            (restOfFormula,resultingGate,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(restOfFormula,variables,varNumbersUsedSoFar,nofGates,memoryTable,lines)
            memoryTable.append(resultingGate)
        return (restOfFormula,memoryTable[len(memoryTable)-1],varNumbersUsedSoFar,nofGates)
    elif firstToken=="?":
        posSpace = restOfFormula.find(" ")
        if posSpace>-1:
            part = int(restOfFormula[0:posSpace])
            restOfFormula = restOfFormula[posSpace+1:]
        else:
            part = int(restOfFormula)
            restOfFormula = ""
        return (restOfFormula,memoryTable[part],varNumbersUsedSoFar,nofGates)
    else:
        # Again, an AP
        return (restOfFormula,variables[firstToken],varNumbersUsedSoFar,nofGates)



# =====================================================
# Main function
# =====================================================
def produceAIGFileFromSlugsFile(filename):

    slugsFile = open(filename,"r")
    slugsLines = slugsFile.readlines()
    slugsFile.close()
    
    # Read the categories
    categories = {"[INPUT]":[],"[OUTPUT]":[],"[ENV_INIT]":[],"[ENV_LIVENESS]":[],"[ENV_TRANS]":[],"[SYS_INIT]":[],"[SYS_LIVENESS]":[],"[SYS_TRANS]":[]}
    currentCategory = ""
    lastCR = False
    propNr = 1
    for line in slugsLines:
        line = line.strip()
        if line.startswith("#"):
            pass
        elif line.startswith("["):
            currentCategory = line
        elif len(line)>0:
            categories[currentCategory].append(line)

    # Check the absense of liveness conditions.
    if len(categories["[ENV_LIVENESS]"])>0:
        raise Exception("Cannot translate specifications that comprise liveness assumptions")
    if len(categories["[SYS_LIVENESS]"])>0:
        raise Exception("Cannot translate specifications that comprise liveness guarantees")

    # Fill up condition lists with "1" whenever needed - w.l.o.g, we assume to have at least one property of every supported type
    for cat in ["[ENV_INIT]","[SYS_INIT]","[ENV_TRANS]","[SYS_TRANS]"]:
        if len(categories[cat])==0:
            categories[cat].append("1")

    # Have a counter for the last variable. Initialize to account for the "checking gates"
    # Also have a list of AIG file lines so far and reserve some numbers for GR(1)-spec
    # semantics encoding
    varNumbersUsedSoFar = 1
    nofGates = 0
    lines = []

    # Put one special latch at the beginning
    latchOutputIsNotFirstRound = varNumbersUsedSoFar*2+2 
    varNumbersUsedSoFar += 1


    # Assign variable numbers to input and output variables
    variables = {}
    for cat in ["[INPUT]","[OUTPUT]"]:
        for x in categories[cat]:
            varNumbersUsedSoFar += 1
            variables[x] = varNumbersUsedSoFar*2
            lines.append(str(varNumbersUsedSoFar*2))

    # Put some special latches later
    latchOutputIsAssumptionsAlreadyViolated = varNumbersUsedSoFar*2+2
    gateNumberAssumptionFailure = varNumbersUsedSoFar*2+4
    gateNumberGuaranteeFailure = varNumbersUsedSoFar*2+6
    varNumbersUsedSoFar += 3

    # Declare one "GR(1) semantics" latch here.
    lines.append(str(latchOutputIsNotFirstRound)+" 1")

    # Prepare latches for the last Input/Output. Also register the latches in the "varibles" dictionary so
    # that they can be used in "recurseMakeAIGEncodingOfBooleanFormula"
    oldValueLatches = {}
    # Also create variable array with pre- and post-bits
    variablesInTrans = {}
    for cat in ["[INPUT]","[OUTPUT]"]:
        for x in categories[cat]:
            varNumbersUsedSoFar += 1
            oldValueLatches[x] = varNumbersUsedSoFar*2
            variablesInTrans[x] = varNumbersUsedSoFar*2
            variablesInTrans[x+"'"] = variables[x]
            lines.append(str(varNumbersUsedSoFar*2)+" "+str(variables[x]))

    # Put one special latch definition here
    lines.append(str(latchOutputIsAssumptionsAlreadyViolated)+" "+str(gateNumberAssumptionFailure ^ 1))

    # Now the output lines
    lines.append(str(gateNumberGuaranteeFailure))

    # Compute the safety init assumptions
    initAssuptions = "& "*(len(categories["[ENV_INIT]"])-1)+" ".join(categories["[ENV_INIT]"])
    (restOfFormula,resultingGateNumberEnvInit,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(initAssuptions,variables,varNumbersUsedSoFar,nofGates,[],lines)
    assert len(restOfFormula)==0

    # Compute the safety trans assumptions
    transAssuptions = "& "*(len(categories["[ENV_TRANS]"])-1)+" ".join(categories["[ENV_TRANS]"])
    (restOfFormula,resultingGateNumberEnvTrans,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(transAssuptions,variablesInTrans,varNumbersUsedSoFar,nofGates,[],lines)
    assert len(restOfFormula)==0

    # Update assumption violations - note that "gateNumberAssumptionFailure" should get the negation
    lines.append(str(gateNumberAssumptionFailure)+" "+str(latchOutputIsAssumptionsAlreadyViolated ^ 1)+" "+str(varNumbersUsedSoFar*2+2))
    lines.append(str(varNumbersUsedSoFar*2+2)+" "+str(varNumbersUsedSoFar*2+5)+" "+str(varNumbersUsedSoFar*2+7))
    lines.append(str(varNumbersUsedSoFar*2+4)+" "+str(resultingGateNumberEnvInit ^ 1)+" "+str(latchOutputIsNotFirstRound ^ 1))
    lines.append(str(varNumbersUsedSoFar*2+6)+" "+str(resultingGateNumberEnvTrans ^ 1)+" "+str(latchOutputIsNotFirstRound))
    nofGates += 4
    varNumbersUsedSoFar += 3

    # Compute the safety init guarantees
    initGuarantees = "& "*(len(categories["[SYS_INIT]"])-1)+" ".join(categories["[SYS_INIT]"])
    (restOfFormula,resultingGateNumberSysInit,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(initGuarantees,variables,varNumbersUsedSoFar,nofGates,[],lines)
    assert len(restOfFormula)==0

    # Compute the safety trans guarantees
    transGuarantees = "& "*(len(categories["[SYS_TRANS]"])-1)+" ".join(categories["[SYS_TRANS]"])
    (restOfFormula,resultingGateNumberSysTrans,varNumbersUsedSoFar,nofGates) = recurseMakeAIGEncodingOfBooleanFormula(transGuarantees,variablesInTrans,varNumbersUsedSoFar,nofGates,[],lines)
    assert len(restOfFormula)==0

    # Detect guarantee violations - exclude cases in which an assumption has already been violated
    lines.append(str(gateNumberGuaranteeFailure)+" "+str(gateNumberAssumptionFailure)+" "+str(varNumbersUsedSoFar*2+3))
    lines.append(str(varNumbersUsedSoFar*2+2)+" "+str(varNumbersUsedSoFar*2+5)+" "+str(varNumbersUsedSoFar*2+7))
    lines.append(str(varNumbersUsedSoFar*2+4)+" "+str(resultingGateNumberSysInit ^ 1)+" "+str(latchOutputIsNotFirstRound ^ 1))
    lines.append(str(varNumbersUsedSoFar*2+6)+" "+str(resultingGateNumberSysTrans ^ 1)+" "+str(latchOutputIsNotFirstRound))
    nofGates += 4
    varNumbersUsedSoFar += 3

    # Print header and payload
    print "aag",str(varNumbersUsedSoFar*2),str(len(variables)),str(len(variables)+2),"1",str(nofGates)
    for line in lines:
        print line

    # Print symbols
    inNumberSoFar = 0
    for cat,prefix in [("[INPUT]",""),("[OUTPUT]","controllable_")]:
        for x in categories[cat]:
            print "i"+str(inNumberSoFar)+" "+prefix+x
            inNumberSoFar += 1
    print "l0 IsNotFirstRound"
    inNumberSoFar = 1
    for cat in ["[INPUT]","[OUTPUT]"]:
        for x in categories[cat]:
            print "l"+str(inNumberSoFar)+" prev_"+x
            inNumberSoFar += 1
    print "l"+str(inNumberSoFar)+" AssumptionsAlreadyViolated"
    print "o0 err"
    print "c"
    print "This input file has been automatically translated by 'translateSafetySlugsSpecToSyntCompAIGFormat.py' from the following input file:"
    for line in slugsLines:
        if len(line.strip())>0:
            sys.stdout.write(line)
    print ""
    

# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv)>1:
        slugsFile = sys.argv[1]
    else:
        print >>sys.stderr,"Error: Expected slugs file name as input."
        sys.exit(1)

    produceAIGFileFromSlugsFile(slugsFile)

