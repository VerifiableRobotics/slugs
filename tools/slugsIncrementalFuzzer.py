#!/usr/bin/python
#
# Does FUZZ-Tests on slugs, i.e., checks whether a specification is realizable or not, and checks the implementation in case
# of a positive answer
#
# May not work on windows (because of using a timeout to run NuSMV due to os.system() return code formatting)

import random
import os
import sys
import subprocess, md5, time

# =====================================================
# Difficulty Options
# =====================================================
nofAPsPerType = 3
probabilityPerProperty = 0.9

# =====================================================
# Execution Options
# =====================================================
slugsMonolithic = "/tmp/fuzz_"+str(os.getpid())+".slugsmonolithic"
slugsIncremental = "/tmp/fuzz_"+str(os.getpid())+".slugsincremental"
incrementalResultFile = "/tmp/fuzz_"+str(os.getpid())+".slugsresultincremental"
incrementalErrorFile = "/tmp/fuzz_"+str(os.getpid())+".slugserrorincremental"
monolithicResultFile = "/tmp/fuzz_"+str(os.getpid())+".slugsresultmonolithic"
monolithicErrorFile = "/tmp/fuzz_"+str(os.getpid())+".slugserrormonolithic"
slugsExecutableAndBasicOptions = "../src/slugs" + " ".join(sys.argv[1:])

# =====================================================
# =====================================================
# Main function for doing one fuzz test.
# =====================================================
# =====================================================
def fuzzOnce():

    # Recursive specification generator
    def makeAProperty(apsUsableWithoutNext,apsUsableWithNext):
        ops = ["!","next","&","|","AP","AP","TRUE"]
        op = random.choice(ops)
        while ((op=="next") and (len(apsUsableWithNext)==0)):
            op = random.choice(ops)

        if (op=="!"):
            return "! "+makeAProperty(apsUsableWithoutNext,apsUsableWithNext)
        elif (op=="next"):
            return makeAProperty(apsUsableWithNext,[])
        elif (op=="&"):
            return "& "+makeAProperty(apsUsableWithoutNext,apsUsableWithNext)+" "+makeAProperty(apsUsableWithoutNext,apsUsableWithNext)
        elif (op=="|"):
            return "| "+makeAProperty(apsUsableWithoutNext,apsUsableWithNext)+" "+makeAProperty(apsUsableWithoutNext,apsUsableWithNext)
        elif (op=="AP"):
            if len(apsUsableWithoutNext)>0:
                return random.choice(apsUsableWithoutNext)

        # Nothing else? Then it shall be "TRUE"
        return "1"


    # Create AP lists
    apsInput = ["in"+str(i) for i in xrange(0,nofAPsPerType)]
    apsOutput = ["out"+str(i) for i in xrange(0,nofAPsPerType)]
    apsInputPost = [a + "'" for a in apsInput]
    apsOutputPost = [a + "'" for a in apsOutput]

    # Init assumptions
    properties = []    
    while random.random() < probabilityPerProperty:
        properties.append(("ASA","[ENV_TRANS]",makeAProperty(apsInput+apsOutput,apsInputPost)))
    while random.random() < probabilityPerProperty:
        properties.append(("ALA","[ENV_LIVENESS]",makeAProperty(apsInput+apsOutput,apsInputPost)))
    while random.random() < probabilityPerProperty:
        properties.append(("AIA","[ENV_INIT]",makeAProperty(apsInput,[])))
    while random.random() < probabilityPerProperty:
        properties.append(("ASG","[SYS_TRANS]",makeAProperty(apsInput+apsOutput,apsInputPost+apsOutputPost)))
    while random.random() < probabilityPerProperty:
        properties.append(("ALG","[SYS_LIVENESS]",makeAProperty(apsInput+apsOutput,apsInputPost+apsOutputPost)))
    while random.random() < probabilityPerProperty:
        properties.append(("AIG","[SYS_INIT]",makeAProperty(apsInput+apsOutput,[])))

    random.shuffle(properties)

    # Write incremental synthesis benchmark
    out = open(slugsIncremental,"w")
    for a in apsInput:
        out.write("INPUT "+a+"\n")
    for a in apsOutput:
        out.write("OUTPUT "+a+"\n")
    for i,(a,b,c) in enumerate(properties):
        out.write(a+" PROP"+str(i)+" "+c+"\nCR\n")
    out.close()

    # =====================================================
    # Run incremental synthesis
    # =====================================================
    retValue = os.system(slugsExecutableAndBasicOptions + " --experimentalIncrementalSynthesis < "+slugsIncremental+" > "+incrementalResultFile+" 2> "+incrementalErrorFile)
    if (retValue!=0):
        print >>sys.stderr, "Slugs (incrememental) failed!"
        raise Exception("Fuzzing Aborted")

    # =====================================================
    # Run monolithic synthesis
    # =====================================================
    slugsIncrementalResultsFile = open(incrementalResultFile,"r")

    for maxProperty in xrange(1,len(properties)):
        out = open(slugsMonolithic,"w")

        out.write("[INPUT]\n")
        for a in apsInput:
            out.write(a+"\n")
        out.write("[OUTPUT]\n")
        for a in apsOutput:
            out.write(a+"\n")
        for (a,b,c) in properties[0:maxProperty]:
            out.write(b+"\n"+c+"\n")
        out.close()

        retValue = os.system(slugsExecutableAndBasicOptions + " --onlyRealizability "+slugsMonolithic+" > "+monolithicResultFile+" 2> "+monolithicErrorFile)
        if (retValue!=0):
            print >>sys.stderr, "Slugs (monolithic) failed!"
            raise Exception("Fuzzing Aborted")

        # Read the result (monolithic)
        monolithicResult = None
        slugsReturn = open(monolithicErrorFile,"r")
        for line in slugsReturn.readlines():
            if line.strip() == "RESULT: Specification is unrealizable.":
                monolithicResult = False
            elif line.strip() == "RESULT: Specification is realizable.":
                monolithicResult = True
        slugsReturn.close()

        if (monolithicResult==None):
            print >>sys.stderr, "Error: Slugs (monolithic) did not return some result"
            raise Exception("Fuzzing Aborted")

        # Read the result (monolithic)        
        incrementalResult = None
        while incrementalResult==None:
            line = slugsIncrementalResultsFile.readline()
            if line.strip() == "RESULT: Specification is unrealizable.":
                incrementalResult = False
            elif line.strip() == "RESULT: Specification is realizable.":
                incrementalResult = True
            if line=="":
                print >>sys.stderr, "No more incremental synthesis result found!"
                raise Exception("Fuzzing Aborted")

        if incrementalResult!=monolithicResult:
            print >>sys.stderr, "Error: Found a different result after "+str(maxProperty)+" property lines!"
            raise Exception("Fuzzing Aborted")
        
    slugsIncrementalResultsFile.close()


# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv)>1:
        seed = sys.argv[1]
    else:
        seed = md5.md5(time.ctime()+str(os.getpid())).digest().encode("hex")
    print "Fuzzing Slugs. Process ID: "+str(os.getpid())+", Seed: "+seed
    random.seed(seed)

    # Start fuzzing    
    while True:
        fuzzOnce()
        sys.stdout.write(".")
        sys.stdout.flush()
